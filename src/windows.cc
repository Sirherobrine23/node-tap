#include <napi.h>
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
#include <iostream>
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

void WriteSession(WINTUN_SESSION_HANDLE Session, const char* Buff, const int Size) {
  BYTE* Packet = WintunAllocateSendPacket(Session, Size);
  if (Packet) {
    memcpy(Packet, Buff, Size);
    WintunSendPacket(Session, Packet);
  }
}

const char* ReadSession(WINTUN_SESSION_HANDLE Session, char* Buff) {
  for (;;) {
    DWORD PacketSize;
    BYTE* Packet = WintunReceivePacket(Session, &PacketSize);
    if (Packet) {
      std::cerr << "Get buffer" << std::endl;
      memcpy(Buff, Packet, PacketSize);
      WintunReleaseReceivePacket(Session, Packet);
      return nullptr;
    } else if (GetLastError() == ERROR_NO_MORE_ITEMS) {
      WaitForSingleObject(WintunGetReadWaitEvent(Session), INFINITE);
    } else {
      DWORD LastError = GetLastError();
      std::string err = std::to_string(LastError);
      std::string msgErr = "Packet read failed, Error code: ";
      return msgErr.append(err).c_str();
    }
  }
}

void closeSession(WINTUN_ADAPTER_HANDLE Adapter) {
  WintunCloseAdapter(Adapter);
}

Napi::Value createInterface(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  Napi::String interfaceName = info[0].As<Napi::String>();
  if (interfaceName.IsUndefined()) interfaceName = Napi::String::New(env, "nodetun");
  Napi::Object options = info[1].ToObject();
  Napi::String dllLocation = options.Get("dll").ToString();

  HMODULE Wintun = InitializeWintun((LPCWSTR)dllLocation.Utf16Value().c_str());
  if (!Wintun) {
    Napi::Error::New(env, "Cannot Initialize Wintun, check dll location").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  GUID GUID;
  CoCreateGuid(&GUID);
  WINTUN_ADAPTER_HANDLE Adapter = WintunCreateAdapter((LPCWSTR)interfaceName.Utf16Value().c_str(), L"Wintun", &GUID);
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

  const Napi::Object funcs = Napi::Object::New(env);

  funcs.Set("deleteInterface", Napi::Function::New(env, [&](const Napi::CallbackInfo& info) -> Napi::Value {
    const Napi::Env env = info.Env();
    try {
      closeSession(Adapter);
    } catch (const std::string& e) {
      Napi::Error::New(env, e.c_str()).ThrowAsJavaScriptException();
    }
    return env.Undefined();
  }));

  funcs.Set("Read", Napi::Function::New(env, [&](const Napi::CallbackInfo& info) -> Napi::Value {
    const Napi::Env env = info.Env();
    try {
      char* Buff = {};
      const char* res = ReadSession(Session, Buff);
      if (!res) return Napi::Buffer<char>::New(env, Buff, sizeof(Buff));
      Napi::Error::New(env, res).ThrowAsJavaScriptException();
    } catch (const std::string& e) {
      Napi::Error::New(env, e.c_str()).ThrowAsJavaScriptException();
    }
    return env.Undefined();
  }));

  funcs.Set("Write", Napi::Function::New(env, [&](const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    const Napi::Buffer<char> chuck = info[0].As<Napi::Buffer<char>>();
    WriteSession(Session, chuck.Data(), chuck.ByteLength());
    return env.Undefined();
  }));

  return funcs;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  return exports;
}

NODE_API_MODULE(addon, Init);