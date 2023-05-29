#include <napi.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>

int tun_alloc(char* dev) {
  struct ifreq ifr;
  int fd, err;
  if((fd = open("/dev/net/tun", O_RDWR)) < 0) return -1;
  memset(&ifr, 0, sizeof(ifr));
  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  ifr.ifr_flags = IFF_TAP;
  strncpy(ifr.ifr_name, dev, IFNAMSIZ);

  if((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
    close(fd);
    return err;
  }

  if((err = ioctl(fd, TUNSETPERSIST, 1)) < 0){
    close(fd);
    return err;
  }

  return fd;
}

Napi::Value createInterface(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  std::string dev = info[0].ToString().Utf8Value();
  const Napi::Object options = info[1].IsObject() ? info[1].ToObject() : Napi::Object::New(env);

  int fd, err;
  if((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    std::string err = "Cannot open /dev/net/tun, error: ";
    Napi::Error::New(env, err.append(std::to_string(fd)).c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ);
  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  ifr.ifr_flags = IFF_TUN;

  if((err = ioctl(fd, TUNSETIFF, &ifr)) < 0) {
    close(fd);
    std::string errMsg = "Cannot set tun/tap interface: ";
    Napi::Error::New(env, errMsg.append(std::to_string(err)).c_str()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return Napi::Number::New(env, fd);
}

class ReadAsync : public Napi::AsyncWorker {
  private:
  int fd, PacketSize, Size;
  unsigned char* CallbackBuffer;

  public:
  ReadAsync(const Napi::Function& callback, const Napi::Number& sessionFd, const Napi::Number& Size) :
    AsyncWorker(callback),
    fd(sessionFd.Int32Value()),
    PacketSize(Size.Int32Value()),
    Size(-1)
  {}
  ~ReadAsync() {}

  void Execute() override {
    Size = read(fd, CallbackBuffer, PacketSize);
    if (Size == -1) return SetError("Cannot read fd");
    else if (Size == 0) return SetError("EOF");
  }

  void OnOK() override {
    std::cerr << std::endl << "Size: " << Size << std::endl;
    Napi::HandleScope scope(Env());
    Callback().Call({Env().Null(), Napi::Buffer<unsigned char>::New(Env(), CallbackBuffer, Size)});
  }

  void OnError(const Napi::Error& err) override {
    Napi::HandleScope scope(Env());
    Callback().Call({err.Value(), Env().Null()});
  }
};

Napi::Value ReadBuffer(const Napi::CallbackInfo& info) {
  const Napi::Function Callback = info[2].As<Napi::Function>();
  const Napi::Number Size = info[1].ToNumber();
  const Napi::Number fd = info[0].ToNumber();
  ReadAsync* read = new ReadAsync(Callback, fd, Size);
  read->Queue();
  return info.Env().Undefined();
}

Napi::Value WriteBuff(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  int res, fd = info[0].ToNumber().Int32Value();
  const Napi::Buffer<char> chuck = info[1].As<Napi::Buffer<char>>();
  res = write(fd, chuck.Data(), chuck.ByteLength());
  if (res < 0) {
    std::string errMsg = "Write error: ";
    std::cerr << errMsg.append(std::to_string(res)).c_str() << std::endl;
  }
  return env.Undefined();
}

Napi::Value CloseFD(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  int fd = info[0].ToNumber().Int32Value();
  close(fd);
  return env.Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  exports.Set("WriteBuff", Napi::Function::New(env, WriteBuff));
  exports.Set("ReadBuff", Napi::Function::New(env, ReadBuffer));
  exports.Set("CloseFD", Napi::Function::New(env, CloseFD));
  return exports;
}

NODE_API_MODULE(addon, Init);