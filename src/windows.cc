#include <napi.h>
#include <iostream>
#include <map>
#include <thread>
#include <winsock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <mstcpip.h>
#include <ip2string.h>
#include <winternl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <combaseapi.h>
#include "wintun.h"

static WINTUN_CREATE_ADAPTER_FUNC *WintunCreateAdapter;
static WINTUN_CLOSE_ADAPTER_FUNC *WintunCloseAdapter;
static WINTUN_OPEN_ADAPTER_FUNC *WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC *WintunGetAdapterLUID;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC *WintunGetRunningDriverVersion;
static WINTUN_DELETE_DRIVER_FUNC *WintunDeleteDriver;
static WINTUN_SET_LOGGER_FUNC *WintunSetLogger;
static WINTUN_START_SESSION_FUNC *WintunStartSession;
static WINTUN_END_SESSION_FUNC *WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC *WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC *WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC *WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC *WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC *WintunSendPacket;

static HMODULE InitializeWintun(LPCWSTR location = L"wintun.dll") {
  HMODULE Wintun = LoadLibraryExW(location, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!Wintun) return NULL;
  #define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
  if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
    X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
    X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
    X(WintunAllocateSendPacket) || X(WintunSendPacket))
  #undef X
  {
    DWORD LastError = GetLastError();
    FreeLibrary(Wintun);
    SetLastError(LastError);
    return NULL;
  }
  return Wintun;
}

std::string GuidToString(const GUID& interfeceGuid) {
  // char guid_string[37];
  // snprintf(guid_string, sizeof(guid_string), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", interfeceGuid.Data1, interfeceGuid.Data2, interfeceGuid.Data3, interfeceGuid.Data4[3], interfeceGuid.Data4[4], interfeceGuid.Data4[5], interfeceGuid.Data4[6], interfeceGuid.Data4[7]);
  // return guid_string;

  std::string nGuidString = std::to_string(interfeceGuid.Data1);
  nGuidString = nGuidString.append("-").append(std::to_string(interfeceGuid.Data2)).append("-").append(std::to_string(interfeceGuid.Data3)).append("-");
  nGuidString = nGuidString.append(std::to_string(interfeceGuid.Data4[3]));
  nGuidString = nGuidString.append(std::to_string(interfeceGuid.Data4[4]));
  nGuidString = nGuidString.append(std::to_string(interfeceGuid.Data4[5]));
  nGuidString = nGuidString.append(std::to_string(interfeceGuid.Data4[6]));
  nGuidString = nGuidString.append(std::to_string(interfeceGuid.Data4[7]));
  return nGuidString;
}

std::map<std::string, WINTUN_SESSION_HANDLE> Sessions;
std::map<std::string, WINTUN_ADAPTER_HANDLE> Adapters;

Napi::Value createInterface(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  Napi::String interfaceName = info[0].As<Napi::String>();
  if (interfaceName.IsUndefined()) interfaceName = Napi::String::New(env, "nodetun");
  Napi::Object options = info[1].ToObject();

  HMODULE Wintun = InitializeWintun((LPCWSTR)options.Get("dll").ToString().Utf16Value().c_str());
  if (!Wintun) {
    Napi::Error::New(env, "Cannot Initialize Wintun, check dll location").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  WINTUN_ADAPTER_HANDLE Adapter;
  Adapter = WintunOpenAdapter((LPCWSTR)interfaceName.Utf16Value().c_str());
  // End adapter
  if (Adapter == NULL) {
    WintunCloseAdapter(Adapter);
    Adapter = NULL;
  }

  GUID interfeceGuid; CoCreateGuid(&interfeceGuid);
  Adapter = WintunCreateAdapter((LPCWSTR)interfaceName.Utf16Value().c_str(), L"Wintun", &interfeceGuid);
  if (!Adapter) {
    int errStatus = GetLastError();
    if (errStatus == 5) Napi::Error::New(env, "Run a administrador user").ThrowAsJavaScriptException();
    else {
      std::string err = std::to_string(errStatus);
      std::string msgErr = "Failed to create adapter, Error code: ";
      Napi::Error::New(env, msgErr.append(err).c_str()).ThrowAsJavaScriptException();
    }
    return env.Undefined();
  }

  MIB_UNICASTIPADDRESS_ROW AddressRow;
  InitializeUnicastIpAddressEntry(&AddressRow);
  WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
  AddressRow.Address.Ipv4.sin_family = AF_INET;
  AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = htonl((10 << 24) | (6 << 16) | (7 << 8) | (7 << 0)); /* 10.6.7.7 */
  AddressRow.OnLinkPrefixLength = 24; /* This is a /24 network */
  AddressRow.DadState = IpDadStatePreferred;

  DWORD LastError = CreateUnicastIpAddressEntry(&AddressRow);
  if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS) {
    WintunCloseAdapter(Adapter);
    Napi::Error::New(env, "Failed to set IP address").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  WINTUN_SESSION_HANDLE Session = WintunStartSession(Adapter, 0x400000);
  if (!Session) {
    WintunCloseAdapter(Adapter);
    Napi::Error::New(env, "Failed to create adapter").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string guidString;
  Adapters[(guidString = GuidToString(interfeceGuid))] = Adapter;
  Sessions[guidString] = Session;
  return Napi::String::New(env, guidString);
}

Napi::Value getSessions(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  const Napi::Array napiSessions = Napi::Array::New(env);
  for (std::map<std::string, WINTUN_SESSION_HANDLE>::iterator it = Sessions.begin(); it != Sessions.end(); ++it) napiSessions.Set(napiSessions.Length(), Napi::String::New(env, it->first));
  return napiSessions;
}

class ReadAsync : public Napi::AsyncWorker {
  private:
  BYTE* CallbackBuffer;
  DWORD PacketSize;
  WINTUN_SESSION_HANDLE Session;
  public:
  ReadAsync(const Napi::Function& callback, const Napi::String& sessionGuid) : AsyncWorker(callback), Session(Sessions[sessionGuid.Utf8Value()]) {}
  ~ReadAsync() {}

  void Execute() override {
    for (;;) {
      CallbackBuffer = WintunReceivePacket(Session, &PacketSize);
      if (CallbackBuffer) break;
      else if (GetLastError() == ERROR_NO_MORE_ITEMS) WaitForSingleObject(WintunGetReadWaitEvent(Session), INFINITE);
      else {
        DWORD LastError = GetLastError();
        std::string err = std::to_string(LastError);
        std::string msgErr = "Packet read failed, Error code: ";
        std::cerr << msgErr.append(err).c_str() << std::endl;
        SetError(msgErr.append(err));
        break;
      }
    }
  }

  void OnOK() override {
    Napi::HandleScope scope(Env());
    Callback().Call({Env().Null(), Napi::Buffer<BYTE>::New(Env(), CallbackBuffer, PacketSize)});
    WintunReleaseReceivePacket(Session, CallbackBuffer);
  }

  void OnError(const Napi::Error& e) override {
    std::cerr << "Error" << std::endl;
    Napi::HandleScope scope(Env());
    const Napi::Value ee = e.Value();
    Callback().Call({ee, Env().Null()});
  }
};

Napi::Value ReadSessionBuffer(const Napi::CallbackInfo& info) {
  const Napi::String guidString = info[0].ToString();
  if (Sessions.find(guidString.Utf8Value()) == Sessions.end()) {
    const Napi::Env env = info.Env();
    Napi::Error::New(env, "Session GUID not exists").ThrowAsJavaScriptException();
    return env.Undefined();
  };
  ReadAsync* read = new ReadAsync(info[1].As<Napi::Function>(), guidString);
  read->Queue();
  return info.Env().Undefined();
}

class WriteAsync : public Napi::AsyncWorker {
  private:
  WINTUN_SESSION_HANDLE Session;
  DWORD PacketSize;
  BYTE* Data;

  public:
  WriteAsync(const Napi::Function& callback, const Napi::String& sessionGuid, const Napi::Buffer<BYTE> Buff) :
    AsyncWorker(callback),
    Session(Sessions[sessionGuid.Utf8Value()]),
    PacketSize(Buff.ByteLength()),
    Data(Buff.Data())
  {}
  ~WriteAsync() {}

  void Execute() override {
    BYTE* Packet = WintunAllocateSendPacket(Session, PacketSize);
    if (!Packet) {
      SetError("Cannot write Buffer");
      return;
    }

    memcpy(Packet, Data, PacketSize);
    WintunSendPacket(Session, Packet);
  }

  void OnOK() override {
    Napi::HandleScope scope(Env());
    Callback().Call({Env().Null()});
  }

  void OnError(const Napi::Error& e) override {
    std::cerr << "Error" << std::endl;
    Napi::HandleScope scope(Env());
    const Napi::Value ee = e.Value();
    Callback().Call({ee, Env().Null()});
  }
};

Napi::Value WriteSessionBuffer(const Napi::CallbackInfo& info) {
  const Napi::String guidString = info[0].ToString();
  const Napi::Env env = info.Env();
  if (Sessions.find(guidString.Utf8Value()) == Sessions.end()) {
    Napi::Error::New(env, "Session GUID not exists").ThrowAsJavaScriptException();
    return env.Undefined();
  };
  if (!(info[1].IsObject())) {
    Napi::Error::New(env, "Arg 1 require Buffer").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if (!(info[2].IsFunction())) {
    Napi::Error::New(env, "Arg 2 require Callback").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  WriteAsync* Write = new WriteAsync(info[2].As<Napi::Function>(), guidString, info[1].As<Napi::Buffer<BYTE>>());
  Write->Queue();
  return env.Undefined();
}

Napi::Value CloseAdapter(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  const Napi::String guidString = info[0].ToString();
  if (Sessions.find(guidString.Utf8Value()) == Sessions.end()) {
    Napi::Error::New(env, "Session GUID not exists").ThrowAsJavaScriptException();
    return env.Undefined();
  };
  WintunEndSession(Sessions[guidString.Utf8Value()]);
  WintunCloseAdapter(Adapters[guidString.Utf8Value()]);
  Sessions.erase(guidString.Utf8Value());
  Adapters.erase(guidString.Utf8Value());
  return env.Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getSessions", Napi::Function::New(env, getSessions));
  exports.Set("closeAdapter", Napi::Function::New(env, CloseAdapter));
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  exports.Set("ReadSessionBuffer", Napi::Function::New(env, ReadSessionBuffer));
  exports.Set("WriteSessionBuffer", Napi::Function::New(env, WriteSessionBuffer));
  return exports;
}

NODE_API_MODULE(addon, Init);