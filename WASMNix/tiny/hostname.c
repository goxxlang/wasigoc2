#include <stdio.h>
#include <unistd.h>

int main(void) {
  char buf[256];
  if (gethostname(buf, sizeof(buf)) != 0) return 1;
  printf("%s\n", buf);
  return 0;
}
