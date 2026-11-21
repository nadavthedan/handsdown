#include "input.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#define ABSOLUTE_POINTER_AXIS_MAX 32767
#define POINTER_MOVE_TIMEOUT_MS 500
#define POINTER_POSITION_TOLERANCE_PX 2.0
#define POINTER_NUDGE_PX 8.0
#define POINTER_CALIBRATION_ATTEMPTS 3
#define POINTER_CALIBRATION_RETRY_DELAY_MS 300
#define KEY_RELEASE_TIMEOUT_MS 2000
#define KEY_POLL_INTERVAL_MS 10

static struct libevdev_uinput *virtual_device;
static struct libevdev_uinput *absolute_pointer_device;
static volatile sig_atomic_t stopping;

static void on_signal(int sig) {
  (void)sig;
  stopping = 1;
}

int emit(unsigned int type, unsigned int code, int value) {
  return libevdev_uinput_write_event(virtual_device, type, code, value);
}
int sync_output(void) { return emit(EV_SYN, SYN_REPORT, 0); }

int input_stop_action(struct input_state *state) {
  (void)state;
  stop_virtual_keyboard_override();
  return 0;
}

static int absolute_pointer_init(void) {
  if (absolute_pointer_device)
    return 0;

  struct libevdev *device = libevdev_new();
  if (!device)
    return -1;

  struct input_absinfo abs_x = {.value = 0,
                                .minimum = 0,
                                .maximum = ABSOLUTE_POINTER_AXIS_MAX,
                                .fuzz = 0,
                                .flat = 0,
                                .resolution = 1};
  struct input_absinfo abs_y = abs_x;

  libevdev_set_name(device, "handsdown absolute pointer");

  int rc;
  if ((rc = libevdev_enable_event_type(device, EV_ABS)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_ABS, ABS_X, &abs_x)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_ABS, ABS_Y, &abs_y)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_KEY, BTN_LEFT, NULL)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_KEY, BTN_RIGHT, NULL)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_KEY, BTN_MIDDLE, NULL)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_REL, REL_WHEEL, NULL)) < 0 ||
      (rc = libevdev_enable_event_code(device, EV_REL, REL_HWHEEL, NULL)) < 0 ||
      (rc = libevdev_enable_property(device, INPUT_PROP_DIRECT)) < 0) {
    fprintf(stderr, "Could not configure absolute pointer: %s\n",
            strerror(-rc));
    libevdev_free(device);
    return -1;
  }

  rc = libevdev_uinput_create_from_device(device, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          &absolute_pointer_device);
  libevdev_free(device);
  if (rc < 0) {
    fprintf(stderr, "Could not create absolute pointer: %s\n", strerror(-rc));
    return -1;
  }

  return 0;
}

static int normalized_to_axis_value(double normalized) {
  if (!isfinite(normalized) || normalized <= 0.0)
    return 0;
  if (normalized >= 1.0)
    return ABSOLUTE_POINTER_AXIS_MAX;
  return (int)lround(normalized * ABSOLUTE_POINTER_AXIS_MAX);
}

static int write_absolute_pointer_move(int x, int y) {
  if (libevdev_uinput_write_event(absolute_pointer_device, EV_ABS, ABS_X, x) <
          0 ||
      libevdev_uinput_write_event(absolute_pointer_device, EV_ABS, ABS_Y, y) <
          0 ||
      libevdev_uinput_write_event(absolute_pointer_device, EV_SYN, SYN_REPORT,
                                  0) < 0) {
    return -1;
  }
  return 0;
}

static int absolute_pointer_move(double normalized_x, double normalized_y) {
  int x = normalized_to_axis_value(normalized_x);
  int y = normalized_to_axis_value(normalized_y);

  return write_absolute_pointer_move(x, y);
}

static void absolute_pointer_destroy(void) {
  if (!absolute_pointer_device)
    return;
  libevdev_uinput_destroy(absolute_pointer_device);
  absolute_pointer_device = NULL;
}

enum pointer_mapping_mode {
  POINTER_MAP_UNKNOWN,
  POINTER_MAP_DESKTOP,
  POINTER_MAP_MONITOR,
};

static enum pointer_mapping_mode active_pointer_mapping = POINTER_MAP_UNKNOWN;

int pointer_input_init(void) {
  active_pointer_mapping = POINTER_MAP_UNKNOWN;
  return absolute_pointer_init();
}

static void
local_to_normalized_position(const struct overlay_geometry *geometry,
                             enum pointer_mapping_mode mapping, double local_x,
                             double local_y, double *normalized_x,
                             double *normalized_y) {
  if (mapping == POINTER_MAP_MONITOR) {
    *normalized_x = local_x / geometry->width_px;
    *normalized_y = local_y / geometry->height_px;
    return;
  }

  *normalized_x = ((double)geometry->window_monitor_x + local_x -
                   geometry->monitors_bounds_x) /
                  geometry->monitors_bounds_width;
  *normalized_y = ((double)geometry->window_monitor_y + local_y -
                   geometry->monitors_bounds_y) /
                  geometry->monitors_bounds_height;
}

static bool pointer_position_matches(const struct overlay_pointer_position *p,
                                     double x, double y) {
  return fabs(p->x_px - x) <= POINTER_POSITION_TOLERANCE_PX &&
         fabs(p->y_px - y) <= POINTER_POSITION_TOLERANCE_PX;
}

static int probe_pointer_mapping(const struct overlay_geometry *geometry,
                                 enum pointer_mapping_mode mapping,
                                 double local_x, double local_y,
                                 bool force_motion) {
  double normalized_x, normalized_y;
  uint64_t token = overlay_pointer_arm_motion();

  if (force_motion) {
    double nudge_x = local_x + POINTER_NUDGE_PX;
    if (nudge_x > geometry->width_px)
      nudge_x = local_x - POINTER_NUDGE_PX;
    local_to_normalized_position(geometry, mapping, nudge_x, local_y,
                                 &normalized_x, &normalized_y);
    if (absolute_pointer_move(normalized_x, normalized_y) < 0)
      return -1;
  }

  local_to_normalized_position(geometry, mapping, local_x, local_y,
                               &normalized_x, &normalized_y);
  if (absolute_pointer_move(normalized_x, normalized_y) < 0)
    return -1;

  return overlay_pointer_wait_for_position(token, local_x, local_y,
                                           POINTER_POSITION_TOLERANCE_PX,
                                           POINTER_MOVE_TIMEOUT_MS)
             ? 0
             : -1;
}

static int calibrate_pointer_mapping(const struct overlay_geometry *geometry,
                                     double local_x, double local_y,
                                     bool force_motion) {
  const enum pointer_mapping_mode modes[] = {POINTER_MAP_DESKTOP,
                                             POINTER_MAP_MONITOR};

  for (int attempt = 0; attempt < POINTER_CALIBRATION_ATTEMPTS; attempt++) {
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
      if (probe_pointer_mapping(geometry, modes[i], local_x, local_y,
                                force_motion) == 0) {
        active_pointer_mapping = modes[i];
        return 0;
      }
    }
    if (attempt + 1 < POINTER_CALIBRATION_ATTEMPTS)
      usleep(POINTER_CALIBRATION_RETRY_DELAY_MS * 1000);
  }

  active_pointer_mapping = POINTER_MAP_UNKNOWN;
  return -1;
}

int pointer_input_move_to_overlay_percent(double x_percent, double y_percent) {
  struct overlay_snapshot snapshot;
  if (overlay_snapshot(&snapshot) < 0 || snapshot.geometry.width_px <= 0 ||
      snapshot.geometry.height_px <= 0 ||
      snapshot.geometry.monitors_bounds_width <= 0 ||
      snapshot.geometry.monitors_bounds_height <= 0) {
    fprintf(stderr, "Could not read overlay geometry\n");
    return -1;
  }

  const struct overlay_geometry *geometry = &snapshot.geometry;
  double local_x = geometry->width_px * x_percent / 100.0;
  double local_y = geometry->height_px * y_percent / 100.0;
  bool force_motion =
      pointer_position_matches(&snapshot.pointer, local_x, local_y);

  if (overlay_pointer_tracking_start() < 0) {
    fprintf(stderr, "Could not start pointer tracking\n");
    return -1;
  }

  int result;
  if (active_pointer_mapping == POINTER_MAP_UNKNOWN) {
    result =
        calibrate_pointer_mapping(geometry, local_x, local_y, force_motion);
  } else if (probe_pointer_mapping(geometry, active_pointer_mapping, local_x,
                                   local_y, force_motion) == 0) {
    result = 0;
  } else {
    active_pointer_mapping = POINTER_MAP_UNKNOWN;
    result =
        calibrate_pointer_mapping(geometry, local_x, local_y, force_motion);
  }

  if (overlay_pointer_tracking_stop() < 0)
    result = -1;
  return result;
}

void pointer_input_destroy(void) {
  active_pointer_mapping = POINTER_MAP_UNKNOWN;
  absolute_pointer_destroy();
}

static int held_event_index(unsigned int code, HeldEventArray *events) {
  if (events == NULL) {
    return -1;
  }
  for (size_t i = 0; i < events->len; i++) {
    if (code == events->data[i].key)
      return i;
  }
  return -1;
}
static int macro_event_index(const struct input_event *event,
                             MacroEventArray *macros) {
  if (macros == NULL) {
    return -1;
  }
  for (size_t i = 0; i < macros->len; i++) {
    MacroEvent macro = macros->data[i];
    if (event->code == macro.key && event->value == macro.value_to_trigger)
      return i;
  }
  return -1;
}
static int button_event_index(unsigned int code, ButtonEventArray *events) {
  if (events == NULL) {
    return -1;
  }
  for (size_t i = 0; i < events->len; i++) {
    if (code == events->data[i].key)
      return i;
  }
  return -1;
}

static int any_key_down(int fd, bool *down) {
  unsigned char keys[(KEY_CNT + 7) / 8];

  memset(keys, 0, sizeof keys);
  if (ioctl(fd, EVIOCGKEY(sizeof keys), keys) < 0)
    return -1;

  *down = false;
  for (size_t i = 0; i < sizeof keys; i++) {
    if (keys[i] != 0) {
      *down = true;
      break;
    }
  }
  return 0;
}

static int drain_events(struct libevdev *input) {
  struct input_event ev;
  unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;

  for (;;) {
    int rc = libevdev_next_event(input, flags, &ev);

    if (rc == -EAGAIN) {
      if (flags == LIBEVDEV_READ_FLAG_SYNC) {
        flags = LIBEVDEV_READ_FLAG_NORMAL;
        continue;
      }
      return 0;
    }

    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
      flags = LIBEVDEV_READ_FLAG_SYNC;
      continue;
    }

    if (rc < 0)
      return rc;
  }
}

static int wait_for_keys_released(int keyboard_fd, struct libevdev *input,
                                  int timeout_ms) {
  struct pollfd pfd = {.fd = keyboard_fd, .events = POLLIN};
  int waited = 0;

  for (;;) {
    bool down;
    if (any_key_down(keyboard_fd, &down) < 0) {
      fprintf(stderr, "EVIOCGKEY: %s\n", strerror(errno));
      return -1;
    }

    if (!down)
      break;

    if (waited >= timeout_ms) {
      fprintf(stderr, "Timed out waiting for all keys to be released\n");
      return -ETIMEDOUT;
    }

    int rc = drain_events(input);
    if (rc < 0) {
      fprintf(stderr, "Could not drain input events: %s\n", strerror(-rc));
      return rc;
    }

    int pr = poll(&pfd, 1, KEY_POLL_INTERVAL_MS);
    if (pr < 0 && errno != EINTR) {
      fprintf(stderr, "poll: %s\n", strerror(errno));
      return -1;
    }

    if (pr > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
      fprintf(stderr, "Keyboard device disconnected or errored\n");
      return -1;
    }

    waited += KEY_POLL_INTERVAL_MS;
  }

  return drain_events(input);
}

static int grab_keyboard(int keyboard_fd, struct libevdev *input) {
  for (;;) {
    int rc = wait_for_keys_released(keyboard_fd, input, KEY_RELEASE_TIMEOUT_MS);
    if (rc < 0)
      return rc;

    rc = libevdev_grab(input, LIBEVDEV_GRAB);
    if (rc < 0) {
      fprintf(stderr, "EVIOCGRAB: %s\n", strerror(-rc));
      return rc;
    }

    bool down;
    if (any_key_down(keyboard_fd, &down) < 0) {
      fprintf(stderr, "EVIOCGKEY: %s\n", strerror(errno));
      libevdev_grab(input, LIBEVDEV_UNGRAB);
      return -1;
    }

    if (!down)
      return 0;

    rc = libevdev_grab(input, LIBEVDEV_UNGRAB);
    if (rc < 0) {
      fprintf(stderr, "Could not release keyboard grab: %s\n", strerror(-rc));
      return rc;
    }
  }
}

static bool execute_held_actions(const struct input_event *event,
                                 HeldEventArray *held_events) {
  int index = held_event_index(event->code, held_events);
  if (index < 0)
    return false;

  HeldEvent *held = &held_events->data[index];
  if (event->value == 1)
    held->active = true;
  else if (event->value == 0)
    held->active = false;

  return true;
}

static bool execute_button_actions(const struct input_event *event,
                                   ButtonEventArray *button_events) {
  int index = button_event_index(event->code, button_events);
  if (index < 0)
    return false;

  ButtonEvent *button = &button_events->data[index];
  if (event->value == 1) {
    if (emit(EV_KEY, button->button, 1) < 0)
      return false;
  } else if (event->value == 0) {
    if (emit(EV_KEY, button->button, 0) < 0)
      return false;
  }

  return sync_output() >= 0;
}

static bool execute_macro_actions(const struct input_event *event,
                                  struct input_state *state) {
  int index = macro_event_index(event, state->events->macro_events);
  if (index < 0)
    return false;

  return state->events->macro_events->data[index].action(state) >= 0;
}

static bool toggle_modifiers(const struct input_event *event,
                             struct input_modifiers *mods) {
  switch (event->code) {
  case KEY_RIGHTSHIFT:
  case KEY_LEFTSHIFT:
    mods->shift = event->value;
    return true;
  case KEY_RIGHTALT:
  case KEY_LEFTALT:
    mods->alt = event->value;
    return true;
  case KEY_RIGHTCTRL:
  case KEY_LEFTCTRL:
    mods->ctrl = event->value;
    return true;
  }

  return false;
}

static int handle_event(const struct input_event *event,
                        struct input_state *state) {
  if (event->type != EV_KEY)
    return emit(event->type, event->code, event->value);

  toggle_modifiers(event, state->mods);

  bool event_handled = execute_held_actions(event, state->events->held_events);
  event_handled |= execute_button_actions(event, state->events->button_events);
  event_handled |= execute_macro_actions(event, state);

  return event_handled ? 0 : emit(event->type, event->code, event->value);
}

static int held_actions(struct input_state *state) {
  HeldEventArray *held_events = state->events->held_events;
  for (size_t i = 0; i < held_events->len; i++) {
    if (held_events->data[i].active &&
        held_events->data[i].interval_action(state) < 0)
      return -1;
  }
  return 0;
}

static void *overlay_thread_fn(void *arg) {
  overlay_create(arg);
  return NULL;
}

static void input_state_destroy(struct input_state *state) {
  if (!state)
    return;

  if (state->events) {
    HeldEventArray_free(state->events->held_events);
    MacroEventArray_free(state->events->macro_events);
    ButtonEventArray_free(state->events->button_events);
  }
  if (state->overlay) {
    OverlayRectArray_free(state->overlay->rects);
    OverlayLineArray_free(state->overlay->lines);
    OverlayTextArray_free(state->overlay->texts);
  }

  free(state->events);
  free(state->overlay);
  free(state->mods);
  free(state);
}

int run_mode(const char *keyboard_device_path, mode_setup_fn setup,
             bool use_absolute_pointer) {
  struct input_state *state = calloc(1, sizeof *state);
  if (!state)
    return -1;

  state->events = calloc(1, sizeof *state->events);
  state->mods = calloc(1, sizeof *state->mods);
  state->overlay = calloc(1, sizeof *state->overlay);
  if (!state->events || !state->mods || !state->overlay)
    goto fail;

  state->events->held_events = calloc(1, sizeof(HeldEventArray));
  state->events->macro_events = calloc(1, sizeof(MacroEventArray));
  state->events->button_events = calloc(1, sizeof(ButtonEventArray));
  state->overlay->rects = calloc(1, sizeof(OverlayRectArray));
  state->overlay->lines = calloc(1, sizeof(OverlayLineArray));
  state->overlay->texts = calloc(1, sizeof(OverlayTextArray));
  if (!state->events->held_events || !state->events->macro_events ||
      !state->events->button_events || !state->overlay->rects ||
      !state->overlay->lines || !state->overlay->texts)
    goto fail;

  if (setup(state) < 0)
    goto fail;

  if (use_absolute_pointer && pointer_input_init() < 0) {
    fprintf(stderr, "Could not initialize absolute pointer\n");
    goto fail;
  }

  pthread_t overlay_thread;
  if (pthread_create(&overlay_thread, NULL, overlay_thread_fn,
                     state->overlay) != 0) {
    fprintf(stderr, "Overlay thread creation failed\n");
    if (use_absolute_pointer)
      pointer_input_destroy();
    goto fail;
  }

  if (virtual_keyboard_override(keyboard_device_path, state) != 0)
    fprintf(stderr, "Could not initialize virtual device\n");

  if (use_absolute_pointer)
    pointer_input_destroy();
  overlay_stop();
  pthread_join(overlay_thread, NULL);
  input_state_destroy(state);
  return 0;

fail:
  input_state_destroy(state);
  return -1;
}

int virtual_keyboard_override(const char *physical_keyboard_device_path,
                              struct input_state *state) {
  int keyboard_fd =
      open(physical_keyboard_device_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (keyboard_fd < 0) {
    fprintf(stderr, "failed to open keyboard device\n");
    return -1;
  }

  struct libevdev *input = NULL;
  int result = 0;
  bool grabbed = false;
  int rc = libevdev_new_from_fd(keyboard_fd, &input);
  if (rc < 0) {
    fprintf(stderr, "libevdev_new_from_fd: %s\n", strerror(-rc));
    result = 1;
    goto cleanup;
  }

  if (libevdev_enable_event_type(input, EV_REL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_X, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_Y, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_WHEEL, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_HWHEEL, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_LEFT, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_RIGHT, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_MIDDLE, NULL) < 0 ||
      libevdev_enable_property(input, INPUT_PROP_POINTER) < 0) {
    fprintf(stderr, "Could not add mouse capabilities\n");
    result = 1;
    goto cleanup;
  }

  rc = libevdev_uinput_create_from_device(input, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          &virtual_device);
  if (rc < 0) {
    fprintf(stderr, "Could not create uinput device: %s\n", strerror(-rc));
    result = 1;
    goto cleanup;
  }

  struct sigaction sa = {.sa_handler = on_signal};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  if (grab_keyboard(keyboard_fd, input) < 0) {
    result = -1;
    goto cleanup;
  }
  grabbed = true;

  struct pollfd pfd = {.fd = keyboard_fd, .events = POLLIN};

  while (!stopping) {
    int pr = poll(&pfd, 1, 10);
    if (pr < 0 && errno != EINTR) {
      fprintf(stderr, "poll");
      break;
    }

    if (pr > 0 && (pfd.revents & POLLIN)) {
      unsigned int read_flags = LIBEVDEV_READ_FLAG_NORMAL;
      for (;;) {
        struct input_event ev;
        rc = libevdev_next_event(input, read_flags, &ev);

        if (rc == -EAGAIN) {
          read_flags = LIBEVDEV_READ_FLAG_NORMAL;
          break;
        }
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
          read_flags = LIBEVDEV_READ_FLAG_SYNC;
          continue;
        }
        if (rc != LIBEVDEV_READ_STATUS_SUCCESS) {
          fprintf(stderr, "Input read error (libevdev status %d)\n", rc);
          stopping = 1;
          break;
        }

        read_flags = LIBEVDEV_READ_FLAG_NORMAL;
        if (handle_event(&ev, state) < 0) {
          fprintf(stderr, "Could not forward input event\n");
          stopping = 1;
          break;
        }
      }
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      fprintf(stderr, "Keyboard device disconnected or errored\n");
      break;
    }

    if (held_actions(state) < 0) {
      fprintf(stderr, "Could not emit mouse movement\n");
      break;
    }
  }

cleanup:
  if (grabbed)
    libevdev_grab(input, LIBEVDEV_UNGRAB);
  if (virtual_device) {
    libevdev_uinput_destroy(virtual_device);
    virtual_device = NULL;
  }
  if (input)
    libevdev_free(input);
  close(keyboard_fd);
  return result;
}

void stop_virtual_keyboard_override(void) { stopping = 1; }
