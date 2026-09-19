#include "display.h"
#include "input.h"
#include "overlay.h"
#include <stdio.h>
#include <stdlib.h>

void grid_overlay_fourway_split_keyhandler(GridOverlay *go, guint keyval) {
  // int input_fd = *(int *)go->user_data;
  double last_width_percent =
      go->state.grid_sections[go->state.grid_sections_count - 1].width_percent;
  double last_height_percent =
      go->state.grid_sections[go->state.grid_sections_count - 1].height_percent;
  int x = AXIS_RESOLUTION_MAX * (last_width_percent / 2) / 100;
  int y = AXIS_RESOLUTION_MAX * (last_height_percent / 2) / 100;
  switch (keyval) {
  case GDK_KEY_d:
    return;
  case GDK_KEY_f:
    return;
  case GDK_KEY_j:
    return;
  case GDK_KEY_k:
    return;
  }
  return;
}

GridOverlay *grid_overlay_fourway_split_init(GridConfiguration *config,
                                             int input_fd) {
  GridSection *sections = malloc(sizeof(GridSection));
  sections[0] = (GridSection){1, 1, 100, 100, 2, 2, {{0, 0, 0, 0}}};
  GridText *texts = malloc(4 * sizeof(GridText));
  texts[0] = (GridText){25, 25, "d", 1};
  texts[1] = (GridText){25, 75, "f", 1};
  texts[2] = (GridText){75, 25, "j", 1};
  texts[3] = (GridText){75, 75, "k", 1};
  GridState state = {false, sections, 1, texts, 4};
  GridOverlay *overlay = malloc(sizeof(GridOverlay));
  overlay->config = *config;
  overlay->state = state;
  overlay->key_handler = grid_overlay_fourway_split_keyhandler;
  // overlay->user_data = &input_fd; // TODO: change this this is weird
  return overlay;
}

void grid_overlay_fourway_split_destroy(GridOverlay *overlay) {
  free(overlay->state.grid_sections);
  free(overlay->state.grid_texts);
  free(overlay);
}

int grid_mode_four_way_rec_split(int input_fd, DisplayContext *display_ctx) {

  Monitor my_monitors[MAX_MONITORS];
  for (int i = 0; i < 5; i++) {
    int count = display_get_monitors(display_ctx, my_monitors, MAX_MONITORS);

    printf("Number of monitors: %d\n", count);
    for (int m = 0; m < count; m++) {
      printf("Mointor %d: %s | res: %dx%d at Position (X: %d, Y: %d)\n", m,
             my_monitors[m].name, my_monitors[m].width, my_monitors[m].height,
             my_monitors[m].x, my_monitors[m].y);
    }
  }
  GridConfiguration config = {1,  {1, 1, 1, 0.3}, {0, 0, 0, 1},
                              20, {1, 1, 1, 0.3}, {0, 0, 0, 1}};
  GridOverlay *overlay = grid_overlay_fourway_split_init(&config, input_fd);
  overlay_create(overlay);
  grid_overlay_fourway_split_destroy(overlay);

  return 0;
}
