#include "grid_mode.h"
#include "input.h"
#include "overlay.h"
#include "utils.h"
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdio.h>

static void create_grid_rect(OverlayRectArray *rects, double min_x_percent,
                             double min_y_percent, double max_x_percent,
                             double max_y_percent) {
  OverlayRectArray_push(rects, (OverlayRect){{min_x_percent, min_y_percent},
                                             max_x_percent - min_x_percent,
                                             max_y_percent - min_y_percent,
                                             {255, 255, 255, 0},
                                             {0, 0, 0, 0}});
}

static void create_grid_cross(OverlayLineArray *lines, double min_x_percent,
                              double min_y_percent, double max_x_percent,
                              double max_y_percent) {
  double x_mid_percent = middle(max_x_percent, min_x_percent);
  double y_mid_percent = middle(max_y_percent, min_y_percent);
  OverlayLineArray_push(lines, (OverlayLine){{x_mid_percent, min_y_percent},
                                             {x_mid_percent, max_y_percent},
                                             1,
                                             {255, 255, 255, 1},
                                             {0, 0, 0, 1}});
  OverlayLineArray_push(lines, (OverlayLine){{min_x_percent, y_mid_percent},
                                             {max_x_percent, y_mid_percent},
                                             1,
                                             {255, 255, 255, 1},
                                             {0, 0, 0, 1}});
}

static void move_text_to_latest_cross(OverlayTextArray *texts,
                                      double min_x_percent,
                                      double min_y_percent,
                                      double max_x_percent,
                                      double max_y_percent) {
  static char labels[][2] = {{"f"}, {"j"}, {"d"}, {"k"}};
  static const double x_fractions[] = {0.25, 0.75, 0.25, 0.75};
  static const double y_fractions[] = {0.25, 0.25, 0.75, 0.75};
  static const uint font_sizes[] = {20, 20, 20, 20};
  struct rgba black = {0, 0, 0, 1};
  struct rgba white = {255, 255, 255, 1};
  double width_percent = max_x_percent - min_x_percent;
  double height_percent = max_y_percent - min_y_percent;

  for (size_t i = 0; i < sizeof labels / sizeof labels[0]; i++) {
    OverlayText text = {
        {min_x_percent + width_percent * x_fractions[i],
         min_y_percent + height_percent * y_fractions[i]},
        labels[i],
        1,
        font_sizes[i],
        white,
        black,
    };
    if (texts->len == i)
      OverlayTextArray_push(texts, text);
    else
      texts->data[i] = text;
  }
}

static void focus_split(struct overlay_state *overlay, double min_x_percent,
                        double min_y_percent, double max_x_percent,
                        double max_y_percent) {
  pthread_mutex_lock(&overlay->scene_mutex);
  create_grid_rect(overlay->rects, min_x_percent, min_y_percent, max_x_percent,
                   max_y_percent);
  create_grid_cross(overlay->lines, min_x_percent, min_y_percent, max_x_percent,
                    max_y_percent);
  move_text_to_latest_cross(overlay->texts, min_x_percent, min_y_percent,
                            max_x_percent, max_y_percent);
  pthread_mutex_unlock(&overlay->scene_mutex);

  double center_x_percent = middle(min_x_percent, max_x_percent);
  double center_y_percent = middle(min_y_percent, max_y_percent);
  overlay_redraw();
  if (pointer_input_move_to_overlay_percent(center_x_percent,
                                            center_y_percent) < 0)
    fprintf(stderr, "Could not move pointer to the focused split\n");
}

static int focus_quadrant(struct input_state *state, bool right, bool bottom) {
  struct overlay_state *overlay = state->overlay;
  OverlayRect last_rect = overlay->rects->data[overlay->rects->len - 1];
  double half_width = last_rect.width_percent / 2;
  double half_height = last_rect.height_percent / 2;
  double min_x = last_rect.p.x_percent + (right ? half_width : 0);
  double max_x =
      last_rect.p.x_percent + (right ? last_rect.width_percent : half_width);
  double min_y = last_rect.p.y_percent + (bottom ? half_height : 0);
  double max_y =
      last_rect.p.y_percent + (bottom ? last_rect.height_percent : half_height);

  focus_split(overlay, min_x, min_y, max_x, max_y);
  return 0;
}

static int focus_top_left_split(struct input_state *state) {
  return focus_quadrant(state, false, false);
}

static int focus_top_right_split(struct input_state *state) {
  return focus_quadrant(state, true, false);
}

static int focus_bottom_left_split(struct input_state *state) {
  return focus_quadrant(state, false, true);
}

static int focus_bottom_right_split(struct input_state *state) {
  return focus_quadrant(state, true, true);
}

static int setup_grid_mode(struct input_state *state) {
  static const MacroEvent macros[] = {
      {KEY_F, 1, focus_top_left_split},
      {KEY_J, 1, focus_top_right_split},
      {KEY_D, 1, focus_bottom_left_split},
      {KEY_K, 1, focus_bottom_right_split},
      {KEY_CAPSLOCK, 1, input_stop_action},
      {KEY_ESC, 1, input_stop_action},
  };

  for (size_t i = 0; i < sizeof macros / sizeof macros[0]; i++)
    MacroEventArray_push(state->events->macro_events, macros[i]);

  state->overlay->base_background_color = (struct rgba){0.0, 0.0, 0.0, 0.5};
  state->overlay->is_color_secondary = false;
  create_grid_rect(state->overlay->rects, 0, 0, 100, 100);
  create_grid_cross(state->overlay->lines, 0, 0, 100, 100);
  move_text_to_latest_cross(state->overlay->texts, 0, 0, 100, 100);
  return 0;
}

int grid_mode_four_way_rec_split(void) {
  return run_mode("/dev/input/event3", setup_grid_mode, true);
}
