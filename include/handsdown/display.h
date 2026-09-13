#ifndef DISPLAY_H
#define DISPLAY_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_MONITORS 16

typedef struct {
  uint32_t global_id;
  int x;
  int y;
  int width;
  int height;
  int refresh;
  int scale;
  char name[64];
} Monitor;

typedef struct {
  Monitor monitors[MAX_MONITORS];
  int count;
  int total_width;
  int total_height;

  pthread_mutex_t lock;       // Protects shared monitor array
  pthread_t thread_id;        // Background thread handle
  atomic_bool running;        // Loop control flag
  struct wl_display *display; // Saved handle for graceful wakeup
} DisplayContext;

DisplayContext *display_listener_start(void);
void display_listener_stop(DisplayContext *ctx);

// Thread-safe Getter: Safely copies monitor info to the caller thread
int display_get_monitors(DisplayContext *ctx, Monitor *out_monitors,
                         int max_out);
#endif
