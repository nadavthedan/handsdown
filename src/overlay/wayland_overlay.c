#include "cairo.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "gtk/gtkcssprovider.h"
#include "overlay.h"
#include "pango/pango-font.h"
#include "pango/pango-layout.h"
#include "pango/pangocairo.h"
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <stdio.h>

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  GridOptions *grid_options = (GridOptions *)user_data;
  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);

  cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

  for (uint i = 0; i < grid_options->grid_sections_count; i++) {
    GridSection section = grid_options->grid_sections[i];

    double x_start = section.start_x_percent * width / 100;
    double section_width = section.width_percent * width / 100;

    double y_start = section.start_y_percent * height / 100;
    double section_height = section.height_percent * height / 100;

    cairo_set_source_rgba(
        cr, section.background_color.red, section.background_color.green,
        section.background_color.blue, section.background_color.alpha);
    cairo_rectangle(cr, x_start, y_start, section_width, section_height);
    cairo_fill(cr);
  }

  if (!grid_options->use_secondary_color) {
    cairo_set_source_rgba(
        cr, grid_options->border_color.red, grid_options->border_color.green,
        grid_options->border_color.blue, grid_options->border_color.alpha);
  } else {
    cairo_set_source_rgba(cr, grid_options->border_secondary_color.red,
                          grid_options->border_secondary_color.green,
                          grid_options->border_secondary_color.blue,
                          grid_options->border_secondary_color.alpha);
  }
  cairo_set_line_width(cr, grid_options->border_thickness);

  for (uint i = 0; i < grid_options->grid_sections_count; i++) {
    GridSection section = grid_options->grid_sections[i];

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
  snprintf(font, sizeof(font), "Monospace %d", grid_options->font_size);
  PangoFontDescription *font_desc = pango_font_description_from_string(font);
  pango_layout_set_font_description(layout, font_desc);
  pango_font_description_free(font_desc);
  for (uint i = 0; i < grid_options->grid_texts_count; i++) {
    GridText gtext = grid_options->grid_texts[i];
    if (!grid_options->use_secondary_color) {
      cairo_set_source_rgba(
          cr, grid_options->font_color.red, grid_options->font_color.green,
          grid_options->font_color.blue, grid_options->font_color.alpha);
    } else {
      cairo_set_source_rgba(cr, grid_options->font_secondary_color.red,
                            grid_options->font_secondary_color.green,
                            grid_options->font_secondary_color.blue,
                            grid_options->font_secondary_color.alpha);
    }
    pango_layout_set_text(layout, gtext.text, gtext.text_len);
    double x = gtext.center_x_percent * width / 100;
    double y = gtext.center_y_percent * height / 100;
    cairo_move_to(cr, x - ((double)grid_options->font_size / 2),
                  y - ((double)grid_options->font_size / 2));
    pango_cairo_show_layout(cr, layout);
  }

  g_object_unref(layout);

  return FALSE; // Let event propagate
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer user_data) {
  g_print("Key pressed: keyval=%d, name=%s\n", event->keyval,
          gdk_keyval_name(event->keyval));
  GridOptions *grid_options = (GridOptions *)user_data;

  switch (event->keyval) {
  case GDK_KEY_Escape:
  case GDK_KEY_q:
    g_print("closing overlay...\n");
    gtk_main_quit();
    return TRUE;
  case GDK_KEY_w:
  case GDK_KEY_W:
    g_print("Action: W key triggered!\n");
    return TRUE;
  case GDK_KEY_i:
  case GDK_KEY_I:
    grid_options->use_secondary_color = !grid_options->use_secondary_color;
    g_print("Inversion called. Secondary-Color: %s\n",
            grid_options->use_secondary_color ? "True" : "False");
    gtk_widget_queue_draw(widget); // redraw overlay
    return TRUE;
  }

  return TRUE;
}

int overlay_create(GridOptions *options) {
  gtk_init(NULL, NULL);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);

  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

  gtk_layer_set_keyboard_interactivity(GTK_WINDOW(window), TRUE);
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider, "window { background-color: rgba(0, 0, 0, 0.1); ", -1, NULL);

  GtkStyleContext *context = gtk_widget_get_style_context(window);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect(window, "draw", G_CALLBACK(on_draw), options);
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press),
                   options);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
