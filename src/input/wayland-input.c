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

static bool mode_on;

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

static int set_mode(bool enable, struct input_events *ev) {
  mode_on = enable;

  if (enable) {
    for (int i = 0; i < ev->held_events->len; i++) {
      HeldEvent *held = &ev->held_events->data[i];

      // If key was forwarded as down, release it on the virtual keyboard so the
      // focused app own't leave it stuck.
      if (held->forwarded_down) {
        if (emit(EV_KEY, held->key, 0) < 0) {
          return -1;
        }
        held->forwarded_down = false;
      }

      // If held when mode starts, use it for movement and eat its release.
      held->supress_until_release = held->physical_down;
      held->active = false;
    }
    return sync_output();
  }

  // Keep supress_until_release set until any held movement key is released.
  return 0;
}

static int handle_event(const struct input_event *event,
                        struct input_events *events) {
  if (event->type == EV_SYN)
    return emit(event->type, event->code, event->value);

  if (event->type != EV_KEY)
    return emit(event->type, event->code, event->value);

  if (event->code == KEY_F12) {
    if (event->value == 1 && set_mode(!mode_on, events) < 0) {
      return -1;
    }
    return 0;
  }

  int i = held_event_index(event->code, events->held_events);
  if (i >= 0) {
    HeldEvent *held = &events->held_events->data[i];
    if (event->value == 1)
      held->physical_down = true;
    if (event->value == 0)
      held->physical_down = false;

    if (mode_on) {
      if (event->value == 1) {
        held->active = true;
        held->supress_until_release = true;
      } else if (event->value == 0) {
        held->active = false;
        held->supress_until_release = false;
      }
      return 0;
    }

    if (held->supress_until_release) {
      if (event->value == 0)
        held->supress_until_release = false;
      return 0; // Eats release for keys consumed in mouse mode.
    }

    if (event->value == 1) {
      held->forwarded_down = true;
    } else if (event->value == 0) {
      held->forwarded_down = false;
    }
  }

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

int virtual_device_init(char *physical_keyboard_device_path,
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

  if (ioctl(keyboard_fd, EVIOCGRAB, 1) < 0) {
    fprintf(stderr, "EVIOCGRAB: %s\n", strerror(errno));
    libevdev_uinput_destroy(virtual_device);
    libevdev_free(input);
    close(keyboard_fd);
    return -1;
  }

  fprintf(stderr, "F12 toggles mouse mode, Ctrl-C exits.\n");

  struct sigaction sa = {.sa_handler = on_signal};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  struct pollfd pfd = {.fd = keyboard_fd, .events = POLLIN};

  while (!stopping) {
    int pr = poll(&pfd, 1, 10);
    if (pr < 0 && errno != EINTR) {
      fprintf(stderr, "poll");
      break;
    }

    if (pr > 0 && (pfd.revents & POLLIN)) {
      for (;;) {
        struct input_event ev;
        rc = libevdev_next_event(input, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (rc == -EAGAIN)
          break;
        if (rc != 0) {
          fprintf(stderr, "Input read error (libevdev status %d)\n", rc);
          stopping = 1;
          break;
        }
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

    if (mode_on && held_actions(events) < 0) {
      fprintf(stderr, "Could not emit mouse movement\n");
      break;
    }
  }

  for (int i = 0; i < events->held_events->len; i++) {
    HeldEvent held = events->held_events->data[i];
    if (held.forwarded_down) {
      emit(EV_KEY, held.key, 0);
    }
  }
  sync_output();

  ioctl(keyboard_fd, EVIOCGRAB, 0);
  libevdev_uinput_destroy(virtual_device);
  libevdev_free(input);
  close(keyboard_fd);
  return 0;
}
