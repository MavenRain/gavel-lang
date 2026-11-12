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
    // S9a: a last word `out:N` is not an argument. The entry is in the host
    // form: at the status 0, the N out words of the memory follow the status.
    const last = words.at(-1) ?? "";
    const host = /^out:\d+$/.test(last);
    const rest = host ? words.slice(0, -1) : words;
    // S9a (D41): a word `at:B:W,W,...` is not an argument. Before the call,
    // the harness writes the u64 words W at the byte B of the memory.
    for (const w of rest.filter((x) => x.startsWith("at:"))) {
      const [, at, list] = w.split(":");
      const vals = list.split(",").map((x) => BigInt(x));
      new BigUint64Array(instance.exports.memory.buffer, Number(at), vals.length).set(vals);
    }
    const argv = rest.filter((x) => !x.startsWith("at:"));
    const status = BigInt.asUintN(64, fn(...argv.map((w) => BigInt(w.replace(/w$/, "")))));
    const outs = host && status === 0n ? [...new BigUint64Array(instance.exports.memory.buffer, 0, Number(last.slice(4)))] : [];
    return [status, ...outs].join(" ");
  } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return "trap";
    throw e;
  }
};

const calls = name === "-"
  ? readFileSync(0, "utf8").split("\n").filter((line) => line !== "").map((line) => line.split(" "))
  : [[name, ...args]];
process.stdout.write(calls.map((c) => call(c) + "\n").join(""));
