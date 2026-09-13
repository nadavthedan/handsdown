#include "display.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

typedef struct {
  DisplayContext *ctx;
  struct wl_output *wl_output;
  Monitor temp_mon;
} LocalOutput;

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                                   int32_t x, int32_t y, int32_t physical_width,
                                   int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t transform) {
  LocalOutput *local = (LocalOutput *)data;
  local->temp_mon.x = x;
  local->temp_mon.y = y;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
                               uint32_t flags, int32_t width, int32_t height,
                               int32_t refresh) {
  LocalOutput *local = (LocalOutput *)data;
  if (flags & WL_OUTPUT_MODE_CURRENT) {
    local->temp_mon.width = width;
    local->temp_mon.height = height;
    local->temp_mon.refresh = refresh;
  }
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                                int32_t factor) {
  LocalOutput *local = (LocalOutput *)data;
  local->temp_mon.scale = factor;
}

static void output_handle_name(void *data, struct wl_output *wl_output,
                               const char *name) {
  LocalOutput *local = (LocalOutput *)data;
  snprintf(local->temp_mon.name, sizeof(local->temp_mon.name), "%s", name);
}

static void output_handle_description(void *data, struct wl_output *wl_output,
                                      const char *description) {
  // placeholder
}

static void output_handle_done(void *data, struct wl_output *wl_output) {
  LocalOutput *local = (LocalOutput *)data;
  DisplayContext *ctx = local->ctx;

  pthread_mutex_lock(&ctx->lock);

  bool found = false;
  for (int i = 0; i < ctx->count; i++) {
    if (ctx->monitors[i].global_id == local->temp_mon.global_id) {
      ctx->monitors[i] = local->temp_mon;
      found = true;
      break;
    }
  }
  if (!found && ctx->count < MAX_MONITORS) {
    ctx->monitors[ctx->count++] = local->temp_mon;
  }

  pthread_mutex_unlock(&ctx->lock);
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
  DisplayContext *ctx = (DisplayContext *)data;

  if (strcmp(interface, wl_output_interface.name) == 0) {
    LocalOutput *local = calloc(1, sizeof(LocalOutput));
    local->ctx = ctx;
    local->temp_mon.global_id = id;
    local->temp_mon.scale = 1;

    uint32_t bind_version =
        version < 4 ? version
                    : 4; // bind version 4 to support .name and .description
    local->wl_output =
        wl_registry_bind(registry, id, &wl_output_interface, bind_version);

    wl_output_add_listener(local->wl_output, &output_listener, local);
  }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t id) {
  DisplayContext *ctx = (DisplayContext *)data;

  pthread_mutex_lock(&ctx->lock);
  for (int i = 0; i < ctx->count; i++) {
    if (ctx->monitors[i].global_id == id) {
      // shift on removal
      for (int j = i; j < ctx->count - 1; j++) {
        ctx->monitors[j] = ctx->monitors[j + 1];
      }

      ctx->count--;
      break;
    }
  }
  pthread_mutex_unlock(&ctx->lock);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void *background_thread_main(void *arg) {
  DisplayContext *ctx = (DisplayContext *)arg;

  struct wl_display *display = wl_display_connect(NULL);
  if (!display)
    return NULL;

  ctx->display = display;
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, ctx);

  // Initial discovery wl_display_roundtrip
  wl_display_roundtrip(display);
  wl_display_roundtrip(display);

  int display_fd = wl_display_get_fd(display);
  struct pollfd pfd = {.fd = display_fd, .events = POLLIN};

  while (atomic_load(&ctx->running)) {
    while (wl_display_prepare_read(display) != 0) {
      wl_display_dispatch_pending(display);
    }

    wl_display_flush(display);

    // Block on poll for 100ms or until socket activity / thread stop
    if (poll(&pfd, 1, 100) > 0) {
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);
    } else {
      wl_display_cancel_read(display);
    }
  }

  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  ctx->display = NULL;
  return NULL;
}

DisplayContext *display_listener_start(void) {
  DisplayContext *ctx = calloc(1, sizeof(DisplayContext));
  if (!ctx)
    return NULL;

  pthread_mutex_init(&ctx->lock, NULL);
  atomic_store(&ctx->running, true);

  if (pthread_create(&ctx->thread_id, NULL, background_thread_main, ctx) != 0) {
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
    return NULL;
  }

  return ctx;
}

void display_listener_stop(DisplayContext *ctx) {
  if (!ctx)
    return;

  atomic_store(&ctx->running, false);
  pthread_join(ctx->thread_id, NULL);
  pthread_mutex_destroy(&ctx->lock);
  free(ctx);
}

int display_get_monitors(DisplayContext *ctx, Monitor *out_monitors,
                         int max_out) {
  if (!ctx)
    return 0;

  pthread_mutex_lock(&ctx->lock);

  int to_copy = (ctx->count < max_out) ? ctx->count : max_out;
  memcpy(out_monitors, ctx->monitors, sizeof(Monitor) * to_copy);

  pthread_mutex_unlock(&ctx->lock);
  return to_copy;
}

int get_focused_monitor_from_pointer(DisplayContext *ctx, int pointer_x,
                                     int pointer_y) {
  pthread_mutex_lock(&ctx->lock);

  int focused_index = -1;
  for (int i = 0; i < ctx->count; i++) {
    Monitor *m = &ctx->monitors[i];
    if (pointer_x >= m->x && pointer_x < (m->x + m->width) &&
        pointer_y >= m->y && pointer_y < (m->y + m->height)) {
      focused_index = i;
      break;
    }
  }

  pthread_mutex_unlock(&ctx->lock);
  return focused_index; // Index of the monitor containing the cursor
}
