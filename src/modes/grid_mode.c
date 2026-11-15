#include "grid_mode.h"
#include "overlay.h"
#include "utils.h"
#include <stdbool.h>
#include <stdlib.h>

static void create_grid_cross(OverlayLineArray *lines, double min_x_percent,
                              double min_y_percent, double max_x_percent,
                              double max_y_percent) {
  double x_mid_precent = middle(max_x_percent, min_x_percent);
  double y_mid_precent = middle(max_y_percent, min_y_percent);
  OverlayLineArray_push(lines, (OverlayLine){{x_mid_precent, min_y_percent},
                                             {x_mid_precent, max_y_percent},
                                             1,
                                             {255, 255, 255, 1},
                                             {0, 0, 0, 1}});
  OverlayLineArray_push(lines, (OverlayLine){{min_x_percent, y_mid_precent},
                                             {max_x_percent, y_mid_precent},
                                             1,
                                             {255, 255, 255, 1},
                                             {0, 0, 0, 1}});
}

struct overlay_state *grid_overlay_fourway_split_init(void) {
  struct overlay_state *overlay = calloc(1, sizeof(struct overlay_state));
  if (!overlay) {
    return NULL;
  }

  overlay->base_background_color = (struct rgba){0.0, 0.0, 0.0, 0.5};
  overlay->is_color_secondary = false;
  overlay->key_down_handler = NULL;
  overlay->key_release_handler = NULL;

  OverlayRectArray *rects = calloc(1, sizeof(OverlayRectArray));
  OverlayLineArray *lines = calloc(1, sizeof(OverlayLineArray));
  OverlayTextArray *texts = calloc(1, sizeof(OverlayTextArray));

  create_grid_cross(lines, 0, 0, 100, 100);
  create_grid_cross(lines, 0, 0, 50, 50);

  overlay->rects = rects;
  overlay->lines = lines;
  overlay->texts = texts;

  return overlay;
}

void grid_overlay_fourway_split_destroy(struct overlay_state *overlay) {
  if (!overlay) {
    return;
  }

  free(overlay->rects->data);
  free(overlay->lines->data);
  free(overlay->texts->data);
  free(overlay->rects);
  free(overlay->lines);
  free(overlay->texts);
  free(overlay);
}

int grid_mode_four_way_rec_split(void) {
  struct overlay_state *overlay = grid_overlay_fourway_split_init();
  if (!overlay) {
    return 1;
  }

  overlay_create(overlay);
  grid_overlay_fourway_split_destroy(overlay);

  return 0;
}
