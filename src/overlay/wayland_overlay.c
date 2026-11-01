#include "cairo.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtkcssprovider.h"
#include "overlay.h"
#include "pango/pango-font.h"
#include "pango/pango-layout.h"
#include "pango/pangocairo.h"
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <stdio.h>

static gboolean
on_draw(GtkWidget *widget, cairo_t *cr,
        gpointer user_data) { // TODO: make this readable and clean
  GridOverlay *grid_overlay = (GridOverlay *)user_data;
  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);
  grid_overlay->state.width = width;
  grid_overlay->state.height = height;

  cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

  for (uint i = 0; i < grid_overlay->state.grid_sections_count; i++) {
    GridSection section = grid_overlay->state.grid_sections[i];

    double x_start = section.start_x_percent * width / 100;
    double section_width = section.width_percent * width / 100;

    double y_start = section.start_y_percent * height / 100;
    double section_height = section.height_percent * height / 100;

    cairo_set_source_rgba(cr, section.config.background_color.red,
                          section.config.background_color.green,
                          section.config.background_color.blue,
                          section.config.background_color.alpha);
    cairo_rectangle(cr, x_start, y_start, section_width, section_height);
    cairo_fill(cr);
  }

  if (!grid_overlay->state.use_secondary_color) {
    cairo_set_source_rgba(cr, grid_overlay->config.border_color.red,
                          grid_overlay->config.border_color.green,
                          grid_overlay->config.border_color.blue,
                          grid_overlay->config.border_color.alpha);
  } else {
    cairo_set_source_rgba(cr, grid_overlay->config.border_secondary_color.red,
                          grid_overlay->config.border_secondary_color.green,
                          grid_overlay->config.border_secondary_color.blue,
                          grid_overlay->config.border_secondary_color.alpha);
  }
  cairo_set_line_width(cr, grid_overlay->config.border_thickness);

  for (uint i = 0; i < grid_overlay->state.grid_sections_count; i++) {
    GridSection section = grid_overlay->state.grid_sections[i];

    double x_start = section.start_x_percent * width / 100;
    double section_width = section.width_percent * width / 100;
    double cell_width = section_width / section.cell_horizontal_count;
    double x_end = x_start + section_width;

    double y_start = section.start_y_percent * height / 100;
    double section_height = section.height_percent * height / 100;
    double cell_height = section_height / section.cell_vertical_count;
    double y_end = y_start + section_height;

    for (double x_draw = cell_width; x_start + x_draw < x_end;
         x_draw += cell_width) {
      cairo_move_to(cr, x_start + x_draw + 0.5, y_start);
      cairo_line_to(cr, x_start + x_draw + 0.5, y_end);
    }

    for (double y_draw = cell_height; y_start + y_draw < y_end;
         y_draw += cell_height) {
      cairo_move_to(cr, x_start, y_start + y_draw + 0.5);
      cairo_line_to(cr, x_end, y_start + y_draw + 0.5);
    }
  }
  cairo_stroke(cr);

  PangoLayout *layout = pango_cairo_create_layout(cr);
  char font[64];
  snprintf(font, sizeof(font), "Monospace %d", grid_overlay->config.font_size);
  PangoFontDescription *font_desc = pango_font_description_from_string(font);
  pango_layout_set_font_description(layout, font_desc);
  pango_font_description_free(font_desc);
  for (uint i = 0; i < grid_overlay->state.grid_texts_count; i++) {
    GridText gtext = grid_overlay->state.grid_texts[i];
    if (!grid_overlay->state.use_secondary_color) {
      cairo_set_source_rgba(cr, grid_overlay->config.font_color.red,
                            grid_overlay->config.font_color.green,
                            grid_overlay->config.font_color.blue,
                            grid_overlay->config.font_color.alpha);
    } else {
      cairo_set_source_rgba(cr, grid_overlay->config.font_secondary_color.red,
                            grid_overlay->config.font_secondary_color.green,
                            grid_overlay->config.font_secondary_color.blue,
                            grid_overlay->config.font_secondary_color.alpha);
    }
    pango_layout_set_text(layout, gtext.text, gtext.text_len);
    double x = gtext.center_x_percent * width / 100;
    double y = gtext.center_y_percent * height / 100;
    cairo_move_to(cr, x - ((double)grid_overlay->config.font_size / 2),
                  y - ((double)grid_overlay->config.font_size / 2));
    pango_cairo_show_layout(cr, layout);
  }

  g_object_unref(layout);

  return FALSE; // Let event propagate
}

static void on_realize(GtkWidget *widget, gpointer data) {
  cairo_region_t *empty = cairo_region_create();
  gtk_widget_input_shape_combine_region(widget, empty);
  cairo_region_destroy(empty);
}

int overlay_create(GridOverlay *overlay) {
  gtk_init(NULL, NULL);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);

  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(window), FALSE);
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider, "window { background-color: rgba(0, 0, 0, 0.1); }", -1, NULL);

  GtkStyleContext *context = gtk_widget_get_style_context(window);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect(window, "draw", G_CALLBACK(on_draw), overlay);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(window, "realize", G_CALLBACK(on_realize), NULL);

  gtk_widget_set_app_paintable(window, TRUE);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
