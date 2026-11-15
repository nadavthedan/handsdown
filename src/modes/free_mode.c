#include "input.h"
#include <linux/input-event-codes.h>
#include <stdio.h>

static int up_interval_action(void) {
  if (emit(EV_REL, REL_Y, -1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int left_interval_action(void) {
  if (emit(EV_REL, REL_X, -1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int down_interval_action(void) {
  if (emit(EV_REL, REL_Y, 1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int right_interval_action(void) {
  if (emit(EV_REL, REL_X, 1 * 3) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int exit_macro(void) {
  stop_virtual_keyboard_override();
  return 0;
}

static int free_mode_destroy(struct input_events *ev) {
  if (ev->held_events) {
    free(ev->held_events->data);
    free(ev->held_events);
  }

  if (ev->macro_events) {
    free(ev->macro_events->data);
    free(ev->macro_events);
  }

  free(ev);

  return 0;
}

int free_mode(void) {
  struct input_events *ev = calloc(1, sizeof(struct input_events));
  HeldEventArray *held = calloc(1, sizeof(HeldEventArray));

  HeldEvent up = {KEY_W, false, false, false, false, up_interval_action};
  HeldEvent left = {KEY_A, false, false, false, false, left_interval_action};
  HeldEvent down = {KEY_S, false, false, false, false, down_interval_action};
  HeldEvent right = {KEY_D, false, false, false, false, right_interval_action};

  HeldEventArray_push(held, up);
  HeldEventArray_push(held, left);
  HeldEventArray_push(held, down);
  HeldEventArray_push(held, right);

  MacroEventArray *macro = calloc(1, sizeof(MacroEventArray));

  MacroEvent exit1 = {KEY_ESC, 1, exit_macro};
  MacroEvent exit2 = {KEY_CAPSLOCK, 1, exit_macro};

  MacroEventArray_push(macro, exit1);
  MacroEventArray_push(macro, exit2);

  ev->macro_events = macro;
  ev->held_events = held;

  int err = virtual_keyboard_override("/dev/input/event3", ev);
  if (err) {
    fprintf(stderr, "Free mode failed to initialize virtual device\n");
  }
  free_mode_destroy(ev);
  return 0;
}
