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
      "sources": [
        "src/tap.cc"
      ],
      "conditions": [
        ["OS=='win'", {
          "include_dirs": [
            "src/dep/win/wintun/api"
          ]
        }]
      ]
    }
  ],
}