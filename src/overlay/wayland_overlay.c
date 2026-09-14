#include "cairo.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "gtk/gtkcssprovider.h"
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-layer-shell.h>

typedef struct {
  double cell_width;
  double cell_height;
  double border_thickness;
  gboolean use_secondary_color;
  struct {
    double red;
    double green;
    double blue;
    double alpha;
  } border_color;
  struct {
    double red;
    double green;
    double blue;
    double alpha;
  } border_secondary_color;
} GridOptions;

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  GridOptions *grid_options = (GridOptions *)user_data;
  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);

  cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

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

  for (int x = grid_options->cell_width; x < width;
       x += grid_options->cell_width) {
    cairo_move_to(cr, x + 0.5, 0);
    cairo_line_to(cr, x + 0.5, height);
  }

  for (int y = grid_options->cell_height; y < height;
       y += grid_options->cell_height) {
    cairo_move_to(cr, 0, y + 0.5);
    cairo_line_to(cr, width, y + 0.5);
  }

  cairo_stroke(cr);

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

int overlay_create(int *argc, char **argv[]) {
  gtk_init(argc, argv);
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

  GridOptions options = {100, 50, 1, FALSE, {1, 1, 1, 0.3}, {0, 0, 0, 1}};
  g_signal_connect(window, "draw", G_CALLBACK(on_draw), &options);
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press),
                   &options);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
