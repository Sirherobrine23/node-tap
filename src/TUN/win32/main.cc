#include <napi.h>
#include "interface_maneger.cc"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return exports;
}
NODE_API_MODULE(vnot, Init)