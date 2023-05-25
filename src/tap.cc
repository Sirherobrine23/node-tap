#include <napi.h>
#if _WIN32
#include "windows.cc"
#else
#include "linux.cc"
#endif

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  return exports;
}

NODE_API_MODULE(addon, Init);