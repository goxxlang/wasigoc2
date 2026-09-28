#include <unistd.h>

/* Chrome process row. Strong in the module (GetCurrentProcessId). */
__attribute__((weak)) pid_t wasigo_process_id(void);

pid_t getpid(void) {
  if (wasigo_process_id)
    return wasigo_process_id();
  return 1;
}
