#include "glib-object.h"
#include "glib.h"
#include "overlay.h"

#include <cairo.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PERCENT 100.0
#define REQUEST_TIMEOUT_MS 1000

static GtkWidget *overlay_window;
static gint overlay_ready;

struct observed_pointer {
  pthread_mutex_t mutex;
  pthread_cond_t updated;
  pthread_cond_t captured;
  uint64_t sequence;
  double x_px;
  double y_px;
  bool capture_pending;
  bool capture_done;
};

static struct observed_pointer observed_pointer = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .updated = PTHREAD_COND_INITIALIZER,
    .captured = PTHREAD_COND_INITIALIZER,
};

typedef int (*overlay_request_fn)(void *data);

struct overlay_request {
  overlay_request_fn fn;
  int result;
  bool done;
  bool capture;
  struct overlay_snapshot snapshot;
  pthread_mutex_t mutex;
  pthread_cond_t finished;
};

static void set_deadline(struct timespec *deadline, int timeout_ms) {
  clock_gettime(CLOCK_REALTIME, deadline);
  deadline->tv_sec += timeout_ms / 1000;
  deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
  if (deadline->tv_nsec >= 1000000000L) {
    deadline->tv_sec++;
    deadline->tv_nsec -= 1000000000L;
  }
}

static void release_request(struct overlay_request *request) {
  pthread_cond_destroy(&request->finished);
  pthread_mutex_destroy(&request->mutex);
  free(request);
}

static struct overlay_request *new_request(overlay_request_fn fn) {
  struct overlay_request *request = calloc(1, sizeof *request);
  request->fn = fn;
  request->result = -1;
  pthread_mutex_init(&request->mutex, NULL);
  pthread_cond_init(&request->finished, NULL);
  return request;
}

static gboolean dispatch_request(gpointer user_data) {
  struct overlay_request *request = user_data;

  int result = request->fn(request);

  pthread_mutex_lock(&request->mutex);
  request->result = result;
  request->done = true;
  pthread_cond_signal(&request->finished);
  pthread_mutex_unlock(&request->mutex);
  return G_SOURCE_REMOVE;
}

static int submit_request(struct overlay_request *request,
                          struct overlay_snapshot *out) {
  if (!g_atomic_int_get(&overlay_ready)) {
    release_request(request);
    return -1;
  }

  GSource *source = g_idle_source_new();
  g_source_set_callback(source, dispatch_request, request,
                        (GDestroyNotify)release_request);
  if (!g_source_attach(source, NULL)) {
    g_source_destroy(source);
    g_source_unref(source);
    release_request(request);
    return -1;
  }
  g_source_unref(source);

  struct timespec deadline;
  set_deadline(&deadline, REQUEST_TIMEOUT_MS);
  pthread_mutex_lock(&request->mutex);
  while (!request->done) {
    if (pthread_cond_timedwait(&request->finished, &request->mutex,
                               &deadline) != 0)
      break;
  }
  int result = request->result;
  if (out && result == 0)
    *out = request->snapshot;
  pthread_mutex_unlock(&request->mutex);
  return result;
}

static void set_source(cairo_t *cr, struct rgba color) {
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
}

static double percent_to_pixels(double percent, double total) {
  return total * percent / PERCENT;
}

static struct rgba pick_color(struct rgba primary, struct rgba secondary,
                              bool use_secondary) {
  return use_secondary ? secondary : primary;
}

static void draw_rects(cairo_t *cr, const OverlayRectArray rects, double width,
                       double height, bool use_secondary) {
  for (size_t i = 0; i < rects.len; i++) {
    const OverlayRect rect = rects.data[i];

    set_source(cr, pick_color(rect.color_primary, rect.color_secondary,
                              use_secondary));
    cairo_rectangle(cr, percent_to_pixels(rect.p.x_percent, width),
                    percent_to_pixels(rect.p.y_percent, height),
                    percent_to_pixels(rect.width_percent, width),
                    percent_to_pixels(rect.height_percent, height));
    cairo_fill(cr);
  }
}

static void draw_lines(cairo_t *cr, const OverlayLineArray lines, double width,
                       double height, bool use_secondary) {
  for (size_t i = 0; i < lines.len; i++) {
    const OverlayLine line = lines.data[i];
    set_source(cr, pick_color(line.color_primary, line.color_secondary,
                              use_secondary));
    cairo_set_line_width(cr, line.thickness);
    cairo_move_to(cr, percent_to_pixels(line.p1.x_percent, width),
                  percent_to_pixels(line.p1.y_percent, height));
    cairo_line_to(cr, percent_to_pixels(line.p2.x_percent, width),
                  percent_to_pixels(line.p2.y_percent, height));
    cairo_stroke(cr);
  }
}

static void draw_texts(cairo_t *cr, const OverlayTextArray texts, double width,
                       double height, bool use_secondary) {
  PangoLayout *layout = pango_cairo_create_layout(cr);

  for (size_t i = 0; i < texts.len; i++) {
    const OverlayText text = texts.data[i];
    set_source(cr, pick_color(text.color_primary, text.color_secondary,
                              use_secondary));

    g_autofree char *font = g_strdup_printf("Monospace %u", text.font_size);
    PangoFontDescription *font_desc = pango_font_description_from_string(font);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, text.text, text.text_len);
    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    cairo_move_to(
        cr, percent_to_pixels(text.p.x_percent, width) - text_width / 2.0,
        percent_to_pixels(text.p.y_percent, height) - text_height / 2.0);
    pango_cairo_show_layout(cr, layout);
  }
  g_object_unref(layout);
}

static gboolean signal_capture_commit(gpointer data) {
  (void)data;
  if (overlay_window) {
    GdkDisplay *display = gtk_widget_get_display(overlay_window);
    if (display)
      gdk_display_sync(display);
  }

  pthread_mutex_lock(&observed_pointer.mutex);
  observed_pointer.capture_done = true;
  pthread_cond_broadcast(&observed_pointer.captured);
  pthread_mutex_unlock(&observed_pointer.mutex);
  return G_SOURCE_REMOVE;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  struct overlay_state *overlay = user_data;
  double width = gtk_widget_get_allocated_width(widget);
  double height = gtk_widget_get_allocated_height(widget);

  pthread_mutex_lock(&overlay->scene_mutex);
  set_source(cr, overlay->base_background_color);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  draw_rects(cr, *overlay->rects, width, height, overlay->is_color_secondary);
  draw_lines(cr, *overlay->lines, width, height, overlay->is_color_secondary);
  draw_texts(cr, *overlay->texts, width, height, overlay->is_color_secondary);
  pthread_mutex_unlock(&overlay->scene_mutex);

  pthread_mutex_lock(&observed_pointer.mutex);
  bool signal_capture = observed_pointer.capture_pending;
  observed_pointer.capture_pending = false;
  pthread_mutex_unlock(&observed_pointer.mutex);

  if (signal_capture)
    g_idle_add(signal_capture_commit, NULL);

  return FALSE;
}

static void record_pointer_position(double x_px, double y_px) {
  pthread_mutex_lock(&observed_pointer.mutex);
  observed_pointer.x_px = x_px;
  observed_pointer.y_px = y_px;
  observed_pointer.sequence++;
  pthread_cond_broadcast(&observed_pointer.updated);
  pthread_mutex_unlock(&observed_pointer.mutex);
}

static gboolean on_pointer_motion(GtkWidget *widget, GdkEventMotion *event,
                                  gpointer user_data) {
  (void)widget;
  (void)user_data;
  record_pointer_position(event->x, event->y);
  return FALSE;
}

static gboolean on_pointer_enter(GtkWidget *widget, GdkEventCrossing *event,
                                 gpointer user_data) {
  (void)widget;
  (void)user_data;
  record_pointer_position(event->x, event->y);
  return FALSE;
}

static void on_realize(GtkWidget *widget, gpointer data) {
  (void)data;
  cairo_region_t *empty = cairo_region_create();
  gtk_widget_input_shape_combine_region(widget, empty);
  cairo_region_destroy(empty);
  g_atomic_int_set(&overlay_ready, 1);
}

int overlay_create(struct overlay_state *overlay) {
  pthread_mutex_init(&overlay->scene_mutex, NULL);

  pthread_mutex_lock(&observed_pointer.mutex);
  observed_pointer.sequence = 0;
  observed_pointer.x_px = 0;
  observed_pointer.y_px = 0;
  observed_pointer.capture_pending = false;
  observed_pointer.capture_done = false;
  pthread_mutex_unlock(&observed_pointer.mutex);

  gtk_init(NULL, NULL);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  overlay_window = window;
  g_atomic_int_set(&overlay_ready, 0);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);

  const GtkLayerShellEdge edges[] = {
      GTK_LAYER_SHELL_EDGE_TOP,
      GTK_LAYER_SHELL_EDGE_BOTTOM,
      GTK_LAYER_SHELL_EDGE_LEFT,
      GTK_LAYER_SHELL_EDGE_RIGHT,
  };
  for (size_t i = 0; i < G_N_ELEMENTS(edges); i++)
    gtk_layer_set_anchor(GTK_WINDOW(window), edges[i], TRUE);

  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(window), FALSE);

  g_signal_connect(window, "draw", G_CALLBACK(on_draw), overlay);
  g_signal_connect(window, "realize", G_CALLBACK(on_realize), NULL);
  g_signal_connect(window, "motion-notify-event", G_CALLBACK(on_pointer_motion),
                   NULL);
  g_signal_connect(window, "enter-notify-event", G_CALLBACK(on_pointer_enter),
                   NULL);
  gtk_widget_add_events(window,
                        GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK);

  gtk_widget_set_app_paintable(window, TRUE);
  gtk_widget_show_all(window);
  gtk_main();
  gtk_widget_destroy(window);
  g_atomic_int_set(&overlay_ready, 0);
  overlay_window = NULL;
  pthread_mutex_destroy(&overlay->scene_mutex);

  return 0;
}

static gboolean queue_draw_cb(gpointer data) {
  (void)data;
  if (overlay_window)
    gtk_widget_queue_draw(overlay_window);
  return G_SOURCE_REMOVE;
}

void overlay_redraw(void) { g_idle_add(queue_draw_cb, NULL); }

static gboolean quit_main_loop(gpointer data) {
  (void)data;
  g_atomic_int_set(&overlay_ready, 0);
  gtk_main_quit();
  return G_SOURCE_REMOVE;
}

void overlay_stop(void) { g_idle_add(quit_main_loop, NULL); }

static int update_input_region(void *data) {
  struct overlay_request *request = data;
  if (!overlay_window)
    return -1;

  cairo_region_t *region = cairo_region_create();
  if (request->capture) {
    cairo_rectangle_int_t area = {
        0,
        0,
        gtk_widget_get_allocated_width(overlay_window),
        gtk_widget_get_allocated_height(overlay_window),
    };
    cairo_region_union_rectangle(region, &area);
  }
  gtk_widget_input_shape_combine_region(overlay_window, region);
  cairo_region_destroy(region);

  pthread_mutex_lock(&observed_pointer.mutex);
  observed_pointer.capture_done = false;
  observed_pointer.capture_pending = true;
  pthread_mutex_unlock(&observed_pointer.mutex);

  gtk_widget_queue_draw(overlay_window);
  return 0;
}

static int wait_for_capture_commit(void) {
  struct timespec deadline;
  set_deadline(&deadline, REQUEST_TIMEOUT_MS);

  pthread_mutex_lock(&observed_pointer.mutex);
  while (!observed_pointer.capture_done) {
    if (pthread_cond_timedwait(&observed_pointer.captured,
                               &observed_pointer.mutex, &deadline) != 0) {
      fprintf(stderr, "Timed out applying overlay input region\n");
      pthread_mutex_unlock(&observed_pointer.mutex);
      return -1;
    }
  }
  pthread_mutex_unlock(&observed_pointer.mutex);
  return 0;
}

static int submit_pointer_tracking(bool enabled) {
  struct overlay_request *request = new_request(update_input_region);
  request->capture = enabled;
  if (submit_request(request, NULL) < 0)
    return -1;
  return wait_for_capture_commit();
}

static int set_pointer_tracking(bool enabled) {
  if (submit_pointer_tracking(enabled) == 0)
    return 0;
  submit_pointer_tracking(false);
  return -1;
}

int overlay_pointer_tracking_start(void) { return set_pointer_tracking(true); }
int overlay_pointer_tracking_stop(void) { return set_pointer_tracking(false); }

uint64_t overlay_pointer_arm_motion(void) {
  pthread_mutex_lock(&observed_pointer.mutex);
  uint64_t token = observed_pointer.sequence;
  pthread_mutex_unlock(&observed_pointer.mutex);
  return token;
}

static bool within_position_tolerance(double value, double expected,
                                      double tolerance) {
  double difference = value - expected;
  return (difference < 0 ? -difference : difference) <= tolerance;
}

bool overlay_pointer_wait_for_position(uint64_t token, double expected_x_px,
                                       double expected_y_px,
                                       double tolerance_px, int timeout_ms) {
  struct timespec deadline;
  set_deadline(&deadline, timeout_ms);

  pthread_mutex_lock(&observed_pointer.mutex);
  while (observed_pointer.sequence == token ||
         !within_position_tolerance(observed_pointer.x_px, expected_x_px,
                                    tolerance_px) ||
         !within_position_tolerance(observed_pointer.y_px, expected_y_px,
                                    tolerance_px)) {
    if (pthread_cond_timedwait(&observed_pointer.updated,
                               &observed_pointer.mutex, &deadline) != 0) {
      pthread_mutex_unlock(&observed_pointer.mutex);
      return false;
    }
  }
  pthread_mutex_unlock(&observed_pointer.mutex);
  return true;
}

static void read_geometry(struct overlay_geometry *out) {
  memset(out, 0, sizeof *out);

  out->width_px = gtk_widget_get_allocated_width(overlay_window);
  out->height_px = gtk_widget_get_allocated_height(overlay_window);

  GdkDisplay *display = gtk_widget_get_display(overlay_window);
  GdkWindow *window = gtk_widget_get_window(overlay_window);
  GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, window);
  if (monitor) {
    GdkRectangle monitor_geometry;
    gdk_monitor_get_geometry(monitor, &monitor_geometry);
    out->window_monitor_x = monitor_geometry.x;
    out->window_monitor_y = monitor_geometry.y;
  }

  int monitor_count = gdk_display_get_n_monitors(display);
  GdkMonitor *first_monitor =
      monitor_count > 0 ? gdk_display_get_monitor(display, 0) : NULL;
  if (!first_monitor) {
    out->monitors_bounds_width = (int)out->width_px;
    out->monitors_bounds_height = (int)out->height_px;
    return;
  }

  GdkRectangle first_monitor_geometry;
  gdk_monitor_get_geometry(first_monitor, &first_monitor_geometry);
  int min_x = first_monitor_geometry.x;
  int min_y = first_monitor_geometry.y;
  int max_x = first_monitor_geometry.x + first_monitor_geometry.width;
  int max_y = first_monitor_geometry.y + first_monitor_geometry.height;

  for (int i = 1; i < monitor_count; i++) {
    GdkMonitor *current = gdk_display_get_monitor(display, i);
    if (!current)
      continue;

    GdkRectangle monitor_rect;
    gdk_monitor_get_geometry(current, &monitor_rect);
    if (monitor_rect.x < min_x)
      min_x = monitor_rect.x;
    if (monitor_rect.y < min_y)
      min_y = monitor_rect.y;
    if (monitor_rect.x + monitor_rect.width > max_x)
      max_x = monitor_rect.x + monitor_rect.width;
    if (monitor_rect.y + monitor_rect.height > max_y)
      max_y = monitor_rect.y + monitor_rect.height;
  }

  out->monitors_bounds_x = min_x;
  out->monitors_bounds_y = min_y;
  out->monitors_bounds_width = max_x - min_x;
  out->monitors_bounds_height = max_y - min_y;
}

static int read_snapshot(void *data) {
  struct overlay_request *request = data;
  if (!overlay_window)
    return -1;

  read_geometry(&request->snapshot.geometry);

  pthread_mutex_lock(&observed_pointer.mutex);
  request->snapshot.pointer.x_px = observed_pointer.x_px;
  request->snapshot.pointer.y_px = observed_pointer.y_px;
  pthread_mutex_unlock(&observed_pointer.mutex);
  return 0;
}

int overlay_snapshot(struct overlay_snapshot *out) {
  struct overlay_request *request = new_request(read_snapshot);
  return submit_request(request, out);
}
