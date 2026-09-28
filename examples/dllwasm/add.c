/* DLL WASM module. Reactor shared object: no _start.
   LoadLibrary accepts the \0asm image; exports are the DLL entry points. */

__attribute__((export_name("DllMain")))
int DllMain(void *module, unsigned reason, void *reserved) {
  (void)module;
  (void)reserved;
  (void)reason;
  return 1;
}

__attribute__((export_name("Add")))
int Add(int a, int b) { return a + b; }
