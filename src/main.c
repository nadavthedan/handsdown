#include "display.h"
#include "gdk/gdkkeysyms.h"
#include "grid_mode.h"
#include "input.h"
#include "overlay.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  printf("This is handsdown the best tool!\n");
  int fd = input_init();

  grid_mode_four_way_rec_split();
  input_click_left(fd);

  printf("INFO: exiting cleanly...\n");
  input_destroy(fd);
  printf("INFO: exit complete\n");

  return EXIT_SUCCESS;
}
