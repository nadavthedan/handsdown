#ifndef INPUT_H
#define INPUT_H
#include "overlay.h"
#include "utils.h"
#include <stdbool.h>

struct input_state;

typedef struct {
  unsigned int key;
  bool active;
  int (*interval_action)(struct input_state *);
} HeldEvent;

typedef struct {
  unsigned int key;
  int value_to_trigger;
  int (*action)(struct input_state *);
} MacroEvent;

typedef struct {
  unsigned int key;
  unsigned int button;
} ButtonEvent;

DEFINE_ARRAY(HeldEvent, HeldEventArray)
DEFINE_ARRAY(MacroEvent, MacroEventArray)
DEFINE_ARRAY(ButtonEvent, ButtonEventArray)

struct input_events {
  HeldEventArray *held_events;
  MacroEventArray *macro_events;
  ButtonEventArray *button_events;
};

struct input_modifiers {
  bool shift;
  bool ctrl;
  bool alt;
  bool super;
}; // may need to change to support MAC but don't care about it rn

struct input_state {
  struct input_modifiers *mods;
  struct input_events *events;
  struct overlay_state *overlay;
};

typedef int (*mode_setup_fn)(struct input_state *state);

int input_stop_action(struct input_state *state);
int run_mode(const char *keyboard_device_path, mode_setup_fn setup,
             bool use_absolute_pointer);
int virtual_keyboard_override(const char *physical_keyboard_device_path,
                              struct input_state *state);
int emit(unsigned int type, unsigned int code, int value);
int sync_output(void);
int pointer_input_init(void);
int pointer_input_move_to_overlay_percent(double x_percent, double y_percent);
void pointer_input_destroy(void);
void stop_virtual_keyboard_override(void);

#endif
