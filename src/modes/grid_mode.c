#include "grid_mode.h"
#include "overlay.h"
#include <stdbool.h>
#include <stdlib.h>

GridOverlay *grid_overlay_fourway_split_init(GridConfiguration *config) {
  // make max section a global var and validate for it (right here it is 64)
  GridSection *sections = malloc(sizeof(GridSection) * 64);
  sections[0] = (GridSection){0, 0, 100, 100, 2, 2, {{0, 0, 0, 0}}};
  GridText *texts = malloc(4 * sizeof(GridText));
  texts[0] = (GridText){25, 25, "d", 1};
  texts[1] = (GridText){25, 75, "f", 1};
  texts[2] = (GridText){75, 25, "j", 1};
  texts[3] = (GridText){75, 75, "k", 1};
  GridState state = {0, 0, false, sections, 1, texts, 4};
  GridOverlay *overlay = calloc(1, sizeof(GridOverlay));
  overlay->config = *config;
  overlay->state = state;

  ModeData *mode = calloc(1, sizeof(ModeData));
  mode->go = overlay;

  overlay->mode_state_data = mode;
  return overlay;
}

void grid_overlay_fourway_split_destroy(GridOverlay *overlay) {
  ModeData *mode = (ModeData *)overlay->mode_state_data;
  free(overlay->state.grid_sections);
  free(overlay->state.grid_texts);
  free(mode);
  free(overlay);
}

int grid_mode_four_way_rec_split(void) {
  GridConfiguration config = {1,  {1, 1, 1, 0.3}, {0, 0, 0, 1},
                              20, {1, 1, 1, 0.3}, {0, 0, 0, 1}};
  GridOverlay *overlay = grid_overlay_fourway_split_init(&config);
  overlay_create(overlay);
  grid_overlay_fourway_split_destroy(overlay);

  return 0;
}
