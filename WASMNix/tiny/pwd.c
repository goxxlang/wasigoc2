#include <stdio.h>
#include <unistd.h>

int main(void) {
  char buf[4096];
  if (!getcwd(buf, sizeof(buf))) return 1;
  printf("%s\n", buf);
  return 0;
}
