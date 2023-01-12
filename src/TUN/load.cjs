// @ts-nocheck
const fs = require("node:fs");
if (process.platform === "win32") {
  return module.exports = require("./win32/win32.node");
} else if (process.platform === "linux"||process.platform === "android") {
  return module.exports = require("./linux/linux.node");
}

throw new Error("Unsupported platform: " + process.platform)