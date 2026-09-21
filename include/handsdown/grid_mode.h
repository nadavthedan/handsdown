#ifndef GRID_MODE_H
#define GRID_MODE_H

#include "display.h"
#include "overlay.h"

typedef struct {
  int input_fd;
  GridOverlay *go;
  guint settle_source;
  gboolean settle_active;
  double tx;
  double ty;
  double last_x;
  double last_y;
  int steps;
} ModeData;

int grid_mode_four_way_rec_split(int input_fd, DisplayContext *display_ctx);

#endif
