#ifndef OVERLAY_H
#define OVERLAY_H

#include "gdk/gdk.h"
#include "glib.h"
#include "input.h"
#include <stdbool.h>
#include <sys/types.h>

struct rgba {
  double red;
  double green;
  double blue;
  double alpha;
};

typedef struct {
  double start_x_percent;
  double start_y_percent;
  double width_percent;
  double height_percent;
  double cell_horizontal_count;
  double cell_vertical_count;
  struct {
    struct rgba background_color;
  } config;
} GridSection;

typedef struct {
  double center_x_percent;
  double center_y_percent;
  char *text;
  uint text_len;
} GridText;

typedef struct {
  double border_thickness;
  struct rgba border_color;
  struct rgba border_secondary_color;
  uint font_size;
  struct rgba font_color;
  struct rgba font_secondary_color;
} GridConfiguration;

typedef struct {
  gdouble width;
  gdouble height;
  bool use_secondary_color;
  GridSection *grid_sections;
  uint grid_sections_count;
  GridText *grid_texts;
  uint grid_texts_count;
} GridState;

typedef struct {
  gboolean cursor_in_window;
  gdouble x;
  gdouble y;
} CursorData;

typedef struct GridOverlay GridOverlay;

typedef void (*KeyHandler)(GridOverlay *overlay, guint keyval);

struct GridOverlay {
  GridConfiguration config;
  GridState state;
  KeyHandler key_handler;
  CursorData cursor;
  void *mode_state_data;
};

int overlay_create(GridOverlay *options);

#endif
