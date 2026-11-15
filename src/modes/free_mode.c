#include "input.h"
#include "overlay.h"
#include <linux/input-event-codes.h>
#include <stdio.h>

static int up_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_Y, -1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int left_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_X, -1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int down_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_Y, 1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int right_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_X, 1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int exit_macro(struct input_state *state) {
  (void)state;
  stop_virtual_keyboard_override();
  return 0;
}

static int free_mode_destroy(struct input_state *state) {
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

int free_mode(void) {
  struct input_state *state = calloc(1, sizeof(struct input_state));
  struct input_events *events = calloc(1, sizeof(struct input_events));
  HeldEventArray *held = calloc(1, sizeof(HeldEventArray));

  HeldEvent up = {KEY_W, false, up_interval_action};
  HeldEvent left = {KEY_A, false, left_interval_action};
  HeldEvent down = {KEY_S, false, down_interval_action};
  HeldEvent right = {KEY_D, false, right_interval_action};

  HeldEventArray_push(held, up);
  HeldEventArray_push(held, left);
  HeldEventArray_push(held, down);
  HeldEventArray_push(held, right);

  MacroEventArray *macro = calloc(1, sizeof(MacroEventArray));

  MacroEvent exit1 = {KEY_ESC, 1, exit_macro};
  MacroEvent exit2 = {KEY_CAPSLOCK, 1, exit_macro};

  MacroEventArray_push(macro, exit1);
  MacroEventArray_push(macro, exit2);

  ButtonEventArray *buttons = calloc(1, sizeof(ButtonEventArray));
  ButtonEvent left_click = {KEY_SPACE, BTN_LEFT};
  ButtonEventArray_push(buttons, left_click);

  events->macro_events = macro;
  events->held_events = held;
  events->button_events = buttons;

  struct input_modifiers *modifiers = calloc(1, sizeof(struct input_modifiers));

  struct overlay_state *overlay_state = calloc(1, sizeof(struct overlay_state));

  overlay_state->base_background_color = (struct rgba){0, 0, 0, 0};

  state->events = events;
  state->mods = modifiers;
  state->overlay = overlay_state;

  int err = virtual_keyboard_override("/dev/input/event3", state);
  if (err) {
    fprintf(stderr, "Free mode failed to initialize virtual device\n");
  }
  free_mode_destroy(state);
  return 0;
}
