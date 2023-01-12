#include <napi.h>
using namespace Napi;

namespace nodeStream {
  class StreamData : public Napi::ObjectWrap<StreamData> {
    public:
      static Napi::Object Init(Napi::Env env, Napi::Object exports);
      StreamData(const Napi::CallbackInfo& info);
      Napi::Value Write(const Napi::CallbackInfo& info);

    private:
      static Napi::FunctionReference constructor;

  };

  Napi::Object StreamData::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "StreamData", {
      InstanceMethod("write", &StreamData::Write)
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("StreamData", func);
    return exports;
  }

  StreamData::StreamData(const Napi::CallbackInfo& info) : Napi::ObjectWrap<StreamData>(info) {
    // NOOP
  }

  Napi::Value StreamData::Write(const Napi::CallbackInfo& info) {
    if (info.Length() < 1 ) {
      throw Napi::Error::New(info.Env(), "1 argument expected");
    }
    if (!info[0].IsBuffer()) {
      throw Napi::Error::New(info.Env(), "The parameter must be a buffer");
    }
    Napi::Buffer<char> buffer = info[0].As<Napi::Buffer<char>>();
    return buffer;
  }
}
