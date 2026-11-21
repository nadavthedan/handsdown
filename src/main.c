#include "free_mode.h"
#include "grid_mode.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  printf("This is handsdown the best tool!\n");
  // int fd = input_init();
  // free_mode();
  grid_mode_four_way_rec_split();
  // input_click_left(fd);

  printf("INFO: exiting cleanly...\n");
  // input_destroy(fd);
  printf("INFO: exit complete\n");

  return EXIT_SUCCESS;
}
