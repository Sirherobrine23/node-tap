#include <napi.h>
#include <functional>

class DuplexStream {
  const Napi::CallbackInfo& __env;
  Napi::Object __node_stream;

  public:
  DuplexStream(const Napi::CallbackInfo &info) : __env(info), __node_stream(info.Env().Global().ToObject().Get("require").As<Napi::Function>().Call({Napi::String::New(info.Env(), "stream")}).As<Napi::Object>().Get("Duplex").As<Napi::Function>().Call({}).As<Napi::Object>()) {}

  Napi::Object getStream() {
    return __node_stream;
  }

  // Set write function
  void setWrite(std::function<Napi::Value (const Napi::CallbackInfo &info)> T) {
    __node_stream.Set("_write", Napi::Function::New(__env.Env(), T));
  }

  // Set write function
  void setWrite(const Napi::Function& T) {
    __node_stream.Set("_write", T);
  }

  // Send chuck without enconding
  void Push(Napi::Buffer<char> chuck) {
    __node_stream.Get("push").As<Napi::Function>().Call(__node_stream, {chuck});
  };

  // Write Chuck and set value
  void Push(Napi::Value chuck, Napi::String encoding) {
    __node_stream.Get("push").As<Napi::Function>().Call(__node_stream, {chuck, encoding});
  };
};