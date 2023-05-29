{
  "target_defaults": {
    "include_dirs": [
      "<!(node -p \"require('node-addon-api').include_dir\")"
    ],
    "defines": [
      "NAPI_DISABLE_CPP_EXCEPTIONS"
    ]
  },
  "targets": [
    {
      "target_name": "tap",
      "conditions": [
        ["OS=='win'", {
          "include_dirs": [
            "addon/dep/win/wintun/api"
          ],
          "sources": [
            "addon/windows.cc"
          ],
          "libraries": [
            "ws2_32.lib",
            "ntdll.lib",
            "iphlpapi.lib"
          ]
        }],
        ["OS=='linux'", {
          "sources": [
            "addon/linux.cc"
          ]
        }],
      ]
    }
  ],
}