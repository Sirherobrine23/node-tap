#include <napi.h>

Napi::Value createInterface(const Napi::CallbackInfo& info) {
  Napi::Error::New(info.Env(), "Not configure function to Linux").ThrowAsJavaScriptException();
  return info.Env().Undefined();
}