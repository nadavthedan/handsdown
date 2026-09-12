#include "display.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

typedef struct {
  struct wl_output *wl_output;
  uint32_t global_id;
  int x;
  int y;
  int width_mm;
  int height_mm;
  int mode_width;
  int mode_height;
  int refresh_mhz;
  int scale;
  char name[64];
  char make_model[128];
} MonitorInfo;

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                                   int32_t x, int32_t y, int32_t physical_width,
                                   int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t transform) {
  MonitorInfo *mon = (MonitorInfo *)data;
  mon->x = x;
  mon->y = y;
  mon->width_mm = physical_width;
  mon->height_mm = physical_height;
  snprintf(mon->make_model, sizeof(mon->make_model), "%s %s", make, model);
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
                               uint32_t flags, int32_t width, int32_t height,
                               int32_t refresh) {
  MonitorInfo *mon = (MonitorInfo *)data;
  if (flags & WL_OUTPUT_MODE_CURRENT) {
    mon->mode_width = width;
    mon->mode_height = height;
    mon->refresh_mhz = refresh;
  }
}

static void output_handle_done(void *data, struct wl_output *wl_output) {
  MonitorInfo *mon = (MonitorInfo *)data;
  printf("\nMonitor Discovered:\n");
  printf("Name/Model: %s (%s)\n", mon->name[0] ? mon->name : "Unknown",
         mon->make_model);
  printf("Global Position: X=%d, Y=%d\n", mon->x, mon->y);
  printf("Resolution: %dx%d @ %.2f Hz\n", mon->mode_width, mon->mode_height,
         mon->refresh_mhz);
  printf("Scale Factor: %dx\n", mon->scale);
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                                int32_t factor) {
  MonitorInfo *mon = (MonitorInfo *)data;
  mon->scale = factor;
}

static void output_handle_name(void *data, struct wl_output *wl_output,
                               const char *name) {
  MonitorInfo *mon = (MonitorInfo *)data;
  snprintf(mon->name, sizeof(mon->name), "%s", name);
}

static void output_handle_description(void *data, struct wl_output *wl_output,
                                      const char *description) {
  // todo
}

static const struct wl_output_listener output_listener = {
    .geometry = output_handle_geometry,
    .mode = output_handle_mode,
    .done = output_handle_done,
    .scale = output_handle_scale,
    .name = output_handle_name,
    .description = output_handle_description,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t id, const char *interface,
                                   uint32_t version) {
  if (strcmp(interface, wl_output_interface.name) == 0) {
    MonitorInfo *mon = calloc(1, sizeof(MonitorInfo));
    mon->global_id = id;
    mon->scale = 1;

    uint32_t bind_version =
        version < 4 ? version
                    : 4; // bind version 4 to support .name and .description
    mon->wl_output =
        wl_registry_bind(registry, id, &wl_output_interface, bind_version);

    wl_output_add_listener(mon->wl_output, &output_listener, mon);
  }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t id) {
  // TODO: handle remove
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

int display_init(void) {
  struct wl_display *display = wl_display_connect(NULL);
  if (!display) {
    printf("Failed to connect to Wayland display server\n");
    return ECONNREFUSED;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);

  wl_display_roundtrip(display);
  wl_display_roundtrip(display);

  wl_registry_destroy(registry);
  wl_display_disconnect(display);

  return 0;
}
