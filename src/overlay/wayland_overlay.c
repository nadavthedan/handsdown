#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "glib.h"
#include "overlay.h"

#include <cairo.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#define PERCENT 100.0
#define HALF_PIXEL 0.5

static const struct rgba BACKDROP = {0, 0, 0, 0.5};

typedef struct {
  double x;
  double y;
  double width;
  double height;
} Rect;

static void set_source(cairo_t *cr, struct rgba color) {
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
}

static struct rgba pick_color(struct rgba primary, struct rgba secondary,
                              bool use_secondary) {
  return use_secondary ? secondary : primary;
}

static Rect section_rect(const GridSection *section, int width, int height) {
  return (Rect){
      .x = section->start_x_percent * width / PERCENT,
      .y = section->start_y_percent * height / PERCENT,
      .width = section->width_percent * width / PERCENT,
      .height = section->height_percent * height / PERCENT,
  };
}

static void draw_section_backgrounds(cairo_t *cr, const GridSection sections[],
                                     uint sections_len, int width, int height) {
  for (uint i = 0; i < sections_len; i++) {
    const GridSection *section = &sections[i];
    Rect rect = section_rect(section, width, height);

    set_source(cr, section->config.background_color);
    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr);
  }
}

static void draw_section_grid(cairo_t *cr, const GridSection sections[],
                              uint sections_len, int width, int height) {
  for (uint i = 0; i < sections_len; i++) {
    const GridSection *section = &sections[i];
    Rect rect = section_rect(section, width, height);

    double cell_width = rect.width / section->cell_horizontal_count;
    double cell_height = rect.height / section->cell_vertical_count;

    for (double x = cell_width; x < rect.width; x += cell_width) {
      cairo_move_to(cr, rect.x + x + HALF_PIXEL, rect.y);
      cairo_line_to(cr, rect.x + x + HALF_PIXEL, rect.y + rect.height);
    }

    for (double y = cell_height; y < rect.height; y += cell_height) {
      cairo_move_to(cr, rect.x, rect.y + y + HALF_PIXEL);
      cairo_line_to(cr, rect.x + rect.width, rect.y + y + HALF_PIXEL);
    }
  }
  cairo_stroke(cr);
}

static void draw_texts(cairo_t *cr, const GridText texts[], uint texts_count,
                       int width, int height, uint font_size) {
  PangoLayout *layout = pango_cairo_create_layout(cr);
  g_autofree char *font = g_strdup_printf("Monospace %u", font_size);
  PangoFontDescription *font_desc = pango_font_description_from_string(font);
  pango_layout_set_font_description(layout, font_desc);
  pango_font_description_free(font_desc);

  for (uint i = 0; i < texts_count; i++) {
    const GridText *text = &texts[i];
    pango_layout_set_text(layout, text->text, text->text_len);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    double x = text->center_x_percent * width / PERCENT;
    double y = text->center_y_percent * height / PERCENT;
    cairo_move_to(cr, x - text_width / 2.0, y - text_height / 2.0);
    pango_cairo_show_layout(cr, layout);
  }

  g_object_unref(layout);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  GridOverlay *overlay = user_data;
  GridState *state = &overlay->state;
  GridConfiguration *config = &overlay->config;

  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);
  state->width = width;
  state->height = height;

  set_source(cr, BACKDROP);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  draw_section_backgrounds(cr, state->grid_sections, state->grid_sections_count,
                           width, height);

  set_source(cr,
             pick_color(config->border_color, config->border_secondary_color,
                        state->use_secondary_color));
  cairo_set_line_width(cr, config->border_thickness);
  draw_section_grid(cr, state->grid_sections, state->grid_sections_count, width,
                    height);

  set_source(cr, pick_color(config->font_color, config->font_secondary_color,
                            state->use_secondary_color));
  draw_texts(cr, state->grid_texts, state->grid_texts_count, width, height,
             config->font_size);

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
  GridOverlay *overlay = data;
  if (event->keyval == GDK_KEY_Escape) {
    gtk_widget_destroy(widget);
    return TRUE;
  }
  if (overlay->key_handler) {
    overlay->key_handler(overlay, event->keyval);
  }
  return TRUE;
}

static gboolean on_key_release(GtkWidget *widget, GdkEventKey *event,
                               gpointer data) {
  (void)widget;
  GridOverlay *overlay = data;
  // if (overlay->key_release_handler) {
  //   overlay->key_release_handler(overlay, event->keyval);
  // }
  return TRUE;
}

int overlay_create(GridOverlay *overlay) {
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

  // gtk_layer_set_keyboard_interactivity(GTK_WINDOW(window), FALSE);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

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
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press),
                   overlay);
  g_signal_connect(window, "key-release-event", G_CALLBACK(on_key_release),
                   overlay);

  gtk_widget_set_app_paintable(window, TRUE);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
