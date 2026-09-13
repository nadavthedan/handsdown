#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "gtk/gtkcssprovider.h"
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-layer-shell.h>

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer user_data) {
  g_print("Key pressed: keyval=%d, name=%s\n", event->keyval,
          gdk_keyval_name(event->keyval));

  if (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_q) {
    g_print("closing overlay...\n");
    gtk_main_quit();
    return TRUE;
  }

  if (event->keyval == GDK_KEY_w) {
    if (event->keyval == GDK_KEY_w) {
      g_print("Action: W key triggered!\n");
      return TRUE;
    }
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
      provider, "window { background-color: rgba(0, 0, 0, 0.5); ", -1, NULL);

  GtkStyleContext *context = gtk_widget_get_style_context(window);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
