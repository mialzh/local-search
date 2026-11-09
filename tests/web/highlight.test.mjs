import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
import { queryWords } from "../../web/api.mjs";

// A minimal DOM double exercises the actual rendering functions without a
// browser dependency. It rejects HTML assignment: document text is never HTML.
class Node {
  constructor(tag = "", text = "") {
    this.tagName = tag;
    this.text = text;
    this.children = [];
    this.attributes = new Map();
    const classes = new Set();
    this.classList = {
      toggle(name, on) { if (on) classes.add(name); else classes.delete(name); },
      contains(name) { return classes.has(name); },
    };
  }
  append(...nodes) {
    for (const node of nodes) {
      if (node.tagName === "fragment") this.children.push(...node.children);
      else this.children.push(typeof node === "string" ? new Node("", node) : node);
    }
  }
  replaceChildren(...nodes) { this.text = ""; this.children = []; this.append(...nodes); }
  set textContent(value) { this.text = value; this.children = []; }
  get textContent() { return this.text + this.children.map((node) => node.textContent).join(""); }
  set innerHTML(_) { throw new Error("Uploaded text must never be assigned as HTML"); }
  setAttribute(key, value) { this.attributes.set(key, value); }
  getAttribute(key) { return this.attributes.get(key); }
  scrollIntoView() { throw new Error("Changing highlighting must not move the document"); }
}

const document = {
  createTextNode: (text) => new Node("", text),
  createElement: (tag) => new Node(tag),
  createDocumentFragment: () => new Node("fragment"),
};
const marks = (node) => [
  ...(node.tagName === "mark" ? [node.textContent] : []),
  ...node.children.flatMap(marks),
];

test("result snippets mark inflected forms as whole words and retain literal text", () => {
  const app = readFileSync(new URL("../../web/app.mjs", import.meta.url), "utf8");
  const code = app.slice(app.indexOf("function appendSnippet("), app.indexOf("function showEmpty("));
  const context = vm.createContext({ document, element(tag, _class, text) {
    const node = document.createElement(tag); node.textContent = text; return node;
  } });
  vm.runInContext(code, context);
  const container = new Node("div");
  const text = "🌊 ДОКУМЕНТАМИ, документы, документами; недокументами <img src=x>";
  context.appendSnippet(container, text, queryWords("документ", ["документами", "документы"]));
  assert.equal(container.textContent, text);
  assert.deepEqual(marks(container), ["ДОКУМЕНТАМИ", "документы", "документами"]);
});

test("reader marks morphology matches and the lamp changes paint state only", () => {
  const reader = readFileSync(new URL("../../web/document-page.mjs", import.meta.url), "utf8");
  const code = reader.slice(reader.indexOf("function syncHighlights("), reader.indexOf("async function loadDocument("));
  const content = new Node("article");
  const controls = new Map();
  const context = vm.createContext({ document, content, queryWords,
    currentQuery: "документ кошка", MAX_MARKS: 2000,
    markedCount: 0, totalMatches: 0, highlightsEnabled: false,
    number: new Intl.NumberFormat("ru-RU"), plurals: new Intl.PluralRules("ru-RU"),
    $(id) { if (!controls.has(id)) controls.set(id, new Node("button")); return controls.get(id); },
  });
  vm.runInContext(code, context);
  const text = "🌊 ДОКУМЕНТАМИ документы документами; недокументами <script>text</script>";
  context.renderText(text, ["документами", "документы"]);
  assert.equal(content.textContent, text);
  assert.deepEqual(marks(content), ["ДОКУМЕНТАМИ", "документы", "документами"]);
  assert.equal(controls.get("highlight-toggle").disabled, false);
  assert.equal(content.classList.contains("highlights-off"), true);
  vm.runInContext("highlightsEnabled = true; syncHighlights();", context);
  assert.equal(content.classList.contains("highlights-off"), false);
  assert.equal(controls.get("highlight-toggle").getAttribute("aria-pressed"), "true");
  assert.equal(content.textContent, text);
  assert.equal(marks(content).length, 3);
});

test("reader disables the lamp when the server reports no actual matches", () => {
  const reader = readFileSync(new URL("../../web/document-page.mjs", import.meta.url), "utf8");
  const code = reader.slice(reader.indexOf("function syncHighlights("), reader.indexOf("async function loadDocument("));
  const content = new Node("article");
  const controls = new Map();
  const context = vm.createContext({ document, content, queryWords,
    currentQuery: "документ", MAX_MARKS: 2000,
    markedCount: 0, totalMatches: 0, highlightsEnabled: false,
    number: new Intl.NumberFormat("ru-RU"), plurals: new Intl.PluralRules("ru-RU"),
    $(id) { if (!controls.has(id)) controls.set(id, new Node("button")); return controls.get(id); },
  });
  vm.runInContext(code, context);
  context.renderText("Документ открыт полностью", []);
  assert.deepEqual(marks(content), []);
  assert.equal(controls.get("highlight-toggle").disabled, true);
});
