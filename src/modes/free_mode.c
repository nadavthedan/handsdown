#include "input.h"
#include "overlay.h"
#include <linux/input-event-codes.h>
#include <pthread.h>
#include <stdio.h>

static int movement_speed(int base_speed, struct input_modifiers *mods) {
  if (mods->shift) {
    return base_speed * 2;
  } else if (mods->alt) {
    return base_speed / 4 || 1;
  }
  return base_speed;
}

static int tick_counter = 0;
static int scroll_tick(int tick_wait) {
  tick_counter++;
  if (tick_counter % tick_wait == 0)
    return 1;

  return 0;
}

static int scroll_up_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_WHEEL, 1 * scroll_tick(5)) < 0 || sync_output() < 0)
    return -1;

  return 0;
}
static int scroll_down_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_WHEEL, -1 * scroll_tick(5)) < 0 || sync_output() < 0)
    return -1;

  return 0;
}
static int scroll_left_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_HWHEEL, -1 * scroll_tick(5)) < 0 || sync_output() < 0)
    return -1;

  return 0;
}
static int scroll_right_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_HWHEEL, 1 * scroll_tick(5)) < 0 || sync_output() < 0)
    return -1;

  return 0;
}

static int up_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_Y, -1 * movement_speed(4, state->mods)) < 0 ||
      sync_output() < 0)
    return -1;

  return 0;
}

static int left_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_X, -1 * movement_speed(4, state->mods)) < 0 ||
      sync_output() < 0)
    return -1;

  return 0;
}

static int down_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_Y, 1 * movement_speed(4, state->mods)) < 0 ||
      sync_output() < 0)
    return -1;

  return 0;
}

static int right_interval_action(struct input_state *state) {
  (void)state;
  if (emit(EV_REL, REL_X, 1 * movement_speed(4, state->mods)) < 0 ||
      sync_output() < 0)
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

static void *overlay_thread_fn(void *arg) {
  overlay_create((struct overlay_state *)arg);
  return NULL;
}

static void setup_free_mode_overlay_hint(struct overlay_state *overlay_state) {
  OverlayRectArray *rects = calloc(1, sizeof(OverlayRectArray));
  OverlayRect title_background = {
      {40, 10}, 20, 4, {255, 0, 132, 0.1}, {240, 40, 154, 0.3}};
  OverlayRectArray_push(rects, title_background);

  OverlayLineArray *lines = calloc(1, sizeof(OverlayLineArray));
  OverlayLine left = {{40, 10}, {40, 14}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine top = {{40, 10}, {60, 10}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine right = {{60, 10}, {60, 14}, 2, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine bottom = {
      {40, 14}, {60, 14}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine left_wall = {
      {0, 0}, {0, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine top_wall = {
      {0, 0}, {100, 0}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine right_wall = {
      {100, 0}, {100, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}};
  OverlayLine bottom_wall = {
      {0, 100}, {100, 100}, 4, {255, 0, 132, 0.5}, {0, 0, 0, 1}};

  OverlayLineArray_push(lines, left);
  OverlayLineArray_push(lines, top);
  OverlayLineArray_push(lines, right);
  OverlayLineArray_push(lines, bottom);
  OverlayLineArray_push(lines, left_wall);
  OverlayLineArray_push(lines, top_wall);
  OverlayLineArray_push(lines, right_wall);
  OverlayLineArray_push(lines, bottom_wall);

  OverlayTextArray *texts = calloc(1, sizeof(OverlayTextArray));
  OverlayText title_text = {{50, 12}, "Free Mode",  9,
                            15,       {0, 0, 0, 1}, {255, 255, 255, 1}};

  OverlayTextArray_push(texts, title_text);

  overlay_state->base_background_color = (struct rgba){0, 0, 0, 0};
  overlay_state->rects = rects;
  overlay_state->lines = lines;
  overlay_state->texts = texts;
}

int free_mode(void) {
  int err;

  struct input_state *state = calloc(1, sizeof(struct input_state));
  struct input_events *events = calloc(1, sizeof(struct input_events));
  HeldEventArray *held = calloc(1, sizeof(HeldEventArray));

  HeldEvent up = {KEY_W, false, up_interval_action};
  HeldEvent left = {KEY_A, false, left_interval_action};
  HeldEvent down = {KEY_S, false, down_interval_action};
  HeldEvent right = {KEY_D, false, right_interval_action};
  HeldEvent scroll_up = {KEY_K, false, scroll_up_action};
  HeldEvent scroll_down = {KEY_J, false, scroll_down_action};
  HeldEvent scroll_left = {KEY_H, false, scroll_left_action};
  HeldEvent scroll_right = {KEY_L, false, scroll_right_action};

  HeldEventArray_push(held, up);
  HeldEventArray_push(held, left);
  HeldEventArray_push(held, down);
  HeldEventArray_push(held, right);
  HeldEventArray_push(held, scroll_up);
  HeldEventArray_push(held, scroll_down);
  HeldEventArray_push(held, scroll_left);
  HeldEventArray_push(held, scroll_right);

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

  setup_free_mode_overlay_hint(overlay_state);

  state->events = events;
  state->mods = modifiers;
  state->overlay = overlay_state;

  pthread_t overlay_thread;
  if (pthread_create(&overlay_thread, NULL, overlay_thread_fn,
                     state->overlay) != 0) {
    fprintf(stderr, "Overlay thread creation failed\n");
    free_mode_destroy(state);
    return -1;
  }

  err = virtual_keyboard_override("/dev/input/event3", state);
  if (err) {
    fprintf(stderr, "Free mode failed to initialize virtual device\n");
  }

  overlay_stop();
  pthread_join(overlay_thread, NULL);
  free_mode_destroy(state);
  return 0;
}
