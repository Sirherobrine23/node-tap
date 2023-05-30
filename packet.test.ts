import { createWriteStream } from "fs";
import { createInterface } from "./src/index.js";

const tun0 = await createInterface("tun0", {
  IPv4: {
    mask: 24,
    IP: "10.10.7.7",
  },
  IPv6: {
    IP: "::ffff:10.10.7.7"
  }
});

tun0.pipe(createWriteStream("tun0.tun"));