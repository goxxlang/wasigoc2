#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT int NativeAdd(int a, int b) { return a + b + 1; }

#if defined(_WIN32)
int __stdcall DllMain(void *module, unsigned reason, void *reserved) {
  (void)module;
  (void)reason;
  (void)reserved;
  return 1;
}
#endif
