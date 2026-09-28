#include <dirent.h>
#include <stdio.h>

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : ".";
  DIR* d = opendir(path);
  if (!d) return 1;
  struct dirent* e;
  while ((e = readdir(d)) != 0) puts(e->d_name);
  closedir(d);
  return 0;
}
