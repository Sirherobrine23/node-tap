import { injectHead } from "./inject.js";
import main from "./mainConnection.js";

const conn = await main("google.com.br", 80);
const data = await injectHead(conn, {
  method: "GET",
  path: "/",
  headers: {
    "User-Agent": "Deno",
  }
});
console.log(data);
conn.end();