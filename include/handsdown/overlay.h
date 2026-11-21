#ifndef OVERLAY_H
#define OVERLAY_H

#include "utils.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
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
  double width_percent;
  double height_percent;
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

struct overlay_pointer_position {
  double x_px;
  double y_px;
};

struct overlay_geometry {
  double width_px;
  double height_px;
  int window_monitor_x;
  int window_monitor_y;
  int monitors_bounds_x;
  int monitors_bounds_y;
  int monitors_bounds_width;
  int monitors_bounds_height;
};

DEFINE_ARRAY(OverlayRect, OverlayRectArray)
DEFINE_ARRAY(OverlayLine, OverlayLineArray)
DEFINE_ARRAY(OverlayText, OverlayTextArray)

struct overlay_state {
  struct rgba base_background_color;
  bool is_color_secondary;
  OverlayRectArray *rects;
  OverlayLineArray *lines;
  OverlayTextArray *texts;
  pthread_mutex_t scene_mutex;
};

struct overlay_snapshot {
  struct overlay_geometry geometry;
  struct overlay_pointer_position pointer;
};

int overlay_create(struct overlay_state *options);
void overlay_redraw(void);
void overlay_stop(void);
int overlay_pointer_tracking_start(void);
int overlay_pointer_tracking_stop(void);
uint64_t overlay_pointer_arm_motion(void);
bool overlay_pointer_wait_for_position(uint64_t token, double expected_x_px,
                                       double expected_y_px,
                                       double tolerance_px, int timeout_ms);
int overlay_snapshot(struct overlay_snapshot *out);

#endif
