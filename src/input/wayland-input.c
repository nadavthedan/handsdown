#include "input.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
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

static struct libevdev_uinput *virtual_device;
static volatile sig_atomic_t stopping;

static void on_signal(int sig) {
  (void)sig;
  stopping = 1;
}

int emit(unsigned int type, unsigned int code, int value) {
  return libevdev_uinput_write_event(virtual_device, type, code, value);
}
int sync_output(void) { return emit(EV_SYN, SYN_REPORT, 0); }

static int held_event_index(unsigned int code, HeldEventArray *events) {
  if (events == NULL) {
    return -1;
  }
  for (int i = 0; i < events->len; i++) {
    if (code == events->data[i].key)
      return i;
  }
  return -1;
}
static int macro_event_index(unsigned int code, MacroEventArray *events) {
  if (events == NULL) {
    return -1;
  }
  for (int i = 0; i < events->len; i++) {
    if (code == events->data[i].key)
      return i;
  }
  return -1;
}
static int button_event_index(unsigned int code, ButtonEventArray *events) {
  if (events == NULL) {
    return -1;
  }
  for (int i = 0; i < events->len; i++) {
    if (code == events->data[i].key)
      return i;
  }
  return -1;
}

#define KEY_RELEASE_TIMEOUT_MS 2000
#define KEY_POLL_INTERVAL_MS 10

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

static int execute_held_actions(const struct input_event *event,
                                struct input_events *events) {
  int index = held_event_index(event->code, events->held_events);
  if (index < 0)
    return 0;

  HeldEvent *held = &events->held_events->data[index];
  if (event->value == 1)
    held->active = true;
  else if (event->value == 0)
    held->active = false;

  return 1;
}

static int execute_button_actions(const struct input_event *event,
                                  struct input_events *events) {
  int index = button_event_index(event->code, events->button_events);
  if (index < 0)
    return 0;

  ButtonEvent *button = &events->button_events->data[index];
  if (event->value == 1) {
    if (emit(EV_KEY, button->button, 1) < 0)
      return -1;
  } else if (event->value == 0) {
    if (emit(EV_KEY, button->button, 0) < 0)
      return -1;
  }

  return sync_output() < 0 ? -1 : 1;
}

static int execute_macro_actions(const struct input_event *event,
                                 struct input_events *events) {
  int index = macro_event_index(event->code, events->macro_events);
  if (index < 0)
    return 0;

  return events->macro_events->data[index].action() < 0 ? -1 : 1;
}

static int handle_event(const struct input_event *event,
                        struct input_events *events) {
  if (event->type == EV_SYN)
    return emit(event->type, event->code, event->value);

  if (event->type != EV_KEY)
    return emit(event->type, event->code, event->value);

  int handled = execute_held_actions(event, events);
  if (handled < 0)
    return -1;
  if (handled > 0)
    return 0;

  handled = execute_button_actions(event, events);
  if (handled < 0)
    return -1;
  if (handled > 0)
    return 0;

  if (execute_macro_actions(event, events) < 0)
    return -1;

  return emit(event->type, event->code, event->value);
}

static int held_actions(struct input_events *ev) {
  if (ev->held_events == NULL)
    return -1;
  for (int i = 0; i < ev->held_events->len; i++) {
    HeldEvent held = ev->held_events->data[i];
    if (held.active) {
      int ret = held.interval_action();
      if (ret < 0)
        return ret;
    }
  }
  return 0;
}

int virtual_keyboard_override(char *physical_keyboard_device_path,
                              struct input_events *events) {
  printf("start\n");
  int keyboard_fd =
      open(physical_keyboard_device_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (keyboard_fd < 0) {
    fprintf(stderr, "failed to open keyboard device\n");
    return -1;
  }

  struct libevdev *input = NULL;
  int rc = libevdev_new_from_fd(keyboard_fd, &input);
  if (rc < 0) {
    fprintf(stderr, "libevdev_new_from_fd: %s\n", strerror(-rc));
    close(keyboard_fd);
    return 1;
  }

  if (libevdev_enable_event_type(input, EV_REL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_X, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_REL, REL_Y, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_LEFT, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_RIGHT, NULL) < 0 ||
      libevdev_enable_event_code(input, EV_KEY, BTN_MIDDLE, NULL) < 0 ||
      libevdev_enable_property(input, INPUT_PROP_POINTER) < 0) {
    fprintf(stderr, "Could not add mouse capabilities\n");
    libevdev_free(input);
    close(keyboard_fd);
    return 1;
  }

  rc = libevdev_uinput_create_from_device(input, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          &virtual_device);
  if (rc < 0) {
    fprintf(stderr, "Could not create uinput device: %s\n", strerror(-rc));
    libevdev_free(input);
    close(keyboard_fd);
    return 1;
  }

  struct sigaction sa = {.sa_handler = on_signal};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  if (grab_keyboard(keyboard_fd, input) < 0) {
    libevdev_uinput_destroy(virtual_device);
    libevdev_free(input);
    close(keyboard_fd);
    return -1;
  }

  fprintf(stderr, "Ctrl-C exits.\n");

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
        if (handle_event(&ev, events) < 0) {
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

    if (held_actions(events) < 0) {
      fprintf(stderr, "Could not emit mouse movement\n");
      break;
    }
  }

  libevdev_grab(input, LIBEVDEV_UNGRAB);
  libevdev_uinput_destroy(virtual_device);
  libevdev_free(input);
  close(keyboard_fd);
  return 0;
}

void stop_virtual_keyboard_override(void) { stopping = 1; }
