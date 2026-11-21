#include "free_mode.h"
#include "input.h"
#include <linux/input-event-codes.h>

static int movement_speed(int base_speed, struct input_modifiers *mods) {
  if (mods->shift)
    return base_speed * 2;
  if (mods->alt) {
    int slowed = base_speed / 4;
    return slowed ? slowed : 1;
  }
  return base_speed;
}

static int scroll_tick(void) {
  static int counter;
  return ++counter % 5 == 0;
}

static int emit_scroll(unsigned int code, int direction) {
  if (emit(EV_REL, code, direction * scroll_tick()) < 0 || sync_output() < 0)
    return -1;
  return 0;
}

static int scroll_up_action(struct input_state *state) {
  (void)state;
  return emit_scroll(REL_WHEEL, 1);
}

static int scroll_down_action(struct input_state *state) {
  (void)state;
  return emit_scroll(REL_WHEEL, -1);
}

static int scroll_left_action(struct input_state *state) {
  (void)state;
  return emit_scroll(REL_HWHEEL, -1);
}

static int scroll_right_action(struct input_state *state) {
  (void)state;
  return emit_scroll(REL_HWHEEL, 1);
}

static int move_action(unsigned int axis, int direction,
                       struct input_state *state) {
  if (emit(EV_REL, axis, direction * movement_speed(4, state->mods)) < 0 ||
      sync_output() < 0)
    return -1;
  return 0;
}

static int up_interval_action(struct input_state *state) {
  return move_action(REL_Y, -1, state);
}

static int left_interval_action(struct input_state *state) {
  return move_action(REL_X, -1, state);
}

static int down_interval_action(struct input_state *state) {
  return move_action(REL_Y, 1, state);
}

static int right_interval_action(struct input_state *state) {
  return move_action(REL_X, 1, state);
}

static const OverlayRect hint_rects[] = {
    {{40, 10}, 20, 4, {255, 0, 132, 0.1}, {240, 40, 154, 0.3}},
};

static const OverlayLine hint_lines[] = {
    {{40, 10}, {40, 14}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{40, 10}, {60, 10}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{60, 10}, {60, 14}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{40, 14}, {60, 14}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{0, 0}, {0, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{0, 0}, {100, 0}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{100, 0}, {100, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
    {{0, 100}, {100, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}},
};

static const OverlayText hint_texts[] = {
    {{50, 12}, "Free Mode", 9, 15, {0, 0, 0, 1}, {255, 255, 255, 1}},
};

static int setup_free_mode(struct input_state *state) {
  static const HeldEvent held[] = {
      {KEY_E, false, up_interval_action},
      {KEY_S, false, left_interval_action},
      {KEY_D, false, down_interval_action},
      {KEY_F, false, right_interval_action},
      {KEY_K, false, scroll_up_action},
      {KEY_J, false, scroll_down_action},
      {KEY_H, false, scroll_left_action},
      {KEY_L, false, scroll_right_action},
  };
  static const MacroEvent macros[] = {
      {KEY_ESC, 1, input_stop_action},
      {KEY_CAPSLOCK, 1, input_stop_action},
  };
  static const ButtonEvent buttons[] = {
      {KEY_SPACE, BTN_LEFT},
  };

  for (size_t i = 0; i < sizeof held / sizeof held[0]; i++)
    HeldEventArray_push(state->events->held_events, held[i]);
  for (size_t i = 0; i < sizeof macros / sizeof macros[0]; i++)
    MacroEventArray_push(state->events->macro_events, macros[i]);
  for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++)
    ButtonEventArray_push(state->events->button_events, buttons[i]);

  state->overlay->base_background_color = (struct rgba){0, 0, 0, 0};
  for (size_t i = 0; i < sizeof hint_rects / sizeof hint_rects[0]; i++)
    OverlayRectArray_push(state->overlay->rects, hint_rects[i]);
  for (size_t i = 0; i < sizeof hint_lines / sizeof hint_lines[0]; i++)
    OverlayLineArray_push(state->overlay->lines, hint_lines[i]);
  for (size_t i = 0; i < sizeof hint_texts / sizeof hint_texts[0]; i++)
    OverlayTextArray_push(state->overlay->texts, hint_texts[i]);
  return 0;
}

int free_mode(void) {
  return run_mode("/dev/input/event3", setup_free_mode, false);
}
