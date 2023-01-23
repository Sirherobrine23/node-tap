import { Duplex } from "node:stream";
import net from "node:net"

export default async function startConnection(targetHost: string, targetPort: number): Promise<Duplex> {
  const connection = net.createConnection(targetPort, targetHost);
  return new Promise((done, reject) => {
    connection.on("error", (err) => reject(err));
    connection.on("connect", () => {
      done(connection);
    });
  });
}