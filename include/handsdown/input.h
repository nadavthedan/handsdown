#ifndef INPUT_H
#define INPUT_H
#include "utils.h"
#include <stdbool.h>

typedef struct {
  unsigned int key;
  bool physical_down;
  bool forwarded_down;
  bool supress_until_release;
  bool active;
  int (*interval_action)(void);
} HeldEvent;

typedef struct {
  unsigned int key;
  int value_to_trigger;
  int (*action)(void);
} MacroEvent;

DEFINE_ARRAY(HeldEvent, HeldEventArray)
DEFINE_ARRAY(MacroEvent, MacroEventArray)

struct input_events {
  HeldEventArray *held_events;
  MacroEventArray *macro_events;
};

int virtual_keyboard_override(char *physical_keyboard_device_path,
                              struct input_events *ev);
int emit(unsigned int type, unsigned int code, int value);
int sync_output(void);
void stop_virtual_keyboard_override(void);
// -------------------------------------------

#endif
