const linux = require("./windows.cjs");
const tun0 = new linux.Tun("nodewintun", { tap: true });


/** @param {Buffer} data */
function getHost(data) {
  const str = data.toString().split("\n");
  return (str.find(line => line.trim().toLocaleLowerCase().startsWith("host: ")) || "").slice(5).trim() || str;
}

tun0.on("data", printPacket);
/** @param {Buffer} data */
function printPacket(data) {
  if (data.byteLength < 20) return console.info("Packet without room header");
  const IpVersion = data[0] >> 4;
  if (IpVersion === 4) {
    console.log("is IPv4 proto");
    console.log(getHost(data));
  } else if (IpVersion === 6 && data.byteLength < 40) return console.info("Packet without room header");
  else if (IpVersion === 6) {
    console.log("is IPv6 proto");
    console.log(getHost(data));
  };
}