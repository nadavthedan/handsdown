#include <stdio.h>

int start_capture(void *key_handlers) {
  int ch;
  printf("started keyboard capture");
  while ((ch = getchar()) != EOF) {
    printf("char pressed: %c", ch);
    switch (ch) {
    case 'q':
    case 'Q':
      break;
    }
  }
  printf("finished keyboard capture");
  return 0;
}
