const stream = require("stream");
const tap = require("./build/Release/tap.node");

module.exports.Tun = class Tun extends stream.Duplex {
  fd;
  /**
   *
   * @param {string} name
   */
  constructor(name, options) {
    super({});
    this.fd = tap.createInterface(name.slice(0, 16), options);
  }

  _final(callback) {
    tap.CloseFD(this.fd);
    callback();
  }

  _write(chuck, encoding, callback) {
    try {
      tap.WriteBuff(this.fd, Buffer.from(chuck, encoding));
      callback();
    } catch (err) {
      callback(err);
    }
  }

  _read(size, atemp = 0) {
    if (!this.fd) return;
    if (atemp > 5) return;
    try {
      const buf = tap.ReadBuff(this.fd, size||1500);
      this.push(buf);
      return buf;
    } catch (err) {
      return this._read(size, ++atemp);
    }
  }
}