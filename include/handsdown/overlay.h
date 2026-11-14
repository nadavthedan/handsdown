#ifndef OVERLAY_H
#define OVERLAY_H

#include "glib.h"
#include "utils.h"
#include <stdbool.h>
#include <sys/types.h>

struct rgba {
  double red;
  double green;
  double blue;
  double alpha;
};

struct overlay_point {
  double x_percent;
  double y_percent;
};

typedef struct {
  struct overlay_point p;
  double width;
  double height;
  struct rgba color_primary;
  struct rgba color_secondary;
} OverlayRect;

typedef struct {
  struct overlay_point p1;
  struct overlay_point p2;
  double thickness;
  struct rgba color_primary;
  struct rgba color_secondary;
} OverlayLine;

typedef struct {
  struct overlay_point p;
  char *text;
  uint text_len;
  uint font_size;
  struct rgba color_primary;
  struct rgba color_secondary;
} OverlayText;

typedef struct Overlay Overlay;

typedef void (*KeyHandler)(Overlay *overlay, guint keyval);

DEFINE_ARRAY(OverlayRect, OverlayRectArray)
DEFINE_ARRAY(OverlayLine, OverlayLineArray)
DEFINE_ARRAY(OverlayText, OverlayTextArray)

struct Overlay {
  struct rgba base_background_color;
  bool is_color_secondary;
  double width;
  double height;
  OverlayRectArray *rects;
  OverlayLineArray *lines;
  OverlayTextArray *texts;
  KeyHandler key_down_handler;
  KeyHandler key_release_handler;
};

int overlay_create(Overlay *options);

#endif
