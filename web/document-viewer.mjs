import { getDocument, queryWords } from "./api.mjs?v=2.1.0";

const $ = (id) => document.getElementById(id);
const dialog = $("document-dialog");
const content = $("document-content");
const scrollArea = $("document-body");
const number = new Intl.NumberFormat("ru-RU");
const plurals = new Intl.PluralRules("ru-RU");
const MAX_MARKS = 2000;
let controller;
let generation = 0;
let currentId;
let currentQuery = "";
let documentData;
let returnFocus;
let marks = [];
let activeMatch = -1;
let totalMatches = 0;

function documentUrl(id, query) {
  const url = new URL(window.location.href);
  url.searchParams.set("document", String(id));
  if (query) url.searchParams.set("q", query);
  return url;
}

function setMatch(index, scroll = true) {
  if (!marks.length) return;
  if (activeMatch >= 0) marks[activeMatch]?.classList.remove("current-match");
  activeMatch = (index + marks.length) % marks.length;
  const mark = marks[activeMatch];
  mark.classList.add("current-match");
  $("document-match-count").textContent = `${number.format(activeMatch + 1)} / ${number.format(marks.length)}${totalMatches > marks.length ? "+" : ""}`;
  if (scroll) mark.scrollIntoView({ block: "center", inline: "nearest", behavior: "auto" });
}

function renderText(text) {
  const words = queryWords(currentQuery);
  const fragment = document.createDocumentFragment();
  const pattern = /[\p{L}\p{Nd}][\p{L}\p{Nd}\p{M}]*/gu;
  let offset = 0;
  let wordCount = 0;
  marks = [];
  totalMatches = 0;
  activeMatch = -1;
  for (const match of text.matchAll(pattern)) {
    wordCount++;
    if (!words.has(match[0].normalize("NFC").toLowerCase())) continue;
    totalMatches++;
    if (marks.length >= MAX_MARKS) continue;
    fragment.append(document.createTextNode(text.slice(offset, match.index)));
    const mark = document.createElement("mark");
    mark.textContent = match[0];
    fragment.append(mark);
    marks.push(mark);
    offset = match.index + match[0].length;
  }
  fragment.append(document.createTextNode(text.slice(offset)));
  content.replaceChildren(fragment);
  $("document-matches").hidden = !words.size;
  $("document-query").textContent = currentQuery;
  $("document-query").title = currentQuery;
  $("document-match-count").textContent = "Нет совпадений";
  $("document-match-count").title = totalMatches > marks.length
    ? `Первые ${number.format(marks.length)} из ${number.format(totalMatches)} совпадений` : "Совпадения слов запроса";
  for (const id of ["previous-match", "next-match"]) $(id).disabled = !marks.length;
  const unit = { one: "слово", few: "слова", many: "слов", other: "слова" }[plurals.select(wordCount)];
  $("document-meta").textContent = `${number.format(wordCount)} ${unit} · TXT`;
  $("document-status").textContent = totalMatches ? `Совпадений: ${number.format(totalMatches)}` : "Документ открыт целиком";
  return marks[0];
}

export async function openDocument(id, { title = "Документ", query = "", trigger, history = true } = {}) {
  controller?.abort();
  const version = ++generation;
  controller = new AbortController();
  const signal = controller.signal;
  currentId = id;
  currentQuery = query;
  documentData = undefined;
  if (trigger) returnFocus = trigger;
  marks = [];
  activeMatch = -1;
  content.replaceChildren();
  content.hidden = true;
  $("document-title").textContent = title;
  $("document-meta").textContent = "Загружаем текст…";
  $("document-status").textContent = "Загружаем документ";
  $("document-loading").hidden = false;
  $("document-error").hidden = true;
  $("document-matches").hidden = true;
  $("download-document").disabled = true;
  scrollArea.setAttribute("aria-busy", "true");
  scrollArea.scrollTop = 0;
  if (!dialog.open) dialog.showModal();
  if (history && Number.isSafeInteger(id) && id > 0) {
    const next = documentUrl(id, query);
    const state = { ...window.history.state, jdanoffDocument: true };
    if (window.history.state?.jdanoffDocument) window.history.replaceState(state, "", next);
    else window.history.pushState(state, "", next);
  }
  try {
    const result = await getDocument(id, signal);
    if (signal.aborted || version !== generation || !dialog.open) return;
    documentData = result;
    $("document-title").textContent = result.title || "Без названия";
    content.hidden = false;
    $("document-loading").hidden = true;
    renderText(result.text);
    $("download-document").disabled = false;
    if (marks.length) setMatch(0);
    document.dispatchEvent(new CustomEvent("document-connection", { detail: "online" }));
  } catch (error) {
    if (signal.aborted || version !== generation || !dialog.open) return;
    $("document-loading").hidden = true;
    $("document-error").hidden = false;
    $("document-meta").textContent = "Текст недоступен";
    $("document-status").textContent = "Документ не открыт";
    $("document-error-message").textContent = error.message || "Попробуй открыть документ ещё раз.";
    $("retry-document").hidden = error.status === 404 || error.code === "InvalidDocumentId";
    if (error.network) document.dispatchEvent(new CustomEvent("document-connection", { detail: "offline" }));
    else if (error.status) document.dispatchEvent(new CustomEvent("document-connection", { detail: "online" }));
  } finally {
    if (version === generation) scrollArea.setAttribute("aria-busy", "false");
  }
}

function closeDocument(navigate = true) {
  if (!dialog.open) return;
  controller?.abort();
  generation++;
  dialog.close();
  scrollArea.setAttribute("aria-busy", "false");
  documentData = undefined;
  marks = [];
  content.replaceChildren();
  if (navigate) {
    if (window.history.state?.jdanoffDocument) window.history.back();
    else {
      const url = new URL(window.location.href);
      url.searchParams.delete("document");
      window.history.replaceState(window.history.state, "", url);
    }
  }
  const target = returnFocus?.isConnected ? returnFocus : $("query");
  target?.focus({ preventScroll: true });
}

$("close-document").addEventListener("click", () => closeDocument());
dialog.addEventListener("cancel", (event) => {
  event.preventDefault();
  closeDocument();
});
dialog.addEventListener("click", (event) => {
  if (event.target !== dialog) return;
  const rect = dialog.getBoundingClientRect();
  if (event.clientX < rect.left || event.clientX > rect.right ||
      event.clientY < rect.top || event.clientY > rect.bottom) closeDocument();
});
$("retry-document").addEventListener("click", () => void openDocument(currentId, {
  title: $("document-title").textContent, query: currentQuery, history: false,
}));
$("previous-match").addEventListener("click", () => setMatch(activeMatch - 1));
$("next-match").addEventListener("click", () => setMatch(activeMatch + 1));
document.addEventListener("keydown", (event) => {
  if (!dialog.open || !marks.length || event.key !== "F3") return;
  event.preventDefault();
  setMatch(activeMatch + (event.shiftKey ? -1 : 1));
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

function readDocumentRoute() {
  const params = new URLSearchParams(window.location.search);
  const raw = params.get("document");
  if (raw === null) {
    closeDocument(false);
    return;
  }
  const id = /^[1-9]\d*$/.test(raw) ? Number(raw) : NaN;
  void openDocument(id, { query: params.get("q") || "", history: false });
}
window.addEventListener("popstate", readDocumentRoute);
readDocumentRoute();
