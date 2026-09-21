#include "grid_mode.h"
#include "display.h"
#include "gdk/gdkkeysyms.h"
#include "input.h"
#include "overlay.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SETTLE_INTERVAL_MS 2
#define CONVERGE_THRESHOLD_PX 1.0
#define MAX_STEPS 14
#define MOVE_DAMPING 0.5

static void clamp_delta(GridOverlay *go, double dx, double dy, int *out_x,
                        int *out_y) {
  int dxi = (int)round(dx);
  int dyi = (int)round(dy);

  int max_left = (int)go->cursor.x;
  int max_right = (int)go->state.width - max_left;
  int max_top = (int)go->cursor.y;
  int max_bottom = (int)go->state.height - max_top;

  if (dxi < -max_left)
    dxi = -max_left;
  if (dxi > max_right)
    dxi = max_right;
  if (dyi < -max_top)
    dyi = -max_top;
  if (dyi > max_bottom)
    dyi = max_bottom;

  *out_x = dxi;
  *out_y = dyi;
}

static void settle_cancel(ModeData *mode) {
  if (!mode->settle_active)
    return;
  g_source_remove(mode->settle_source);
  mode->settle_active = FALSE;
  mode->settle_source = 0;
}

// the delta needs to be damped to not accidently get out of the window
static void damped_delta_to(GridOverlay *go, double tx, double ty, int *dx,
                            int *dy) {
  clamp_delta(go, MOVE_DAMPING * (tx - go->cursor.x),
              MOVE_DAMPING * (ty - go->cursor.y), dx, dy);
}

// Runs every SETTLE_INTERVAL_MS: trims the residual between the measured
// cursor position and the target until the move converges.
static gboolean settle_step(gpointer data) {
  ModeData *mode = (ModeData *)data;
  GridOverlay *go = mode->go;

  if (!go->cursor.cursor_in_window) {
    settle_cancel(mode);
    return G_SOURCE_REMOVE;
  }

  double dx = mode->tx - go->cursor.x;
  double dy = mode->ty - go->cursor.y;

  if ((fabs(dx) <= CONVERGE_THRESHOLD_PX &&
       fabs(dy) <= CONVERGE_THRESHOLD_PX) ||
      mode->steps >= MAX_STEPS) {
    settle_cancel(mode);
    return G_SOURCE_REMOVE;
  }

  // No feedback from the previous move yet (the compositor hasn't returned
  // our motion event). Re-sending now would double-apply and overshoot.
  if (go->cursor.x == mode->last_x && go->cursor.y == mode->last_y)
    return G_SOURCE_CONTINUE;

  int dxi, dyi;
  damped_delta_to(go, mode->tx, mode->ty, &dxi, &dyi);
  input_rel_move(mode->input_fd, dxi, dyi);
  mode->last_x = go->cursor.x;
  mode->last_y = go->cursor.y;
  mode->steps++;
  return G_SOURCE_CONTINUE;
}

static void overlay_move_to(GridOverlay *go, ModeData *mode, double tx,
                            double ty) {
  if (!go->cursor.cursor_in_window)
    return;

  settle_cancel(mode);

  mode->tx = tx;
  mode->ty = ty;
  mode->steps = 0;

  int dxi, dyi;
  damped_delta_to(go, tx, ty, &dxi, &dyi);
  input_rel_move(mode->input_fd, dxi, dyi);
  mode->last_x = go->cursor.x;
  mode->last_y = go->cursor.y;

  mode->settle_active = TRUE;
  mode->settle_source = g_timeout_add(SETTLE_INTERVAL_MS, settle_step, mode);
}

void grid_overlay_fourway_split_keyhandler(GridOverlay *go, guint keyval) {
  ModeData *mode = (ModeData *)go->mode_state_data;
  GridSection last_section =
      go->state.grid_sections[go->state.grid_sections_count - 1];
  double last_width_percent = last_section.width_percent;
  double last_height_percent = last_section.height_percent;
  double last_x_start_percent = last_section.start_x_percent;
  double last_y_start_percent = last_section.start_y_percent;
  double last_start_x = go->state.width * last_x_start_percent / 100;
  double last_start_y = go->state.height * last_y_start_percent / 100;
  double last_width = go->state.width * (last_width_percent / 2) / 100;
  double last_height = go->state.height * (last_height_percent / 2) / 100;
  double base_x = last_start_x + last_width;
  double base_y = last_start_y + last_height;

  double tx, ty;
  bool plus_x = false;
  bool plus_y = false;
  switch (keyval) {
  case GDK_KEY_u:
    if (go->state.grid_sections_count > 1) {
      --go->state.grid_sections_count;
      last_section = go->state.grid_sections[go->state.grid_sections_count - 1];
      go->state.grid_texts[0].center_x_percent =
          last_section.start_x_percent + last_section.width_percent / 4;
      go->state.grid_texts[0].center_y_percent =
          last_section.start_y_percent + last_section.height_percent / 4;
      go->state.grid_texts[1].center_x_percent =
          last_section.start_x_percent + last_section.width_percent / 4;
      go->state.grid_texts[1].center_y_percent =
          last_section.start_y_percent + last_section.height_percent / 4 +
          last_section.height_percent / 2;
      go->state.grid_texts[2].center_x_percent =
          last_section.start_x_percent + last_section.width_percent / 4 +
          last_section.width_percent / 2;
      go->state.grid_texts[2].center_y_percent =
          last_section.start_y_percent + last_section.height_percent / 4;
      go->state.grid_texts[3].center_x_percent =
          last_section.start_x_percent + last_section.width_percent / 4 +
          last_section.width_percent / 2;
      go->state.grid_texts[3].center_y_percent =
          last_section.start_y_percent + last_section.height_percent / 4 +
          last_section.height_percent / 2;
    }
    return;
  case GDK_KEY_d:
    tx = base_x - last_width / 2;
    ty = base_y - last_height / 2;
    break;
  case GDK_KEY_f:
    plus_y = true;
    tx = base_x - last_width / 2;
    ty = base_y + last_height / 2;
    break;
  case GDK_KEY_j:
    plus_x = true;
    tx = base_x + last_width / 2;
    ty = base_y - last_height / 2;
    break;
  case GDK_KEY_k:
    plus_x = true;
    plus_y = true;
    tx = base_x + last_width / 2;
    ty = base_y + last_height / 2;
    break;
  default:
    return;
  }

  printf("current cursor: x: %f, y: %f - goto : x: %f, y: %f\n", go->cursor.x,
         go->cursor.y, tx, ty);
  overlay_move_to(go, mode, tx, ty);
  GridSection *part_section = malloc(sizeof(GridSection));
  part_section->start_x_percent =
      plus_x ? last_x_start_percent + last_width_percent / 2
             : last_x_start_percent;
  part_section->start_y_percent =
      plus_y ? last_y_start_percent + last_height_percent / 2
             : last_y_start_percent;
  part_section->width_percent = last_width_percent / 2;
  part_section->height_percent = last_height_percent / 2;
  part_section->cell_horizontal_count = last_section.cell_horizontal_count;
  part_section->cell_vertical_count = last_section.cell_vertical_count;
  go->state.grid_sections[go->state.grid_sections_count++] = *part_section;
  go->state.grid_texts[0].center_x_percent =
      part_section->start_x_percent + part_section->width_percent / 4;
  go->state.grid_texts[0].center_y_percent =
      part_section->start_y_percent + part_section->height_percent / 4;
  go->state.grid_texts[1].center_x_percent =
      part_section->start_x_percent + part_section->width_percent / 4;
  go->state.grid_texts[1].center_y_percent = part_section->start_y_percent +
                                             part_section->height_percent / 4 +
                                             part_section->height_percent / 2;
  go->state.grid_texts[2].center_x_percent = part_section->start_x_percent +
                                             part_section->width_percent / 4 +
                                             part_section->width_percent / 2;
  go->state.grid_texts[2].center_y_percent =
      part_section->start_y_percent + part_section->height_percent / 4;
  go->state.grid_texts[3].center_x_percent = part_section->start_x_percent +
                                             part_section->width_percent / 4 +
                                             part_section->width_percent / 2;
  go->state.grid_texts[3].center_y_percent = part_section->start_y_percent +
                                             part_section->height_percent / 4 +
                                             part_section->height_percent / 2;
}

GridOverlay *grid_overlay_fourway_split_init(GridConfiguration *config,
                                             int input_fd) {
  // make max section a global var and validate for it (right here it is 64)
  GridSection *sections = malloc(sizeof(GridSection) * 64);
  sections[0] = (GridSection){1, 1, 100, 100, 2, 2, {{0, 0, 0, 0}}};
  GridText *texts = malloc(4 * sizeof(GridText));
  texts[0] = (GridText){25, 25, "d", 1};
  texts[1] = (GridText){25, 75, "f", 1};
  texts[2] = (GridText){75, 25, "j", 1};
  texts[3] = (GridText){75, 75, "k", 1};
  GridState state = {0, 0, false, sections, 1, texts, 4};
  GridOverlay *overlay = calloc(1, sizeof(GridOverlay));
  overlay->config = *config;
  overlay->state = state;
  overlay->key_handler = grid_overlay_fourway_split_keyhandler;

  ModeData *mode = calloc(1, sizeof(ModeData));
  mode->input_fd = input_fd;
  mode->go = overlay;

  overlay->mode_state_data = mode;
  return overlay;
}

void grid_overlay_fourway_split_destroy(GridOverlay *overlay) {
  ModeData *mode = (ModeData *)overlay->mode_state_data;
  settle_cancel(mode);
  free(overlay->state.grid_sections);
  free(overlay->state.grid_texts);
  free(mode);
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
