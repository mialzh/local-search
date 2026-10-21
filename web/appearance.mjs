const $ = (id) => document.getElementById(id);

function setTheme(theme) {
  const dark = theme === "dark";
  document.documentElement.dataset.theme = theme;
  document.querySelector('meta[name="theme-color"]').content = dark ? "#18181b" : "#f5f5f7";
  $("theme-toggle").setAttribute("aria-pressed", String(dark));
  $("theme-toggle").title = dark ? "Включить светлую тему" : "Включить тёмную тему";
  $("theme-icon").setAttribute("href", dark ? "#i-sun" : "#i-moon");
}
setTheme(document.documentElement.dataset.theme === "dark" ? "dark" : "light");
$("theme-toggle").addEventListener("click", () => {
  const theme = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
  setTheme(theme);
  try { localStorage.setItem("jdanoff-search.theme.v1", theme); } catch { /* The toggle also works without browser storage. */ }
});

const brand = $("brand");
function replayBrand() {
  brand.classList.remove("brand-animate");
  // Restart the two short CSS animations without changing the link's width.
  void brand.offsetWidth;
  brand.classList.add("brand-animate");
}
if (!document.body.classList.contains("document-page")) {
  brand.addEventListener("pointerenter", replayBrand);
  brand.addEventListener("focus", replayBrand);
}

