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

  grid_mode_four_way_rec_split();

  printf("INFO: exiting cleanly...\n");
  printf("INFO: exit complete\n");

  return EXIT_SUCCESS;
}
