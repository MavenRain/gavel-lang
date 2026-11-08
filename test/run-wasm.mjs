// Runs entries of a langc Wasm module and prints each result as `langc eval`
// does: a number, or `trap` when the module traps.
// Usage: node test/run-wasm.mjs FILE NAME [ARGS...]
//        node test/run-wasm.mjs FILE -   (one `NAME [ARGS...]` line on stdin per call)
import { readFileSync } from "node:fs";

const [file, name, ...args] = process.argv.slice(2);
if (file === undefined || name === undefined) {
  process.stderr.write("usage: node test/run-wasm.mjs FILE NAME [ARGS...] | FILE -\n");
  process.exit(2);
}
const instance = new WebAssembly.Instance(new WebAssembly.Module(readFileSync(file)), {});

// A Word argument (S8) has the eval form 31w or 0x1fw: the w is dropped.
const call = ([entry, ...words]) => {
  const fn = instance.exports[entry];
  if (typeof fn !== "function") throw new Error(`no exported entry '${entry}'`);
  try {
    return BigInt.asUintN(64, fn(...words.map((w) => BigInt(w.replace(/w$/, ""))))).toString();
  } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return "trap";
    throw e;
  }
};

const calls = name === "-"
  ? readFileSync(0, "utf8").split("\n").filter((line) => line !== "").map((line) => line.split(" "))
  : [[name, ...args]];
process.stdout.write(calls.map((c) => call(c) + "\n").join(""));
