import { ApiError, checkHealth, queryWords, searchDocuments, uploadDocument } from "./api.mjs?v=2.9.0";
import { openUploadDialog } from "./workspace.mjs?v=2.9.0";

const $ = (id) => document.getElementById(id);
const query = $("query");
const limit = $("limit");
const results = $("results");
const input = $("file-input");
const dropZone = $("drop-zone");
const number = new Intl.NumberFormat("ru-RU");
let searchController;
let searchVersion = 0;
let lastQuery = null;
let healthPending = false;
let connectionVersion = 0;
let uploadRunning = false;
let batchTotal = 0;
let batchProcessed = 0;
let batchAdded = 0;
let uploadLabelTimer;
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
  $("connection").title = $("connection-label").textContent + ". Нажми, чтобы проверить соединение.";
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
    const heading = element("h3", "result-title");
    const link = element("a", "result-link", item.title);
    const documentUrl = new URL("/document.html", window.location.origin);
    documentUrl.search = new URLSearchParams({ id: String(item.document_id), q: text, limit: limit.value });
    link.href = documentUrl.pathname + documentUrl.search;
    link.target = "_blank";
    link.rel = "noopener";
    link.title = "Открыть документ в новой вкладке";
    heading.append(link);
    top.append(heading);
    const snippet = element("p", "result-snippet");
    appendSnippet(snippet, item.snippet, words);
    content.append(top, snippet);
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

function clearQueryFeedback() {
  query.removeAttribute("aria-invalid");
  $("search-form").classList.remove("query-empty");
}

async function runSearch() {
  const text = query.value.trim();
  if (!text) {
    query.focus();
    query.setAttribute("aria-invalid", "true");
    const form = $("search-form");
    form.classList.remove("query-empty");
    // Replay only the border and glow; all dimensions stay unchanged.
    void form.offsetWidth;
    form.classList.add("query-empty");
    $("results-status").textContent = "Введи запрос для поиска.";
    return;
  }
  clearQueryFeedback();
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

async function drainUploads() {
  if (uploadRunning) return;
  uploadRunning = true;
  let added = false;
  clearTimeout(uploadLabelTimer);
  $("open-upload").setAttribute("aria-busy", "true");
  try {
    while (uploadQueue.length) {
      const file = uploadQueue.shift();
      $("upload-summary").textContent = `Загрузка ${batchProcessed + 1} из ${batchTotal}`;
      $("upload-button-label").textContent = `Загрузка ${batchProcessed + 1}/${batchTotal}`;
      $("upload-detail").textContent = file.name;
      $("upload-progress").max = batchTotal;
      try {
        await uploadDocument(file);
        setConnection("online");
        batchAdded++;
        added = true;
        const welcome = $("welcome");
        if (welcome) {
          welcome.querySelector("h3").textContent = "Документы добавлены";
          welcome.querySelector("p").textContent = "Введи слова из текста в строку поиска.";
        }
      } catch (error) {
        if (error.network) setConnection("offline");
        else if (error.status) setConnection("online");
        let message = error instanceof ApiError ? error.message : "Не удалось прочитать или загрузить файл.";
        if (error.network) message += " Ответ не получен: проверь документ через поиск перед повторной отправкой.";
        $("upload-errors").append(element("p", "", `${file.name}: ${message}`));
        $("upload-errors").hidden = false;
      } finally {
        batchProcessed++;
        $("upload-progress").value = batchProcessed;
      }
    }
  } finally {
    uploadRunning = false;
    $("open-upload").removeAttribute("aria-busy");
    $("upload-summary").textContent = `Добавлено: ${batchAdded} из ${batchTotal}`;
    $("upload-detail").textContent = batchAdded === batchTotal
      ? "Документы доступны в поиске." : "Проверь файлы с ошибками ниже.";
    $("upload-button-label").textContent = batchAdded > 0 ? `Добавлено: ${batchAdded}` : "Проверь файлы";
    uploadLabelTimer = setTimeout(() => {
      $("upload-button-label").textContent = "Добавить файлы";
    }, 4000);
  }
  if (added && lastQuery !== null && query.value.trim() === lastQuery) await runSearch();
}

function queueFiles(files) {
  if (!files.length) return;
  if (!uploadRunning) {
    batchTotal = 0;
    batchProcessed = 0;
    batchAdded = 0;
    $("upload-errors").replaceChildren();
    $("upload-errors").hidden = true;
    $("upload-progress").value = 0;
  }
  batchTotal += files.length;
  $("upload-progress").max = batchTotal;
  uploadQueue.push(...files);
  $("upload-feedback").hidden = false;
  openUploadDialog();
  void drainUploads();
}

query.addEventListener("input", clearQueryFeedback);
query.addEventListener("blur", clearQueryFeedback);
query.addEventListener("keydown", (event) => {
  if (event.key === "Escape") clearQueryFeedback();
});
$("search-form").addEventListener("submit", (event) => {
  event.preventDefault();
  void runSearch();
});
limit.addEventListener("change", () => {
  if (lastQuery !== null && query.value.trim()) void runSearch();
});
$("choose-files").addEventListener("click", () => input.click());
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
  const initialParams = new URLSearchParams(window.location.search);
  const initialLimit = initialParams.get("limit");
  if (Array.from(limit.options).some((option) => option.value === initialLimit)) limit.value = initialLimit;
  const initialQuery = initialParams.get("q");
  if (initialQuery) {
    query.value = initialQuery;
    void runSearch();
  }
}


