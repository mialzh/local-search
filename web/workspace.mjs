import "./appearance.mjs?v=2.9.0";

const $ = (id) => document.getElementById(id);
const dialog = $("upload-dialog");

export function openUploadDialog() {
  if (!dialog.open) dialog.showModal();
}

$("open-upload").addEventListener("click", openUploadDialog);
for (const id of ["close-upload", "finish-upload"]) {
  $(id).addEventListener("click", () => dialog.close());
}
dialog.addEventListener("click", (event) => {
  if (event.target !== dialog) return;
  const rect = dialog.getBoundingClientRect();
  if (event.clientX < rect.left || event.clientX > rect.right ||
      event.clientY < rect.top || event.clientY > rect.bottom) dialog.close();
});

function setCompact(compact) {
  document.body.classList.toggle("compact", compact);
  $("density-toggle").setAttribute("aria-pressed", String(compact));
}
try { setCompact(localStorage.getItem("local-search.compact.v1") === "true"); }
catch { setCompact(false); }
$("density-toggle").addEventListener("click", () => {
  const compact = !document.body.classList.contains("compact");
  setCompact(compact);
  try { localStorage.setItem("local-search.compact.v1", String(compact)); } catch { /* Optional preference. */ }
});

document.addEventListener("keydown", (event) => {
  if (document.querySelector("dialog[open]")) return;
  const command = (event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k" && !event.altKey;
  const slash = event.key === "/" && !event.ctrlKey && !event.metaKey && !event.altKey &&
    !event.target.closest?.("input, textarea, select, [contenteditable='true']");
  if (command || slash) {
    event.preventDefault();
    $("query").focus();
  }
});

