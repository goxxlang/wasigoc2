/* WASM DLL. On DLL_PROCESS_ATTACH it loads payload.dll, a native PE image
   (not compiled to wasm). kernel32.LoadLibraryA is the guest import.
   The "native.dll" section names that image for the LoadLibrary hop. */

__attribute__((import_module("kernel32"), import_name("LoadLibraryA")))
extern int LoadLibraryA(const char *name);

__attribute__((used, section("native.dll")))
static const char kNativeDll[] = "payload.dll";

__attribute__((export_name("DllMain")))
int DllMain(void *module, unsigned reason, void *reserved) {
  (void)module;
  (void)reserved;
  if (reason == 1) LoadLibraryA(kNativeDll);
  return 1;
}

__attribute__((export_name("NativePath")))
const char *NativePath(void) { return kNativeDll; }
