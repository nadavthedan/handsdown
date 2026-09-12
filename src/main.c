#include "input/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  printf("This is handsdown the best tool!\n");
  HybridInput *fd = input_init(1920, 1080);
  if (!fd->fd_abs || !fd->fd_mouse) {
    printf("ERROR!\n");
  }

  input_rel_move(fd->fd_mouse, 500, 200);
  input_abs_move(fd->fd_abs, 980, 590);
  input_click_left(fd->fd_mouse);

  input_destroy(fd);
  return EXIT_SUCCESS;
}
