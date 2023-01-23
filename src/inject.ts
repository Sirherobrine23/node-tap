import { Duplex } from "stream";

export type payloadConfig = {
  method: string,
  path?: string,
  headers?: {[key: string]: string},
};

export async function injectHead(socket: Duplex, payloadOptions: payloadConfig) {
  socket.write(`${payloadOptions.method} ${payloadOptions.path} HTTP/1.1\r\n`);
  for (const keyName in (payloadOptions.headers ??{})) socket.write(`${keyName}: ${payloadOptions.headers[keyName]}\r\n`);
  socket.write("\r\n");
  let fistData = await new Promise<Buffer>((done) => socket.once("data", (data) => done(Buffer.from(data))));
  let head: Buffer;
  for (let buffEnd = 0; buffEnd < fistData.length; buffEnd++) {
    if (fistData[buffEnd] === 0x0A && fistData[buffEnd -1] === 0x0D && fistData[buffEnd -2] === 0x0A && fistData[buffEnd -3] === 0x0D) {
      head = fistData.slice(0, buffEnd +1);
      fistData = fistData.subarray(buffEnd +1);
      break;
    }
  }

  // Cut fist line
  let Res: Buffer;
  for (let buffEnd = 0; buffEnd < head.length; buffEnd++) {
    if (head[buffEnd] === 0x0A && head[buffEnd - 1] === 0x0D) {
      Res = head.subarray(0, buffEnd +1);
      head = head.subarray(buffEnd +1);
      break;
    }
  }

  let spaces: {start: number, end?: number}[] = [];
  for (let buffEnd = 0; buffEnd < Res.length; buffEnd++) {
    if (Res[buffEnd] === 0x20) {
      if (spaces.length === 0) {
        spaces.push({start: buffEnd});
      } else if (spaces[spaces.length -1].end === undefined) {
        spaces[spaces.length -1].end = buffEnd;
      } else {
        spaces.push({start: buffEnd});
      }
    } else {
      if (spaces[spaces.length -1]) {
        if (spaces[spaces.length -1].end === undefined) {
          spaces[spaces.length -1].end = buffEnd;
        }
      }
    }
  }
  const [ fistVersion, secondStatus ] = spaces;
  let version = Res.subarray(0, fistVersion.start);
  let status = Res.subarray(fistVersion.end, secondStatus.start);
  let statusText = Res.subarray(secondStatus.end);

  return {
    version: version.toString().trim(),
    status: status.toString().trim(),
    statusText: statusText.toString().trim(),
    head: head.toString().split("\r\n").filter((v) => v !== "").map(line => {
      for (let i = 0; i < line.length; i++) {
        if (line[i] === ":") {
          return {
            key: line.slice(0, i),
            value: line.slice(i +2)
          };
        }
      }
      return {
        key: line,
        value: "",
      };
    }).reduce((acc, cur) => {
      acc[cur.key] = cur.value;
      return acc;
    }, {}),
    fistData,
  };
}