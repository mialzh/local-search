import test, { afterEach } from "node:test";
import assert from "node:assert/strict";
import {
  ApiError, checkHealth, queryWords, searchDocuments, uploadDocument, validateFile, validateQuery,
} from "../../web/api.mjs";

const originalFetch = globalThis.fetch;
afterEach(() => { globalThis.fetch = originalFetch; });
const json = (body, status = 200) => new Response(JSON.stringify(body), {
  status, headers: { "Content-Type": "application/json" },
});
const result = { document_id: 1, title: "notes.txt", snippet: "Умные указатели", score: 0.75 };

test("search encodes Cyrillic and special characters and preserves server order", async () => {
  globalThis.fetch = async (path, options) => {
    const url = new URL(path, "http://localhost:8080");
    assert.equal(url.pathname, "/search");
    assert.equal(url.searchParams.get("q"), "C++ & указатели");
    assert.equal(url.searchParams.get("limit"), "20");
    assert.equal(url.searchParams.size, 2);
    assert.equal(options.cache, "no-store");
    return json({ results: [result, { ...result, document_id: 2 }] });
  };
  assert.deepEqual(await searchDocuments("C++ & указатели", 20), [result, { ...result, document_id: 2 }]);
});

test("query limits use UTF-8 bytes", () => {
  validateQuery("я".repeat(1024), 100);
  for (const query of ["", "  ", "a\0b", "я".repeat(1025)]) {
    assert.throws(() => validateQuery(query, 10), { code: "InvalidQuery" });
  }
  for (const limit of [0, 101, 1.5, NaN, "10"]) {
    assert.throws(() => validateQuery("word", limit), { code: "InvalidLimit" });
  }
});

test("invalid query never reaches the server", async () => {
  globalThis.fetch = () => assert.fail("Unexpected request");
  await assert.rejects(searchDocuments(" ", 10), { code: "InvalidQuery" });
});

test("upload sends one file field and lets the browser set Content-Type", async () => {
  const file = new File(["Умные указатели"], "конспект.TXT", { type: "text/plain" });
  const added = { document_id: 3, title: file.name, word_count: 2 };
  globalThis.fetch = async (path, options) => {
    assert.equal(path, "/documents");
    assert.equal(options.method, "POST");
    assert.equal(options.headers, undefined);
    assert.deepEqual(Array.from(options.body.keys()), ["file"]);
    assert.equal(options.body.get("file").name, file.name);
    assert.equal(await options.body.get("file").text(), await file.text());
    return json(added, 201);
  };
  assert.deepEqual(await uploadDocument(file), added);
});

test("file validation rejects unsupported, oversized, empty and invalid UTF-8 files", async () => {
  for (const [file, code] of [
    [new File(["text"], "a.pdf"), "UnsupportedFileType"],
    [new File([new Uint8Array(5 * 1024 * 1024 + 1)], "big.txt"), "DocumentTooLarge"],
    [new File([], "empty.txt"), "InvalidText"],
    [new File(["\ufeff  "], "bom.txt"), "InvalidText"],
    [new File(["x\0y"], "null.txt"), "InvalidText"],
    [new File([new Uint8Array([0xff])], "bad.txt"), "InvalidText"],
    [new File(["text"], "я".repeat(126) + ".txt"), "InvalidRequest"],
  ]) await assert.rejects(validateFile(file), { code });
  await validateFile(new File(["\ufeffПривет"], "valid.txt"));
  await validateFile(new File(["word" + " ".repeat(5 * 1024 * 1024 - 4)], "maximum.txt"));
});

test("rejected files never trigger an upload", async () => {
  globalThis.fetch = () => assert.fail("Unexpected upload");
  await assert.rejects(uploadDocument(new File(["text"], "a.pdf")), { code: "UnsupportedFileType" });
});

test("server errors are readable and do not expose raw HTML", async () => {
  globalThis.fetch = async () => json({ error: { code: "InvalidText", message: "<script>bad</script>" } }, 400);
  await assert.rejects(uploadDocument(new File(["!!!"], "a.txt")), (error) => {
    assert.ok(error instanceof ApiError);
    assert.equal(error.code, "InvalidText");
    assert.equal(error.status, 400);
    assert.equal(error.network, false);
    assert.ok(!error.message.includes("<script>"));
    return true;
  });
});

test("search rejects HTML and malformed results but accepts empty results", async () => {
  globalThis.fetch = async () => new Response("<html>Not the API</html>");
  await assert.rejects(searchDocuments("word", 10), { name: "ApiError", status: 200, network: false });
  for (const results of [null, [{}], [{ ...result, score: "0.75" }], [{ ...result, document_id: -1 }]]) {
    globalThis.fetch = async () => json({ results });
    await assert.rejects(searchDocuments("word", 10), { name: "ApiError", status: 200 });
  }
  globalThis.fetch = async () => json({ results: [] });
  assert.deepEqual(await searchDocuments("missing", 10), []);
});

test("network failures are distinguished from API errors", async () => {
  globalThis.fetch = async () => { throw new TypeError("Failed to fetch"); };
  await assert.rejects(searchDocuments("word", 10), { name: "ApiError", network: true });
});

test("cancellation retains AbortError so a newer query can replace the request", async () => {
  globalThis.fetch = (_path, { signal }) => new Promise((_resolve, reject) => {
    const abort = () => reject(new DOMException("Cancelled", "AbortError"));
    if (signal.aborted) abort();
    else signal.addEventListener("abort", abort, { once: true });
  });
  const controller = new AbortController();
  const pending = searchDocuments("word", 10, controller.signal);
  controller.abort();
  await assert.rejects(pending, { name: "AbortError" });
});

test("unexpected upload response is never reported as success", async () => {
  globalThis.fetch = async () => json({ document_id: 0 }, 201);
  await assert.rejects(uploadDocument(new File(["text"], "a.txt")), { name: "ApiError", status: 201 });
});

test("health requires the real endpoint response", async () => {
  globalThis.fetch = async (path) => { assert.equal(path, "/health"); return new Response("ok\n"); };
  assert.equal(await checkHealth(), true);
  globalThis.fetch = async () => new Response("<html>ok</html>");
  assert.equal(await checkHealth(), false);
  globalThis.fetch = async () => new Response("ok", { status: 503 });
  assert.equal(await checkHealth(), false);
  globalThis.fetch = async () => { throw new TypeError("offline"); };
  assert.equal(await checkHealth(), false);
});

test("highlight words are normalized whole tokens", () => {
  assert.deepEqual(queryWords("УМНЫЕ умные C++ cafe\u0301"), new Set(["умные", "c", "café"]));
  assert.equal(queryWords("указатель").has("указатели"), false);
});
