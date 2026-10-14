const MAX_FILE_SIZE = 5 * 1024 * 1024;
const encoder = new TextEncoder();

const messages = {
  InvalidRequest: "Сервер не смог принять файл. Проверь имя и попробуй ещё раз.",
  InvalidText: "Нужен непустой текст в UTF-8, содержащий хотя бы одно слово, без нулевых символов.",
  InvalidQuery: "Введи хотя бы одно слово. Запрос должен быть не длиннее 2048 байт.",
  InvalidLimit: "Количество результатов должно быть целым числом от 1 до 100.",
  DocumentTooLarge: "Файл слишком большой. Максимальный размер — 5 МБ.",
  UnsupportedFileType: "Поддерживаются только файлы с расширением .txt.",
  InternalError: "На сервере произошла ошибка. Попробуй ещё раз позже.",
};

export class ApiError extends Error {
  constructor(message, { code = "", status = 0, network = false } = {}) {
    super(message);
    this.name = "ApiError";
    this.code = code;
    this.status = status;
    this.network = network;
  }
}

export function validateQuery(query, limit) {
  if (!query.trim() || encoder.encode(query).length > 2048 || query.includes("\0")) {
    throw new ApiError(messages.InvalidQuery, { code: "InvalidQuery" });
  }
  if (!Number.isInteger(limit) || limit < 1 || limit > 100) {
    throw new ApiError(messages.InvalidLimit, { code: "InvalidLimit" });
  }
}

export async function validateFile(file) {
  if (!/\.txt$/i.test(file.name)) {
    throw new ApiError(messages.UnsupportedFileType, { code: "UnsupportedFileType" });
  }
  if (file.size > MAX_FILE_SIZE) {
    throw new ApiError(messages.DocumentTooLarge, { code: "DocumentTooLarge" });
  }
  if (!file.name || encoder.encode(file.name).length > 255 || file.name.includes("\0")) {
    throw new ApiError("Имя файла должно занимать не более 255 байт и не содержать нулевых символов.", {
      code: "InvalidRequest",
    });
  }
  let text;
  try {
    text = new TextDecoder("utf-8", { fatal: true }).decode(await file.arrayBuffer());
  } catch {
    throw new ApiError("Не удалось прочитать текст в UTF-8. Проверь кодировку файла.", {
      code: "InvalidText",
    });
  }
  if (!text.trim() || text.includes("\0")) {
    throw new ApiError(messages.InvalidText, { code: "InvalidText" });
  }
}

async function request(path, { timeout = 15000, signal, ...options } = {}) {
  const controller = new AbortController();
  const abort = () => controller.abort();
  if (signal?.aborted) abort();
  signal?.addEventListener("abort", abort, { once: true });
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    controller.abort();
  }, timeout);
  try {
    const response = await fetch(path, { ...options, signal: controller.signal, cache: "no-store" });
    let body;
    try {
      body = await response.json();
    } catch (error) {
      if (controller.signal.aborted) throw error;
      throw new ApiError("Сервер вернул неожиданный ответ. Проверь, что открыта страница Local Search.", {
        status: response.status,
      });
    }
    if (!response.ok) {
      const code = body?.error?.code ?? "";
      throw new ApiError(messages[code] ?? `Не удалось выполнить запрос (HTTP ${response.status}).`, {
        code, status: response.status,
      });
    }
    return body;
  } catch (error) {
    if (signal?.aborted) throw error;
    if (error instanceof ApiError) throw error;
    const message = timedOut
      ? "Сервер не ответил вовремя. Проверь соединение и попробуй ещё раз."
      : "Не удалось связаться с сервером. Проверь, что он запущен.";
    throw new ApiError(message, { network: true });
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener("abort", abort);
  }
}

export async function searchDocuments(query, limit, signal) {
  validateQuery(query, limit);
  const body = await request(`/search?${new URLSearchParams({ q: query, limit: String(limit) })}`, { signal });
  if (!Array.isArray(body?.results) || body.results.some((item) =>
    !item || !Number.isSafeInteger(item.document_id) || item.document_id <= 0 ||
    typeof item.title !== "string" || typeof item.snippet !== "string" ||
    typeof item.score !== "number" || !Number.isFinite(item.score))) {
    throw new ApiError("Сервер вернул неправильный формат результатов.", { status: 200 });
  }
  return body.results;
}

export async function uploadDocument(file) {
  await validateFile(file);
  const form = new FormData();
  // Browser sets Content-Type together with the correct multipart boundary.
  form.append("file", file, file.name);
  const body = await request("/documents", { method: "POST", body: form, timeout: 60000 });
  if (!Number.isSafeInteger(body?.document_id) || body.document_id <= 0 ||
      typeof body.title !== "string" || !Number.isSafeInteger(body.word_count) || body.word_count < 1) {
    throw new ApiError("Документ отправлен, но сервер вернул неожиданный ответ. Проверь его через поиск.", {
      status: 201,
    });
  }
  return body;
}

export async function checkHealth() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 4000);
  try {
    const response = await fetch("/health", { cache: "no-store", signal: controller.signal });
    return response.ok && (await response.text()).trim() === "ok";
  } catch {
    return false;
  } finally {
    clearTimeout(timer);
  }
}

export function queryWords(query) {
  return new Set(query.normalize("NFC").toLowerCase().match(/[\p{L}\p{Nd}][\p{L}\p{Nd}\p{M}]*/gu) ?? []);
}
