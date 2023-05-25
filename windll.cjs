const path = require("path");
if (process.arch === "arm64") console.log(path.join(__dirname, "src/dep/bin/arm64/wintun.dll"));
else if (process.arch === "arm") console.log(path.join(__dirname, "src/dep/bin/arm/wintun.dll"));
else if (process.arch === "x64") console.log(path.join(__dirname, "src/dep/bin/amd64/wintun.dll"));
else console.log(path.join(__dirname, "src/dep/bin/x86/wintun.dll"));