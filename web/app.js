// app.js: PSI demo glue for hash-only and GC-backed PSI via WebAssembly.
//
// We:
//   - Parse Alice & Bob's sets from textareas (newline or comma).
//   - Optionally auto-generate N random items for both sides.
//   - Hash each item with keyed BLAKE3 (via WASM) expanded to 16 bytes.
//   - Call two PSI flavors in WASM:
//       * psi_hash_only_compute  (naive O(n^2) memcmp PSI)
//       * psi_gc_compute         (GC-backed equality)
//   - Compare masks for consistency and show timings.

// Emscripten was built with -s MODULARIZE=1, so `Module` is a factory
// function that returns a Promise of the initialized module instance.
// We lazily create a single shared instance and cache the Promise.
let wasmModulePromise = null;

function getWasmModule() {
  if (!wasmModulePromise) {
    // Calling Module() triggers loading/instantiation and returns a Promise.
    wasmModulePromise = Module();
  }
  return wasmModulePromise;
}

// Legacy helper: simple FNV-1a 64-bit hash expanded to 16 bytes (digest length used by PSI).
// The demo now uses keyed BLAKE3 via WASM for PSI, but we keep this around for quick tests.
function hashStringToDigest(str) {
  let hash = 0xcbf29ce484222325n; // FNV offset basis 64-bit
  const prime = 0x100000001b3n;   // FNV prime

  for (let i = 0; i < str.length; i++) {
    const c = BigInt(str.charCodeAt(i) & 0xff);
    hash ^= c;
    hash *= prime;
    hash &= (1n << 64n) - 1n; // 64-bit wrap
  }

  const out = new Uint8Array(16);
  // Little-endian 64-bit hash duplicated to 16 bytes.
  for (let i = 0; i < 8; i++) {
    out[i] = Number((hash >> BigInt(8 * i)) & 0xffn);
    out[i + 8] = out[i];
  }
  return out;
}

function formatMs(ms) {
  if (!Number.isFinite(ms)) {
    return "–";
  }
  if (ms < 1) {
    // Sub-millisecond – show higher precision
    return ms.toFixed(6);
  }
  if (ms < 1000) {
    // Normal range
    return ms.toFixed(3);
  }
  // Very slow – less precision
  return ms.toFixed(1);
}

function parseSet(text) {
  return text
    .split(/[\r\n,]+/)
    .map(s => s.trim())
    .filter(s => s.length > 0);
}

function generateRandomString() {
  const bytes = new Uint8Array(8);
  if (window.crypto && window.crypto.getRandomValues) {
    window.crypto.getRandomValues(bytes);
  } else {
    // Fallback: Math.random (less strong, but fine for a demo)
    for (let i = 0; i < bytes.length; i++) {
      bytes[i] = Math.floor(Math.random() * 256);
    }
  }
  return Array.from(bytes).map(b => b.toString(16).padStart(2, "0")).join("");
}

const EMAIL_NAMES = [
  "alice", "bob", "carol", "dave", "erin", "frank",
  "grace", "heidi", "ivan", "judy", "mallory", "nick",
  "olivia", "peggy", "trent", "victor", "wendy"
];

const EMAIL_DOMAINS = [
  "example.com",
  "test.example",
  "demo.local",
  "psi-demo.net"
];

function randomInt(max) {
  if (window.crypto && window.crypto.getRandomValues) {
    const buf = new Uint32Array(1);
    window.crypto.getRandomValues(buf);
    return buf[0] % max;
  }
  return Math.floor(Math.random() * max);
}

function randomChoice(arr) {
  return arr[randomInt(arr.length)];
}

function generateRandomEmail() {
  const name = randomChoice(EMAIL_NAMES);
  const domain = randomChoice(EMAIL_DOMAINS);
  const suffix = generateRandomString().slice(0, 4); // short hex tag for uniqueness
  return `${name}${suffix}@${domain}`;
}

function generateRandomSets(n) {
  // Aim for ~25% overlap between Alice and Bob
  const overlap = Math.max(1, Math.floor(n * 0.25));

  const shared = new Set();
  while (shared.size < overlap) {
    shared.add(generateRandomEmail());
  }
  const sharedList = Array.from(shared);

  const used = new Set(sharedList);
  const alice = [...sharedList];
  const bob   = [...sharedList];

  while (alice.length < n) {
    const email = generateRandomEmail();
    if (!used.has(email)) {
      used.add(email);
      alice.push(email);
    }
  }

  while (bob.length < n) {
    const email = generateRandomEmail();
    if (!used.has(email)) {
      used.add(email);
      bob.push(email);
    }
  }

  return {
    alice: alice.join("\n"),
    bob: bob.join("\n"),
  };
}

async function runPsiDemo() {
  const btnRun = document.getElementById("btn-run");
  const outHash = document.getElementById("out-hash");
  const outGc = document.getElementById("out-gc");
  const timeHashSpan = document.getElementById("time-hash");
  const timeGcSpan = document.getElementById("time-gc");
  const sizeHashSpan = document.getElementById("size-hash");
  const sizeGcSpan = document.getElementById("size-gc");
  const countUsedSpan = document.getElementById("count-used");
  const countUsedGcSpan = document.getElementById("count-used-gc");
  const ratioSpan = document.getElementById("ratio");

  const aliceText = document.getElementById("alice").value;
  const bobText   = document.getElementById("bob").value;

  const setA = parseSet(aliceText);
  const setB = parseSet(bobText);

  if (setA.length === 0 || setB.length === 0) {
    const msg = "(Please provide at least one item for both Alice and Bob.)";
    outHash.textContent = msg;
    outGc.textContent   = msg;
    timeHashSpan.textContent = "–";
    timeGcSpan.textContent   = "–";
    sizeHashSpan.textContent = "0";
    sizeGcSpan.textContent   = "0";
    countUsedSpan.textContent = "0";
    if (countUsedGcSpan) countUsedGcSpan.textContent = "0";
    ratioSpan.textContent    = "–";
    return;
  }

  const count = Math.min(setA.length, setB.length);
  countUsedSpan.textContent = String(count);
  if (countUsedGcSpan) countUsedGcSpan.textContent = String(count);

  btnRun.disabled = true;
    outHash.textContent = "(Running...)";
    outGc.textContent   = "(Running...)";
  timeHashSpan.textContent = "…";
  timeGcSpan.textContent   = "…";
  sizeHashSpan.textContent = "…";
  sizeGcSpan.textContent   = "…";
  ratioSpan.textContent    = "…";

  try {
    // psi_gc.js is built with -s MODULARIZE=1, so `Module` is a factory
    // function that returns a Promise of the initialized module instance.
    const wasm = await getWasmModule();

    const elemBytes = 16;
    const elemBits  = elemBytes * 8;

    // Use Emscripten's low-level exports directly for allocation.
    const malloc = wasm._malloc;
    const free   = wasm._free;

    // DEBUG: log the keys on the resolved wasm module to verify exports.
    const wasmKeys = Object.keys(wasm).sort();
    console.log("[PSI] wasm module keys:", wasmKeys);

    // Try to bind the BLAKE3 hash function from the raw export first.
    let blake3HashBytes = wasm._psi_blake3_hash_bytes;

    // If that didn't work, fall back to cwrap using the C symbol name.
    if (typeof blake3HashBytes !== "function" && typeof wasm.cwrap === "function") {
      try {
        blake3HashBytes = wasm.cwrap(
          "psi_blake3_hash_bytes",
          null,
          ["number", "number", "number"]  // (dataPtr, len, outPtr)
        );
      } catch (e) {
        console.warn("cwrap('psi_blake3_hash_bytes', ...) failed:", e);
      }
    }

    if (typeof blake3HashBytes !== "function") {
      const msg = "psi_blake3_hash_bytes is not available on the wasm module. " +
                  "Check that psi_gc.js contains _psi_blake3_hash_bytes and that " +
                  "index.html is loading the rebuilt psi_gc.js.";
      console.error(msg, wasm);
      outHash.textContent = msg;
      outGc.textContent   = msg;
      timeHashSpan.textContent = "–";
      timeGcSpan.textContent   = "–";
      sizeHashSpan.textContent = "0";
      sizeGcSpan.textContent   = "0";
      ratioSpan.textContent    = "–";
      return;
    }

    const bytesA = new Uint8Array(count * elemBytes);
    const bytesB = new Uint8Array(count * elemBytes);

    const encoder = new TextEncoder();

    // Hash each element independently with keyed BLAKE3 in WASM.
    for (let i = 0; i < count; i++) {
      const msgA = encoder.encode(setA[i]);
      const msgB = encoder.encode(setB[i]);

      const lenA = msgA.length;
      const lenB = msgB.length;

      let inAPtr = 0;
      let inBPtr = 0;

      if (lenA > 0) {
        inAPtr = malloc(lenA);
        wasm.HEAPU8.set(msgA, inAPtr);
      }

      if (lenB > 0) {
        inBPtr = malloc(lenB);
        wasm.HEAPU8.set(msgB, inBPtr);
      }

      const outAPtr = malloc(elemBytes);
      const outBPtr = malloc(elemBytes);

      // Call C function: void psi_blake3_hash_bytes(uint8_t* data, size_t len, uint8_t* out)
      blake3HashBytes(inAPtr, lenA, outAPtr);
      blake3HashBytes(inBPtr, lenB, outBPtr);

      bytesA.set(
        wasm.HEAPU8.subarray(outAPtr, outAPtr + elemBytes),
        i * elemBytes
      );
      bytesB.set(
        wasm.HEAPU8.subarray(outBPtr, outBPtr + elemBytes),
        i * elemBytes
      );

      if (inAPtr) free(inAPtr);
      if (inBPtr) free(inBPtr);
      free(outAPtr);
      free(outBPtr);
    }

    const sizeA = bytesA.length;
    const sizeB = bytesB.length;
    const maskBytes = count;

    const ptrA = malloc(sizeA);
    const ptrB = malloc(sizeB);
    const ptrMaskHash = malloc(maskBytes);
    const ptrMaskGc   = malloc(maskBytes);

    wasm.HEAPU8.set(bytesA, ptrA);
    wasm.HEAPU8.set(bytesB, ptrB);

    const psi_create  = wasm.cwrap("psi_gc_create", "number", ["number","number"]);
    const psi_destroy = wasm.cwrap("psi_gc_destroy", null, ["number"]);
    const psi_prepare = wasm.cwrap("psi_gc_prepare_circuit", "number", ["number"]);
    const psi_hash    = wasm.cwrap("psi_hash_only_compute", "number",
                                     ["number","number","number","number","number"]);
    const psi_gc      = wasm.cwrap("psi_gc_compute", "number",
                                     ["number","number","number","number","number"]);

    const ctx = psi_create(count, elemBits);
    if (!ctx) {
      throw new Error("psi_gc_create returned NULL");
    }

    const prep_rc = psi_prepare(ctx);
    if (prep_rc !== 0) {
      psi_destroy(ctx);
      throw new Error("psi_gc_prepare_circuit failed with code " + prep_rc);
    }

    // Hash-only PSI: run multiple times to get a measurable average
    const hashReps = 50;
    const tHashStart = performance.now();
    let rcHash = 0;
    for (let i = 0; i < hashReps; i++) {
      rcHash = psi_hash(ctx, ptrA, ptrB, count, ptrMaskHash);
    }
    const tHashEnd = performance.now();

    if (rcHash !== 0) {
      psi_destroy(ctx);
      throw new Error("psi_hash_only_compute failed with code " + rcHash);
    }

    // GC-backed PSI
    const tGcStart = performance.now();
    const rcGc = psi_gc(ctx, ptrA, ptrB, count, ptrMaskGc);
    const tGcEnd = performance.now();

    if (rcGc !== 0) {
      psi_destroy(ctx);
      throw new Error("psi_gc_compute failed with code " + rcGc);
    }

    const maskHash = new Uint8Array(wasm.HEAPU8.buffer, ptrMaskHash, maskBytes).slice();
    const maskGc   = new Uint8Array(wasm.HEAPU8.buffer, ptrMaskGc, maskBytes).slice();

    psi_destroy(ctx);
    free(ptrA);
    free(ptrB);
    free(ptrMaskHash);
    free(ptrMaskGc);

    // Check consistency between methods.
    let mismatch = false;
    for (let i = 0; i < count; i++) {
      if (maskHash[i] !== maskGc[i]) {
        mismatch = true;
        break;
      }
    }
  if (mismatch) {
    const msg = "ERROR: hash-only and GC masks differ. See console.";
    outHash.textContent = msg;
    outGc.textContent   = msg;
    console.error("maskHash:", maskHash);
    console.error("maskGc:", maskGc);
    timeHashSpan.textContent = "–";
    timeGcSpan.textContent   = "–";
    sizeHashSpan.textContent = "0";
      sizeGcSpan.textContent   = "0";
      ratioSpan.textContent    = "–";
      return;
    }

    const intersection = [];
    for (let i = 0; i < count; i++) {
      if (maskHash[i] === 1) {
        intersection.push(setA[i]);
      }
    }

  const intersectionText = intersection.length > 0
    ? intersection.join("\n")
    : "(no intersection)";
  outHash.textContent = intersectionText;
  outGc.textContent   = intersectionText;

    const timeHash = (tHashEnd - tHashStart) / hashReps;
    const timeGc   = tGcEnd - tGcStart;

    timeHashSpan.textContent = formatMs(timeHash);
    timeGcSpan.textContent   = formatMs(timeGc);
    sizeHashSpan.textContent = String(intersection.length);
    sizeGcSpan.textContent   = String(intersection.length);

    // Avoid divide-by-zero if hash time is extremely small
    const safeTimeHash = Math.max(timeHash, 1e-6);
    ratioSpan.textContent = timeGc > 0 ? (timeGc / safeTimeHash).toFixed(2) : "–";

  } catch (err) {
    console.error(err);
    const msg = "Error: " + (err && err.message ? err.message : String(err));
    outHash.textContent = msg;
    outGc.textContent   = msg;
    timeHashSpan.textContent = "–";
    timeGcSpan.textContent   = "–";
    sizeHashSpan.textContent = "0";
    sizeGcSpan.textContent   = "0";
    ratioSpan.textContent    = "–";
  } finally {
    btnRun.disabled = false;
  }
}

async function onGenerateClick() {
  const countInput = document.getElementById("countInput");
  const n = parseInt(countInput.value, 10);
  if (!Number.isFinite(n) || n <= 0) {
    alert("Please enter a positive integer for N.");
    return;
  }
  const sets = generateRandomSets(n);
  document.getElementById("alice").value = sets.alice;
  document.getElementById("bob").value   = sets.bob;
}

function main() {
  const btnGenerate = document.getElementById("btn-generate");
  const btnRun      = document.getElementById("btn-run");

  if (btnGenerate) {
    btnGenerate.addEventListener("click", onGenerateClick);
  }
  if (btnRun) {
    btnRun.addEventListener("click", () => {
      runPsiDemo();
    });
  }
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", main);
} else {
  main();
}
