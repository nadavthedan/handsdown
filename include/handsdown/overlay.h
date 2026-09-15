#ifndef OVERLAY_H
#define OVERLAY_H

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
  struct rgba background_color;
} GridSection;

typedef struct {
  double center_x_percent;
  double center_y_percent;
  char *text;
  uint text_len;
} GridText;

typedef struct {
  double border_thickness;
  bool use_secondary_color;
  struct rgba border_color;
  struct rgba border_secondary_color;
  uint font_size;
  struct rgba font_color;
  struct rgba font_secondary_color;
  GridSection *grid_sections;
  uint grid_sections_count;
  GridText *grid_texts;
  uint grid_texts_count;
} GridOptions;

int overlay_create(GridOptions *options);

#endif
