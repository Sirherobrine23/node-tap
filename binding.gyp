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
            "src/dep/win/wintun/api"
          ],
          "sources": [
            "src/windows.cc"
          ]
        }],
        ["OS=='linux'", {
          "sources": [
            "src/linux.cc"
          ]
        }],
      ]
    }
  ],
}