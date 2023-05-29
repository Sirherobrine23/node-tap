import { createInterface, tap } from "./src/index.js";
import fs from "fs";

const tun0 = await createInterface("tun0", {
  IPv4: {
    mask: 24,
    IP: "10.6.7.7",
  },
  IPv6: {
    IP: "::ffff:10.6.7.7"
  }
});
tun0.pipe(fs.createWriteStream("tun0.tun"));
if (process.platform === "win32") {
  tun0.on("data", data => {
    try {
      const a = tap.parseFrame(data);
      console.log(a);
    } catch  (err) {
      console.error(err);
    }
  })
}