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

static int free_mode_destroy(struct input_events *ev) {
  free(ev->held_events->data);
  free(ev->held_events);

  free(ev->macro_events->data);
  free(ev->macro_events);

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

  ev->macro_events = NULL;
  ev->held_events = held;

  int err = virtual_device_init("/dev/input/event3", ev);
  if (err) {
    fprintf(stderr, "Free mode failed to initialize virtual device\n");
  }
  free_mode_destroy(ev);
  return 0;
}
