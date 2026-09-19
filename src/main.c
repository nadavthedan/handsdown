#include "display.h"
#include "gdk/gdkkeysyms.h"
#include "grid_mode.h"
#include "input.h"
#include "overlay.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void handler_example(GridOverlay *go, guint keyval) {
  printf("1\n");
  if (keyval != GDK_KEY_p) {
    return;
  }
  printf("2\n");
  double last_width_precent =
      go->state.grid_sections[go->state.grid_sections_count - 1].width_percent;
  double last_height_precent =
      go->state.grid_sections[go->state.grid_sections_count - 1].height_percent;

  GridSection new_section = {0, 0, last_width_precent / 2, last_height_precent,
                             2, 2, {0.3, 0.1, 0, 0.3}};
  GridSection *new_sections =
      malloc(sizeof(GridSection) * ++go->state.grid_sections_count);
  new_sections = go->state.grid_sections;
  new_sections[go->state.grid_sections_count - 1] = new_section;
  go->state.grid_sections = new_sections;
}

int main(int argc, char *argv[]) {
  printf("This is handsdown the best tool!\n");

  int input_fd = input_init();

  if (input_fd < 0) {
    printf("ERROR!\n");
    return input_fd;
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
  // input_abs_move(input_devices->fd_abs, 980, 580);

  // GridSection section_1 = {0, 0, 100, 100, 2, 2, {0.10, 0.05, 0.04, 0.5}};
  // GridSection section_2 = {0, 0, 50, 50, 2, 2, {0, 0, 0, 0.5}};
  // GridSection section_3 = {25, 25, 25, 25, 2, 2, {0, 0, 0, 0.5}};
  // GridSection sections[] = {section_1, section_2, section_3};
  // GridText text_1 = {31.25, 31.25, "d", 1};
  // GridText text_2 = {31.25, 43.75, "f", 1};
  // GridText text_3 = {43.75, 31.25, "j", 1};
  // GridText text_4 = {43.75, 43.75, "k", 1};
  // GridText texts[] = {text_1, text_2, text_3, text_4};
  // GridOverlay overlay = {
  //     {1, {1, 1, 1, 0.3}, {0, 0, 0, 1}, 20, {1, 1, 1, 0.3}, {0, 0, 0, 1}},
  //     {false, sections, 3, texts, 4},
  //     handler_example};
  // overlay_create(&overlay);

  grid_mode_four_way_rec_split(input_fd, disp_ctx);

  printf("INFO: exiting cleanly...\n");
  display_listener_stop(disp_ctx);
  input_destroy(input_fd);
  printf("INFO: exit complete\n");

  return EXIT_SUCCESS;
}
