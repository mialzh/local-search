import { getDocument, queryWords } from "./api.mjs?v=2.9.0";
import "./appearance.mjs?v=2.9.0";

// Every document opens at its beginning, including reloads and old match links.
if ("scrollRestoration" in history) history.scrollRestoration = "manual";
if (/^#match-\d+$/.test(window.location.hash)) {
  history.replaceState(history.state, "", window.location.pathname + window.location.search);
}
window.scrollTo({ top: 0, left: 0, behavior: "auto" });

const $ = (id) => document.getElementById(id);
const content = $("document-content");
const params = new URLSearchParams(window.location.search);
const rawId = params.get("id") || "";
const id = /^[1-9]\d*$/.test(rawId) ? Number(rawId) : NaN;
const currentQuery = params.get("q") || "";
const number = new Intl.NumberFormat("ru-RU");
const plurals = new Intl.PluralRules("ru-RU");
const MAX_MARKS = 2000;
let controller;
let generation = 0;
let documentData;
let markedCount = 0;
let totalMatches = 0;
let highlightsEnabled = false;

const returnParams = new URLSearchParams();
if (currentQuery) returnParams.set("q", currentQuery);
const searchLimit = params.get("limit");
if (["5", "10", "20", "50", "100"].includes(searchLimit)) returnParams.set("limit", searchLimit);
const returnUrl = "/" + (returnParams.size ? `?${returnParams}` : "");
$("brand").href = returnUrl;

function syncHighlights() {
  // Change only paint, keeping the text layout and the reading position intact.
  content.classList.toggle("highlights-off", !highlightsEnabled);
  const toggle = $("highlight-toggle");
  toggle.setAttribute("aria-pressed", String(highlightsEnabled));
  toggle.title = highlightsEnabled ? "Выключить выделение слов" : `Выделить слова запроса «${currentQuery}»`;
  toggle.disabled = markedCount === 0;
  $("document-status").textContent = totalMatches
    ? `Совпадений: ${number.format(totalMatches)}`
    : "Документ открыт целиком";
}

function renderText(text) {
  const words = queryWords(currentQuery);
  const fragment = document.createDocumentFragment();
  const pattern = /[\p{L}\p{Nd}][\p{L}\p{Nd}\p{M}]*/gu;
  let offset = 0;
  let wordCount = 0;
  markedCount = 0;
  totalMatches = 0;
  for (const match of text.matchAll(pattern)) {
    wordCount++;
    if (!words.has(match[0].normalize("NFC").toLowerCase())) continue;
    totalMatches++;
    if (markedCount >= MAX_MARKS) continue;
    fragment.append(document.createTextNode(text.slice(offset, match.index)));
    const mark = document.createElement("mark");
    mark.textContent = match[0];
    fragment.append(mark);
    markedCount++;
    offset = match.index + match[0].length;
  }
  fragment.append(document.createTextNode(text.slice(offset)));
  content.replaceChildren(fragment);
  $("document-matches").hidden = !words.size;
  const unit = { one: "слово", few: "слова", many: "слов", other: "слова" }[plurals.select(wordCount)];
  $("document-meta").textContent = `${number.format(wordCount)} ${unit} · TXT`;
  syncHighlights();
}

async function loadDocument() {
  controller?.abort();
  controller = new AbortController();
  const signal = controller.signal;
  const version = ++generation;
  $("document-body").setAttribute("aria-busy", "true");
  $("document-loading").hidden = false;
  $("document-error").hidden = true;
  $("document-matches").hidden = true;
  $("document-status").textContent = "Загружаем документ";
  $("document-meta").textContent = "Загружаем текст…";
  $("download-document").disabled = true;
  content.hidden = true;
  content.replaceChildren();
  markedCount = 0;
  documentData = undefined;
  try {
    const result = await getDocument(id, signal);
    if (signal.aborted || version !== generation) return;
    documentData = result;
    const title = result.title || "Без названия";
    $("document-title").textContent = title;
    document.title = `${title} — jdanoff search`;
    content.hidden = false;
    $("document-loading").hidden = true;
    renderText(result.text);
    $("download-document").disabled = false;
  } catch (error) {
    if (signal.aborted || version !== generation) return;
    $("document-loading").hidden = true;
    $("document-error").hidden = false;
    $("document-meta").textContent = "Текст недоступен";
    $("document-status").textContent = "Документ не открыт";
    $("document-error-message").textContent = error.message || "Попробуй открыть документ ещё раз.";
    $("retry-document").hidden = error.status === 404 || error.code === "InvalidDocumentId";
  } finally {
    if (version === generation) $("document-body").setAttribute("aria-busy", "false");
  }
}

$("retry-document").addEventListener("click", () => void loadDocument());
$("highlight-toggle").addEventListener("click", () => {
  highlightsEnabled = !highlightsEnabled;
  syncHighlights();
});
$("download-document").addEventListener("click", () => {
  if (!documentData) return;
  const blob = new Blob([documentData.text], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  const title = Array.from(documentData.title.replace(/[<>:"/\\|?*\u0000-\u001f]/g, "_").replace(/[. ]+$/, "")).slice(0, 180).join("") || "document";
  anchor.download = /\.txt$/i.test(title) ? title : `${title}.txt`;
  anchor.href = url;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});

window.addEventListener("pagehide", () => controller?.abort());
window.addEventListener("pageshow", (event) => {
  if (event.persisted && !documentData) void loadDocument();
});
void loadDocument();
