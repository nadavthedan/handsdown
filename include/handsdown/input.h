#ifndef INPUT_H
#define INPUT_H

typedef struct {
  int fd_mouse;
  int fd_abs;
} HybridInput;

HybridInput *input_init(const int screen_w, const int screen_h);
int input_destroy(HybridInput *fd);
int input_abs_move(const int fd, const int x, const int y);
int input_rel_move(const int fd, const int x, const int y);
int input_click_left(const int fd);
int input_click_right(const int fd);
int input_click_middle(const int fd);

// scroll vertically: postive = up, negative = down
int scroll_vertical(int fd, int clicks);

// scroll horizontal: postive = right, negative = left
int scroll_horizontal(int fd, int clicks);

#endif
