#include <stdio.h>
#include <unistd.h>

#ifdef __wasi__
static long uid(void) { return 0; }
static long gid(void) { return 0; }
#else
static long uid(void) { return (long)getuid(); }
static long gid(void) { return (long)getgid(); }
#endif

int main(void) {
  printf("uid=%ld gid=%ld pid=%ld\n", uid(), gid(), (long)getpid());
  return 0;
}
