#include "input/input.h"
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void setup_abs_axit(int fd, int axis, int min, int max) {
  ioctl(fd, UI_SET_ABSBIT, axis);
  struct uinput_abs_setup abs_setup = {0};
  abs_setup.code = axis;
  abs_setup.absinfo.minimum = min;
  abs_setup.absinfo.maximum = max;
  abs_setup.absinfo.resolution = 1; // required by libinput
  ioctl(fd, UI_ABS_SETUP, &abs_setup);
}

static int abs_device_init(const int screen_w, const int screen_h) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    return fd;

  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  setup_abs_axit(fd, ABS_X, 0, screen_w);
  setup_abs_axit(fd, ABS_Y, 0, screen_h);

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
  ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_PEN);

  struct uinput_setup abs_setup = {0};
  abs_setup.id.bustype = BUS_USB;
  abs_setup.id.vendor = 0x1234;
  abs_setup.id.product = 0x5678;
  strcpy(abs_setup.name, "Virtual Abs Mouse");

  ioctl(fd, UI_DEV_SETUP, &abs_setup);
  ioctl(fd, UI_DEV_CREATE);

  return fd;
}

static int rel_device_init(void) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    return fd;

  ioctl(fd, UI_SET_EVBIT, EV_REL);
  ioctl(fd, UI_SET_RELBIT, REL_X);
  ioctl(fd, UI_SET_RELBIT, REL_Y);
  ioctl(fd, UI_SET_RELBIT, REL_WHEEL);  // vertical
  ioctl(fd, UI_SET_RELBIT, REL_HWHEEL); // horizontal

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

  ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER);

  struct uinput_setup rel_setup = {0};
  rel_setup.id.bustype = BUS_USB;
  rel_setup.id.vendor = 0x1234;
  rel_setup.id.product = 0x5678;
  strcpy(rel_setup.name, "Virtual Relative Mouse");

  ioctl(fd, UI_DEV_SETUP, &rel_setup);
  ioctl(fd, UI_DEV_CREATE);

  return fd;
}

HybridInput *input_init(const int screen_w, const int screen_h) {

  HybridInput *devices = malloc(sizeof(HybridInput));
  if (!devices)
    return NULL;

  // rel setup
  devices->fd_mouse = rel_device_init();
  if (devices->fd_mouse < 0)
    return NULL;

  // abs setup
  devices->fd_abs = abs_device_init(screen_w, screen_h);
  if (devices->fd_abs < 0) {
    ioctl(devices->fd_mouse, UI_DEV_DESTROY);
    close(devices->fd_mouse);
    return NULL;
  }

  usleep(1000000);
  return devices;
}

int input_destroy(HybridInput *devices) {
  ioctl(devices->fd_mouse, UI_DEV_DESTROY);
  close(devices->fd_mouse);
  ioctl(devices->fd_abs, UI_DEV_DESTROY);
  close(devices->fd_abs);
  return 0;
}

void emit(int fd, int type, int code, int val) {
  struct input_event ie;
  memset(&ie, 0, sizeof(ie));
  ie.type = type;
  ie.code = code;
  ie.value = val;
  write(fd, &ie, sizeof(ie));
}

int input_abs_move(const int fd, const int x, const int y) {
  emit(fd, EV_ABS, ABS_X, x);
  emit(fd, EV_ABS, ABS_Y, y);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}

int input_rel_move(const int fd, const int x, const int y) {
  emit(fd, EV_REL, REL_X, x);
  emit(fd, EV_REL, REL_Y, y);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}
int input_click_left(const int fd) {
  emit(fd, EV_KEY, BTN_LEFT, 1);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  usleep(20000);
  emit(fd, EV_KEY, BTN_LEFT, 0);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}
int input_click_right(const int fd) {
  emit(fd, EV_KEY, BTN_RIGHT, 1);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  usleep(20000);
  emit(fd, EV_KEY, BTN_RIGHT, 0);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}
int input_click_middle(const int fd) {
  emit(fd, EV_KEY, BTN_MIDDLE, 1);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  usleep(20000);
  emit(fd, EV_KEY, BTN_MIDDLE, 0);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}

// scroll vertically: postive = up, negative = down
int scroll_vertical(int fd, int clicks) {
  emit(fd, EV_REL, REL_WHEEL, clicks);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}

// scroll horizontal: postive = right, negative = left
int scroll_horizontal(int fd, int clicks) {
  emit(fd, EV_REL, REL_WHEEL, clicks);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}
