#include "display.h"
#include "input.h"
#include "overlay.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  printf("This is handsdown the best tool!\n");

  HybridInput *input_devices = input_init();

  if (!input_devices->fd_abs || !input_devices->fd_mouse) {
    printf("ERROR!\n");
  }

  DisplayContext *disp_ctx = display_listener_start();
  if (!disp_ctx) {
    printf("ERROR: failed to start display listener\n");
    return 1;
  }

  usleep(200000); // await for initial discovery

  Monitor my_monitors[MAX_MONITORS];

  for (int i = 0; i < 5; i++) {
    int count = display_get_monitors(disp_ctx, my_monitors, MAX_MONITORS);

    printf("Number of monitors: %d\n", count);
    for (int m = 0; m < count; m++) {
      printf("Mointor %d: %s | res: %dx%d at Position (X: %d, Y: %d)\n", m,
             my_monitors[m].name, my_monitors[m].width, my_monitors[m].height,
             my_monitors[m].x, my_monitors[m].y);
    }
  }
  input_abs_move(input_devices->fd_abs, 980, 580);

  overlay_create(&argc, &argv);

  printf("INFO: exiting cleanly...\n");
  display_listener_stop(disp_ctx);
  input_destroy(input_devices);
  printf("INFO: exit complete\n");

  return EXIT_SUCCESS;
}
