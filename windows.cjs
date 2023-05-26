const stream = require("stream");
const tap = require("./build/Release/tap.node");
const path = require("path");

module.exports.Tun = class Tun extends stream.Duplex {
  fd;
  /**
   *
   * @param {string} name
   */
  constructor(name, options) {
    super({});
    this.fd = tap.createInterface(name, {
      ...options,
      dll: path.join(__dirname, "wintun.dll"),
    });
  }

  _write(chuck, encoding, callback) {
    try {
      this.fd.Write(Buffer.from(chuck, encoding));
      callback();
    } catch (err) {
      callback(err);
    }
  }

  _read(size, atemp = 0) {
    if (!this.fd) return;
    if (atemp > 5) return;
    try {
      const buf = this.fd.Read();
      this.push(buf);
      return buf;
    } catch (err) {
      return this._read(size, ++atemp);
    }
  }
}