import { ApiError, checkHealth, queryWords, searchDocuments, uploadDocument } from "./api.mjs";

const $ = (id) => document.getElementById(id);
const query = $("query");
const limit = $("limit");
const results = $("results");
const input = $("file-input");
const dropZone = $("drop-zone");
const number = new Intl.NumberFormat("ru-RU");
const scoreFormat = new Intl.NumberFormat("ru-RU", { maximumFractionDigits: 4 });
const plural = new Intl.PluralRules("ru-RU");
let searchController;
let searchVersion = 0;
let lastQuery = null;
let healthPending = false;
let connectionVersion = 0;
let uploadRunning = false;
let uploadedCount = 0;
let nextUploadId = 0;
const uploadQueue = [];

function icon(name, extraClass = "") {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("class", `icon ${extraClass}`.trim());
  svg.setAttribute("aria-hidden", "true");
  const use = document.createElementNS("http://www.w3.org/2000/svg", "use");
  use.setAttribute("href", `#i-${name}`);
  svg.append(use);
  return svg;
}

function element(tag, className, text) {
  const node = document.createElement(tag);
  node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function setConnection(status) {
  connectionVersion++;
  $("connection").dataset.status = status;
  $("connection-label").textContent = {
    online: "Сервер подключён", offline: "Нет соединения", checking: "Проверяем",
  }[status];
  $("connection-notice").hidden = status !== "offline";
}

async function refreshConnection() {
  if (healthPending) return;
  healthPending = true;
  setConnection("checking");
  const version = connectionVersion;
  try {
    const online = await checkHealth();
    if (version === connectionVersion) setConnection(online ? "online" : "offline");
  } finally {
    healthPending = false;
  }
}

function appendSnippet(container, snippet, words) {
  // All server-provided content is rendered as text. Uploaded text never becomes HTML.
  const pattern = /[\p{L}\p{Nd}][\p{L}\p{Nd}\p{M}]*/gu;
  let offset = 0;
  for (const match of snippet.matchAll(pattern)) {
    container.append(document.createTextNode(snippet.slice(offset, match.index)));
    const token = match[0];
    container.append(words.has(token.normalize("NFC").toLowerCase())
      ? element("mark", "", token) : document.createTextNode(token));
    offset = match.index + token.length;
  }
  container.append(document.createTextNode(snippet.slice(offset)));
}

function showEmpty(title, message) {
  const empty = element("div", "empty-state");
  const symbol = element("div", "empty-symbol");
  symbol.append(icon("search"));
  empty.append(symbol, element("h3", "", title), element("p", "", message));
  results.replaceChildren(empty);
}

function showResults(items, text, milliseconds) {
  results.replaceChildren();
  const words = queryWords(text);
  for (const item of items) {
    const article = element("article", "result-item");
    const fileIcon = element("div", "result-icon");
    fileIcon.append(icon("file"));
    const content = element("div", "result-content");
    const top = element("div", "result-top");
    const score = element("span", "result-score", `BM25 ${scoreFormat.format(item.score)}`);
    score.title = "Оценка соответствия запросу. Это не процент уверенности.";
    top.append(element("h3", "result-title", item.title), score);
    const snippet = element("p", "result-snippet");
    appendSnippet(snippet, item.snippet, words);
    content.append(top, snippet, element("span", "result-id", `ДОКУМЕНТ #${item.document_id}`));
    article.append(fileIcon, content);
    results.append(article);
  }
  $("results-title").textContent = "Результаты поиска";
  $("results-meta").textContent = `${number.format(items.length)} в выдаче · ${number.format(Math.round(milliseconds))} мс`;
  $("results-meta").title = "Время запроса вместе с передачей данных. Выдача ограничена выбранным количеством.";
  $("results-status").textContent = `По запросу «${text}» получено результатов: ${items.length}.`;
  if (!items.length) {
    showEmpty("Совпадений пока нет", "Попробуй другие слова или добавь документ с нужным текстом.");
  }
}

async function runSearch() {
  const text = query.value.trim();
  searchController?.abort();
  const controller = new AbortController();
  searchController = controller;
  const version = ++searchVersion;
  const errorBox = $("search-error");
  errorBox.hidden = true;
  $("results-section").setAttribute("aria-busy", "true");
  $("search-button").setAttribute("aria-busy", "true");
  $("results-title").textContent = "Ищем в документах";
  $("results-meta").textContent = "";
  $("results-status").textContent = "Поиск выполняется.";
  const loading = element("div", "loading-state");
  loading.append(element("span", "spinner"), document.createTextNode("Ищем совпадения…"));
  results.replaceChildren(loading);
  const start = performance.now();
  try {
    const items = await searchDocuments(text, Number(limit.value), controller.signal);
    if (version !== searchVersion) return;
    setConnection("online");
    lastQuery = text;
    showResults(items, text, performance.now() - start);
  } catch (error) {
    if (version !== searchVersion || controller.signal.aborted) return;
    if (error.network) setConnection("offline");
    else if (error.status) setConnection("online");
    errorBox.textContent = error.message || "Не удалось выполнить поиск.";
    errorBox.hidden = false;
    $("results-title").textContent = "Поиск не выполнен";
    $("results-status").textContent = "Поиск не выполнен.";
    showEmpty("Попробуем ещё раз?", "Уточни запрос или проверь соединение с сервером.");
  } finally {
    if (version === searchVersion) {
      $("results-section").setAttribute("aria-busy", "false");
      $("search-button").removeAttribute("aria-busy");
    }
  }
}

function setUploadState(entry, state, message) {
  entry.node.dataset.state = state;
  entry.status.textContent = message;
  entry.indicator.replaceChildren();
  if (state === "done") entry.indicator.append(icon("check", "upload-indicator"));
  if (state === "uploading") entry.indicator.append(element("span", "spinner"));
}

async function drainUploads() {
  if (uploadRunning) return;
  uploadRunning = true;
  let added = false;
  try {
    while (uploadQueue.length) {
      const entry = uploadQueue.shift();
      setUploadState(entry, "uploading", "Загружаем и индексируем…");
      try {
        const response = await uploadDocument(entry.file);
        setConnection("online");
        const wordLabel = { one: "слово", few: "слова", many: "слов", other: "слова" }[plural.select(response.word_count)];
        setUploadState(entry, "done", `${number.format(response.word_count)} ${wordLabel} · ID ${response.document_id} · в поиске`);
        entry.name.textContent = response.title;
        $("upload-count").textContent = number.format(++uploadedCount);
        $("upload-live").textContent = `Документ ${response.title} загружен и доступен в поиске.`;
        added = true;
        const welcome = $("welcome");
        if (welcome) {
          welcome.querySelector("h3").textContent = "Первый шаг сделан";
          welcome.querySelector("p").textContent = "Введи слова из текста документа в строку поиска выше.";
          $("first-upload").hidden = true;
        }
      } catch (error) {
        if (error.network) setConnection("offline");
        else if (error.status) setConnection("online");
        const message = error instanceof ApiError ? error.message : "Не удалось прочитать или загрузить файл.";
        setUploadState(entry, "error", error.network
          ? `${message} Ответ на загрузку не получен: проверь документ через поиск перед повторной отправкой.` : message);
        $("upload-live").textContent = `Не удалось загрузить ${entry.file.name}. ${message}`;
      } finally {
        // Keep only displayed metadata after processing, not the file contents.
        entry.file = null;
      }
    }
  } finally {
    uploadRunning = false;
  }
  if (added && lastQuery !== null && query.value.trim() === lastQuery) await runSearch();
}

function queueFiles(files) {
  for (const file of files) {
    const node = element("li", "upload-item");
    node.id = `upload-${++nextUploadId}`;
    const description = element("div", "upload-description");
    const name = element("p", "upload-name", file.name);
    const status = element("p", "upload-status", "В очереди");
    const indicator = element("span", "");
    description.append(name, status);
    node.append(icon("file"), description, indicator);
    $("uploads").prepend(node);
    uploadQueue.push({ file, node, name, status, indicator });
  }
  $("no-uploads").hidden = $("uploads").children.length > 0;
  void drainUploads();
}

$("search-form").addEventListener("submit", (event) => {
  event.preventDefault();
  void runSearch();
});
limit.addEventListener("change", () => {
  if (lastQuery !== null && query.value.trim()) void runSearch();
});
$("choose-files").addEventListener("click", () => input.click());
$("first-upload").addEventListener("click", () => input.click());
input.addEventListener("change", () => {
  queueFiles(Array.from(input.files));
  input.value = "";
});
let dragDepth = 0;
dropZone.addEventListener("dragenter", (event) => {
  event.preventDefault();
  dragDepth++;
  dropZone.classList.add("dragging");
});
dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  if (event.dataTransfer) event.dataTransfer.dropEffect = "copy";
});
dropZone.addEventListener("dragleave", () => {
  if (--dragDepth <= 0) {
    dragDepth = 0;
    dropZone.classList.remove("dragging");
  }
});
dropZone.addEventListener("drop", (event) => {
  event.preventDefault();
  dragDepth = 0;
  dropZone.classList.remove("dragging");
  queueFiles(Array.from(event.dataTransfer?.files ?? []));
});
// Dropping outside the target must not navigate the browser away from the app.
document.addEventListener("dragover", (event) => event.preventDefault());
document.addEventListener("drop", (event) => event.preventDefault());
document.addEventListener("keydown", (event) => {
  if (event.key === "/" && !event.ctrlKey && !event.metaKey && !event.altKey &&
      !event.target.closest("input, textarea, select, [contenteditable='true']")) {
    event.preventDefault();
    query.focus();
  }
});
$("connection").addEventListener("click", () => void refreshConnection());
window.addEventListener("online", () => void refreshConnection());
window.addEventListener("offline", () => setConnection("offline"));
document.addEventListener("visibilitychange", () => {
  if (!document.hidden) void refreshConnection();
});
if (window.location.protocol === "file:") {
  setConnection("offline");
  $("connection-notice").textContent = "Открой http://localhost:8080 после запуска сервера. Страница не подключается к API при открытии HTML-файла напрямую.";
} else {
  void refreshConnection();
}
