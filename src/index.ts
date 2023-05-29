import { Duplex } from "node:stream";
import { createRequire } from "node:module";
import { promisify } from "node:util";
import path from "node:path";
import { fileURLToPath } from "node:url";
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);
export const tap: any = require("../build/Release/tap.node");

export interface tapOptions {
  IPv4?: {
    mask: number;
    IP: string;
  };
  IPv6?: {
    IP: string;
  };
}

export async function createInterface(interfaceName: string, options?: tapOptions): Promise<Duplex> {
  options ||= {};

  if (process.platform === "win32") {
    const guid = tap.createInterface(interfaceName, {
      dll: path.resolve(__dirname, "../wintun.dll"),
      IPv4: !options.IPv4 ? undefined : {
        IP: options.IPv4.IP,
        mask: options.IPv4.mask,
      },
      IPv6: !options.IPv6 ? undefined : {
        IP: options.IPv6.IP
      }
    });
    const duplexStream = new Duplex({
      autoDestroy: true,
      write(chuck, encoding, callback) {
        tap.WriteSessionBuffer(guid, Buffer.from(chuck, encoding), callback);
      },
      read(size) {},
    });
    duplexStream["GUID"] = guid;
    (async () => {
      const ReadSessionBuffer = promisify(tap.ReadSessionBuffer);
      while (true) {
        try {
          const buff: Buffer = await ReadSessionBuffer(guid);
          duplexStream.push(buff);
        } catch (err) {
          duplexStream.emit("error", err)
        }
      }
    })().catch(err => duplexStream.emit("error", err));
    return duplexStream;
  } else if (process.platform === "linux") {
    const ReadBuff = async (fd: number, size: number) => new Promise<Buffer>((done, reject) => tap.ReadBuff(fd, size, (err, buff) => err ? reject(err) : done(buff)));
    const fd = tap.createInterface(interfaceName);
    const duplexStream = new Duplex({
      autoDestroy: true,
      write(chuck, encoding, callback) {
        tap.WriteBuff(fd, Buffer.from(chuck, encoding));
        callback();
      },
      async read(size) {
        return ReadBuff(fd, size).then(buff => duplexStream.push(buff)).catch(err => this.emit("error", err));
      },
    });
    return duplexStream;
  }
  throw new Error("Cannot init interface to current platform");
}