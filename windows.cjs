const stream = require("stream");
const tap = require("./build/Release/tap.node");
const path = require("path");

module.exports.Addon = tap;

module.exports.getSessions = getSessions;
/**
 * 
 * @returns {string[]}
 */
function getSessions() {
  return tap.getSessions();
}

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
      tap.WriteSessionBuffer(this.fd, Buffer.from(chuck, encoding), callback);
    } catch (err) {
      callback(err);
    }
  }

  _read(_size) {
    if (!this.fd) return;
    try {
      tap.ReadSessionBuffer(this.fd, (err, buff) => {
        if (err) return this.emit("error", err);
        return this.push(buff);
      });
    } catch (err) {
      this.emit("error", err);
    }
  }

  _final() {
    tap.closeAdapter(this.fd);
  }

  _destroy(err, callback) {
    tap.closeAdapter(this.fd);
    return callback(err);
  }
}