// tests/run_wasm.mjs
// Runs the freestanding 扎金花 rules checks compiled to WebAssembly.
// Usage: node tests/run_wasm.mjs /tmp/zjh_tests.wasm
import { readFileSync } from "node:fs";

const path = process.argv[2] || "/tmp/zjh_tests.wasm";
const bytes = readFileSync(path);
const { instance } = await WebAssembly.instantiate(bytes, {});
const code = instance.exports.run();

if (code === 0) {
  console.log("zjh logic: PASS (3000 simulated hands, wasm)");
} else {
  console.error(`zjh logic: FAIL at tests/wasm_runner.c:${code}`);
  process.exit(1);
}
