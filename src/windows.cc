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
#include <iostream>
#include <thread>
#include "stream/node_stream.cc"
extern "C" {
  #include <wintun.h>
}

Napi::Value createInterface(const Napi::CallbackInfo& info) {
  const Napi::Env env = info.Env();
  Napi::String interfaceName = info[0].As<Napi::String>();
  if (interfaceName.IsUndefined()) interfaceName = Napi::String::New(env, "tapinterface");

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

  HMODULE Wintun = LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!Wintun) {
    Napi::Error::New(env, "Cannot create device, cannot init wintun").ThrowAsJavaScriptException();
    return env.Undefined();
  }
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
    Napi::Error::New(env, "Cannot create device, cannot init wintun").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!Wintun) {
    Napi::Error::New(env, "Cannot create device, cannot init wintun").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  GUID ExampleGuid = { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } };
  WINTUN_ADAPTER_HANDLE Adapter = WintunCreateAdapter((LPCWSTR)interfaceName.Utf8Value().c_str(), L"Wintun", &ExampleGuid);
  if (!Adapter) {
    Napi::Error::New(env, "Failed to create adapter").ThrowAsJavaScriptException();
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

  DuplexStream stream = DuplexStream(info);
  stream.setWrite([&](const Napi::CallbackInfo &info) {
    const Napi::Env env = info.Env();
    // const Napi::Value chuck = info[0];
    const Napi::String encode = info[1].ToString();
    const Napi::Function Callback = info[2].As<Napi::Function>();

    std::cerr << encode.Utf8Value().c_str() << std::endl;
    Callback.Call({});
    return env.Undefined();
  });

  return stream.getStream();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("createInterface", Napi::Function::New(env, createInterface));
  return exports;
}

NODE_API_MODULE(addon, Init);