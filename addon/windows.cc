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
#include <ws2tcpip.h>
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

  // End adapter
  WINTUN_ADAPTER_HANDLE Adapter = WintunOpenAdapter((LPCWSTR)interfaceName.Utf16Value().c_str());
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

  /* IPv4 config */
  if (options.Get("IPv4").IsObject()) {
    MIB_UNICASTIPADDRESS_ROW AddressRow;
    InitializeUnicastIpAddressEntry(&AddressRow);
    WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
    AddressRow.DadState = IpDadStatePreferred;

    AddressRow.Address.Ipv4.sin_family = AF_INET;
    const Napi::Object IPv4Config = options.Get("IPv4").ToObject();
    AddressRow.OnLinkPrefixLength = IPv4Config.Get("mask").ToNumber().Int32Value();
    // AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = inet_addr(IPv4Config.Get("IP").ToString().Utf8Value().c_str());
    inet_pton(AF_INET, IPv4Config.Get("IP").ToString().Utf8Value().c_str(), &AddressRow.Address.Ipv4.sin_addr);

    DWORD LastError = CreateUnicastIpAddressEntry(&AddressRow);
    if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS) {
      WintunCloseAdapter(Adapter);
      std::string msgErr = "Failed to set IPs address, Error code: ";
      std::string err = std::to_string(LastError);
      Napi::Error::New(env, msgErr.append(err).c_str()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
  }

  /* IPv6 config */
  if (options.Get("IPv6").IsObject()) {
    MIB_UNICASTIPADDRESS_ROW AddressRow;
    InitializeUnicastIpAddressEntry(&AddressRow);
    WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
    AddressRow.DadState = IpDadStatePreferred;

    const Napi::Object IPv6Config = options.Get("IPv6").ToObject();
    AddressRow.Address.Ipv6.sin6_family = AF_INET6;
    inet_pton(AF_INET6, IPv6Config.Get("IP").ToString().Utf8Value().c_str(), &AddressRow.Address.Ipv6.sin6_addr);

    DWORD LastError = CreateUnicastIpAddressEntry(&AddressRow);
    if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS) {
      WintunCloseAdapter(Adapter);
      std::string msgErr = "Failed to set IPs address, Error code: ";
      std::string err = std::to_string(LastError);
      Napi::Error::New(env, msgErr.append(err).c_str()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
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
    if (Packet) {
      memcpy(Packet, Data, PacketSize);
      WintunSendPacket(Session, Packet);
      return;
    }

    DWORD lastErr = GetLastError();
    std::string errMsg = "Cannot write Buffer, Error code: ";
    SetError(errMsg.append((char*)lastErr));
    return;
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
  /* Check Buffer size */
  if (info[1].As<Napi::Buffer<BYTE>>().ByteLength() > WINTUN_MAX_IP_PACKET_SIZE) {
    Napi::Error::New(env, "Data ir large").ThrowAsJavaScriptException();
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

std::string wcharToString(WCHAR *txt) {
  std::wstring ws(txt);
  std::string str(ws.begin(), ws.end());
  return str;
}

Napi::Value parseFrame(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  const Napi::Buffer<BYTE> __data = info[0].As<Napi::Buffer<BYTE>>();
  if (__data.IsEmpty() || __data.IsUndefined()) {
    Napi::Error::New(env, "Set buffer frame!").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  DWORD PacketSize = __data.ByteLength();
  if (PacketSize < 20) {
    Napi::Error::New(env, "Received packet without room for an IP header").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const Napi::Object packetInfo = Napi::Object::New(env);
  BYTE *Packet = __data.Data(), IpVersion = Packet[0] >> 4, Proto;
  WCHAR Src[46], Dst[46];

  if (IpVersion == 4) {
    packetInfo.Set("version", "IPv4");
    RtlIpv4AddressToStringW((struct in_addr *)&Packet[12], Src);
    RtlIpv4AddressToStringW((struct in_addr *)&Packet[16], Dst);
    Proto = Packet[9];
    Packet += 20, PacketSize -= 20;
  } else if (IpVersion == 6 && PacketSize > 40) {
    packetInfo.Set("version", "IPv6");
    RtlIpv6AddressToStringW((struct in_addr6 *)&Packet[8], Src);
    RtlIpv6AddressToStringW((struct in_addr6 *)&Packet[24], Dst);
    Proto = Packet[6];
    Packet += 40, PacketSize -= 40;
  } else {
    Napi::Error::New(env, "Cannot parse frame!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  packetInfo.Set("proto", Napi::Number::New(env, Proto));
  packetInfo.Set("src", Napi::String::New(env, wcharToString(Src)));
  packetInfo.Set("dst", Napi::String::New(env, wcharToString(Dst)));
  if (Proto == 1 && PacketSize >= 8 && Packet[0] == 0) {
    packetInfo.Set("proto", "ICMP");
  }

  return packetInfo;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("parseFrame", Napi::Function::New(env, parseFrame));
  exports.Set("getSessions", Napi::Function::New(env, getSessions));
  exports.Set("closeAdapter", Napi::Function::New(env, CloseAdapter));
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  exports.Set("ReadSessionBuffer", Napi::Function::New(env, ReadSessionBuffer));
  exports.Set("WriteSessionBuffer", Napi::Function::New(env, WriteSessionBuffer));
  return exports;
}

NODE_API_MODULE(addon, Init);