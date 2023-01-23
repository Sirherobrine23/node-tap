#include <napi.h>
#include "tun_maneger.cc"
using namespace Napi;

void loadStart(const Napi::CallbackInfo& info) {
  int fd = linuxTUN::tun_alloc(info[0].As<Napi::String>());
  printf("fd: %d", fd);
  linuxTUN::ReadData::readFunc(info, fd);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("test", Napi::Function::New(env, loadStart));
  return exports;
}
NODE_API_MODULE(vnot, Init)