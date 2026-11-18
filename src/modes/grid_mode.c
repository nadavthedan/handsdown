#include "grid_mode.h"
#include "input.h"
#include "overlay.h"
#include "utils.h"
#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

static void move_text_to_latest_cross(OverlayTextArray *texts,
                                      double min_x_percent,
                                      double min_y_percent,
                                      double max_x_percent,
                                      double max_y_percent) {
  double left_horizontal_middle =
      middle(min_x_percent, middle(min_x_percent, max_x_percent));
  double right_horizontal_middle =
      middle(middle(min_x_percent, max_x_percent), max_x_percent);

  double top_vertical_middle =
      middle(min_y_percent, middle(min_y_percent, max_y_percent));
  double bottom_vertical_middle =
      middle(middle(min_y_percent, max_y_percent), max_y_percent);

  struct rgba black = {0, 0, 0, 1};
  struct rgba white = {255, 255, 255, 1};

  OverlayText top_left = {
      {left_horizontal_middle, top_vertical_middle}, "f", 1, 18, white, black};
  OverlayText top_right = {
      {right_horizontal_middle, top_vertical_middle}, "j", 1, 18, white, black};
  OverlayText bottom_left = {{left_horizontal_middle, bottom_vertical_middle},
                             "d",
                             1,
                             20,
                             white,
                             black};
  OverlayText bottom_right = {{right_horizontal_middle, bottom_vertical_middle},
                              "k",
                              1,
                              20,
                              white,
                              black};

  if (texts->len == 0) {
    OverlayTextArray_push(texts, top_left);
    OverlayTextArray_push(texts, top_right);
    OverlayTextArray_push(texts, bottom_left);
    OverlayTextArray_push(texts, bottom_right);
  } else {
    assert(texts->len == 4);
    texts->data[0] = top_left;
    texts->data[1] = top_right;
    texts->data[2] = bottom_left;
    texts->data[3] = bottom_right;
  }
}

static void focus_split_confinments(struct overlay_state *overlay,
                                    double min_x_percent, double min_y_percent,
                                    double max_x_percent,
                                    double max_y_percent) {
  create_grid_rect(overlay->rects, min_x_percent, min_y_percent, max_x_percent,
                   max_y_percent);
  create_grid_cross(overlay->lines, min_x_percent, min_y_percent, max_x_percent,
                    max_y_percent);
  move_text_to_latest_cross(overlay->texts, min_x_percent, min_y_percent,
                            max_x_percent, max_y_percent);
}

static int focus_top_left_split(struct input_state *state) {
  struct overlay_state *overlay = state->overlay;
  OverlayRect last_rect = overlay->rects->data[overlay->rects->len - 1];
  focus_split_confinments(overlay, last_rect.p.x_percent, last_rect.p.y_percent,
                          last_rect.p.x_percent + (last_rect.width_percent / 2),
                          last_rect.p.y_percent +
                              (last_rect.height_percent / 2));

  overlay_redraw();

  return 0;
}

static int focus_top_right_split(struct input_state *state) {
  struct overlay_state *overlay = state->overlay;
  OverlayRect last_rect = overlay->rects->data[overlay->rects->len - 1];
  focus_split_confinments(
      overlay, last_rect.p.x_percent + (last_rect.width_percent) / 2,
      last_rect.p.y_percent, last_rect.p.x_percent + last_rect.width_percent,
      last_rect.p.y_percent + (last_rect.height_percent / 2));

  overlay_redraw();

  return 0;
}

static int focus_bottom_left_split(struct input_state *state) {
  struct overlay_state *overlay = state->overlay;
  OverlayRect last_rect = overlay->rects->data[overlay->rects->len - 1];
  focus_split_confinments(overlay, last_rect.p.x_percent,
                          last_rect.p.y_percent +
                              (last_rect.height_percent / 2),
                          last_rect.p.x_percent + (last_rect.width_percent / 2),
                          last_rect.p.y_percent + last_rect.height_percent);

  overlay_redraw();

  return 0;
}
static int focus_bottom_right_split(struct input_state *state) {
  struct overlay_state *overlay = state->overlay;
  OverlayRect last_rect = overlay->rects->data[overlay->rects->len - 1];
  focus_split_confinments(
      overlay, last_rect.p.x_percent + (last_rect.width_percent) / 2,
      last_rect.p.y_percent + (last_rect.height_percent) / 2,
      last_rect.p.x_percent + last_rect.width_percent,
      last_rect.p.y_percent + last_rect.height_percent);

  overlay_redraw();

  return 0;
}

struct overlay_state *
grid_overlay_fourway_split_init(struct overlay_state *overlay) {
  overlay->base_background_color = (struct rgba){0.0, 0.0, 0.0, 0.5};
  overlay->is_color_secondary = false;
  overlay->key_down_handler = NULL;
  overlay->key_release_handler = NULL;

  OverlayRectArray *rects = calloc(1, sizeof(OverlayRectArray));
  OverlayLineArray *lines = calloc(1, sizeof(OverlayLineArray));
  OverlayTextArray *texts = calloc(1, sizeof(OverlayTextArray));

  create_grid_rect(rects, 0, 0, 100, 100);
  create_grid_cross(lines, 0, 0, 100, 100);
  move_text_to_latest_cross(texts, 0, 0, 100, 100);

  overlay->rects = rects;
  overlay->lines = lines;
  overlay->texts = texts;

  return overlay;
}

static int grid_overlay_fourway_split_destroy(struct input_state *state) {
  if (!state) {
    return 0;
  }
  if (state->events) {
    if (state->events->held_events) {
      free(state->events->held_events->data);
      free(state->events->held_events);
    }

    if (state->events->macro_events) {
      free(state->events->macro_events->data);
      free(state->events->macro_events);
    }

    if (state->events->button_events) {
      free(state->events->button_events->data);
      free(state->events->button_events);
    }
  }

  if (state->overlay) {
    if (state->overlay->rects) {
      free(state->overlay->rects->data);
      free(state->overlay->rects);
    }
    if (state->overlay->lines) {
      free(state->overlay->lines->data);
      free(state->overlay->lines);
    }
    if (state->overlay->texts) {
      free(state->overlay->texts->data);
      free(state->overlay->texts);
    }
  }

  free(state->events);
  free(state->overlay);
  free(state->mods);

  free(state);

  return 0;
}

static void *overlay_thread_fn(void *arg) {
  overlay_create((struct overlay_state *)arg);
  return NULL;
}

int grid_mode_four_way_rec_split(void) {
  int err;

  struct input_state *state = calloc(1, sizeof(struct input_state));
  struct input_events *events = calloc(1, sizeof(struct input_events));

  MacroEventArray *macros = calloc(1, sizeof(MacroEventArray));

  MacroEvent top_left = {KEY_F, 1, focus_top_left_split};
  MacroEvent top_right = {KEY_J, 1, focus_top_right_split};
  MacroEvent bottom_left = {KEY_D, 1, focus_bottom_left_split};
  MacroEvent bottom_right = {KEY_K, 1, focus_bottom_right_split};

  MacroEventArray_push(macros, top_left);
  MacroEventArray_push(macros, top_right);
  MacroEventArray_push(macros, bottom_left);
  MacroEventArray_push(macros, bottom_right);

  events->macro_events = macros;
  events->held_events = NULL;
  events->button_events = NULL;

  struct input_modifiers *modifiers = calloc(1, sizeof(struct input_modifiers));

  struct overlay_state *overlay_state = calloc(1, sizeof(struct overlay_state));
  grid_overlay_fourway_split_init(overlay_state);

  state->events = events;
  state->mods = modifiers;
  state->overlay = overlay_state;

  pthread_t overlay_thread;
  if (pthread_create(&overlay_thread, NULL, overlay_thread_fn,
                     state->overlay) != 0) {
    fprintf(stderr, "Overlay thread creation failed\n");
    grid_overlay_fourway_split_destroy(state);
    return -1;
  }

  err = virtual_keyboard_override("/dev/input/event3", state);
  if (err) {
    fprintf(stderr, "Free mode failed to initialize virtual device\n");
  }

  overlay_stop();
  pthread_join(overlay_thread, NULL);
  grid_overlay_fourway_split_destroy(state);
  return 0;
}
