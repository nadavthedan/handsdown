#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "glib.h"
#include "overlay.h"

#include <cairo.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#define PERCENT 100.0

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
  for (uint i = 0; i < rects.len; i++) {
    const OverlayRect rect = rects.data[i];

    set_source(cr, pick_color(rect.color_primary, rect.color_secondary,
                              use_secondary));
    cairo_rectangle(cr, percent_to_pixels(rect.p.x_percent, width),
                    percent_to_pixels(rect.p.y_percent, height), rect.width,
                    rect.height);
    cairo_fill(cr);
  }
}

static void draw_lines(cairo_t *cr, const OverlayLineArray lines, double width,
                       double height, bool use_secondary) {
  for (uint i = 0; i < lines.len; i++) {
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

  for (uint i = 0; i < texts.len; i++) {
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

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  Overlay *overlay = user_data;
  overlay->width = gtk_widget_get_allocated_width(widget);
  overlay->height = gtk_widget_get_allocated_height(widget);

  set_source(cr, overlay->base_background_color);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  draw_rects(cr, *overlay->rects, overlay->width, overlay->height,
             overlay->is_color_secondary);
  draw_lines(cr, *overlay->lines, overlay->width, overlay->height,
             overlay->is_color_secondary);
  draw_texts(cr, *overlay->texts, overlay->width, overlay->height,
             overlay->is_color_secondary);

  return FALSE;
}

static void on_realize(GtkWidget *widget, gpointer data) {
  (void)data;
  cairo_region_t *empty = cairo_region_create();
  gtk_widget_input_shape_combine_region(widget, empty);
  cairo_region_destroy(empty);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer data) {
  Overlay *overlay = data;
  if (event->keyval == GDK_KEY_Escape) {
    gtk_widget_destroy(widget);
    return TRUE;
  }
  if (overlay->key_down_handler) {
    overlay->key_down_handler(overlay, event->keyval);
  }
  return TRUE;
}

static gboolean on_key_release(GtkWidget *widget, GdkEventKey *event,
                               gpointer data) {
  (void)widget;
  Overlay *overlay = data;
  if (overlay->key_release_handler) {
    overlay->key_release_handler(overlay, event->keyval);
  }
  return TRUE;
}

int overlay_create(Overlay *overlay) {
  gtk_init(NULL, NULL);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);

  const GtkLayerShellEdge edges[] = {
      GTK_LAYER_SHELL_EDGE_TOP,
      GTK_LAYER_SHELL_EDGE_BOTTOM,
      GTK_LAYER_SHELL_EDGE_LEFT,
      GTK_LAYER_SHELL_EDGE_RIGHT,
  };
  for (size_t i = 0; i < G_N_ELEMENTS(edges); i++) {
    gtk_layer_set_anchor(GTK_WINDOW(window), edges[i], TRUE);
  }

  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(window), FALSE);

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider, "window { background-color: rgba(0, 0, 0, 0.1); }", -1, NULL);
  GtkStyleContext *context = gtk_widget_get_style_context(window);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  g_signal_connect(window, "draw", G_CALLBACK(on_draw), overlay);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(window, "realize", G_CALLBACK(on_realize), NULL);
  // g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press),
  //                  overlay);
  // g_signal_connect(window, "key-release-event", G_CALLBACK(on_key_release),
  //                  overlay);

  gtk_widget_set_app_paintable(window, TRUE);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
