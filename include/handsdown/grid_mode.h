#ifndef GRID_MODE_H
#define GRID_MODE_H

#include "overlay.h"

typedef struct {
  GridOverlay *go;
} ModeData;

int grid_mode_four_way_rec_split(void);

#endif
