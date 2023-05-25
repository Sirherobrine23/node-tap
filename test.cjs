const linux = require("./linux.cjs");

const tun0 = new linux.Tun("tun0", { tap: true });
tun0.on("data", (data) => {
  process.stdout.write(Buffer.concat([
    data.subarray(0, 2),
    data.subarray(5),
  ]));
  process.stdout.write("\n\n");
  console.log("[%f]: Size: %f", Date.now(), data.byteLength);
  tun0.write(Buffer.from([
    0x00,
    0x01,
  ]));
});