#include <stdio.h>
#include <sys/utsname.h>

int main(void) {
  struct utsname u;
  if (uname(&u) != 0) return 1;
  printf("%s %s %s %s %s\n", u.sysname, u.nodename, u.release, u.version, u.machine);
  return 0;
}
