#include "display.h"
#include "input.h"
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define IOCTL_OR_CLEANUP(fd, request, arg)                                     \
  do {                                                                         \
    if (ioctl(fd, request, arg) < 0) {                                         \
      close(fd);                                                               \
      return -1;                                                               \
    }                                                                          \
  } while (0)

static int rel_device_init(void) {
  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    return fd;

  IOCTL_OR_CLEANUP(fd, UI_SET_EVBIT, EV_REL);
  IOCTL_OR_CLEANUP(fd, UI_SET_RELBIT, REL_X);
  IOCTL_OR_CLEANUP(fd, UI_SET_RELBIT, REL_Y);
  IOCTL_OR_CLEANUP(fd, UI_SET_RELBIT, REL_WHEEL);  // vertical
  IOCTL_OR_CLEANUP(fd, UI_SET_RELBIT, REL_HWHEEL); // horizontal

  IOCTL_OR_CLEANUP(fd, UI_SET_EVBIT, EV_KEY);
  IOCTL_OR_CLEANUP(fd, UI_SET_KEYBIT, BTN_LEFT);
  IOCTL_OR_CLEANUP(fd, UI_SET_KEYBIT, BTN_RIGHT);
  IOCTL_OR_CLEANUP(fd, UI_SET_KEYBIT, BTN_MIDDLE);

  IOCTL_OR_CLEANUP(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER);

  struct uinput_setup rel_setup = {0};
  rel_setup.id.bustype = BUS_USB;
  rel_setup.id.vendor = 0x1234;
  rel_setup.id.product = 0x5678;
  strcpy(rel_setup.name, "Virtual Relative Mouse");

  IOCTL_OR_CLEANUP(fd, UI_DEV_SETUP, &rel_setup);
  IOCTL_OR_CLEANUP(fd, UI_DEV_CREATE, NULL);

  return fd;
}

int input_init(void) {
  // rel setup
  int input_fd = rel_device_init();
  if (input_fd < 0)
    return input_fd;

  usleep(1000000);
  return input_fd;
}

int input_destroy(int input_fd) {
  if (input_fd < 0)
    return -1;
  ioctl(input_fd, UI_DEV_DESTROY);
  close(input_fd);
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
  emit(fd, EV_REL, REL_HWHEEL, clicks);
  emit(fd, EV_SYN, SYN_REPORT, 0);
  return 0;
}
