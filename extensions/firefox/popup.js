// SPDX-License-Identifier: GPL-3.0-or-later
const api = globalThis.browser ?? globalThis.chrome;
const label = document.getElementById("site");
const status = document.getElementById("status");
const grant = document.getElementById("grant");
const revoke = document.getElementById("revoke");
let origin = "";
async function refresh() {
  const [tab] = await api.tabs.query({active:true,currentWindow:true});
  try {
    const u = new URL(tab?.url ?? "");
    if (u.protocol !== "https:" && !(u.protocol === "http:" &&
      ["localhost", "127.0.0.1"].includes(u.hostname))) throw new Error("unsupported");
    origin = u.origin;
  } catch { origin = ""; }
  label.textContent = origin || "Página não permitida";
  grant.disabled = revoke.disabled = !origin;
  const {approvedOrigins} = await api.storage.local.get("approvedOrigins");
  const allowed = Array.isArray(approvedOrigins) ? approvedOrigins : [];
  const active = allowed.includes(origin);
  status.textContent = !origin ? "Abra um site HTTPS válido."
    : active ? "Site autorizado a solicitar operações à ponte local."
    : "Site não autorizado. Nenhum periférico pode ser acessado.";
  grant.disabled = !origin || active;
  revoke.disabled = !origin || !active;
}
async function change(allow) {
  if (!origin) return;
  const {approvedOrigins} = await api.storage.local.get("approvedOrigins");
  const set = new Set(Array.isArray(approvedOrigins) ? approvedOrigins : []);
  if (allow) set.add(origin); else set.delete(origin);
  await api.storage.local.set({approvedOrigins:Array.from(set)});
  await refresh();
}
grant.addEventListener("click",()=>change(true));
revoke.addEventListener("click",()=>change(false));
refresh().catch(()=>{status.textContent="Não foi possível consultar a permissão."});


// The native installer stores a suggested site locally. It never grants access
// to that site: permissions can only be granted by the user in this popup.
const configuredLabel = document.getElementById("configured-site");
const openSite = document.getElementById("open-site");
let configuredUrl = "";
async function loadConfiguredSite() {
  try {
    const result = await api.runtime.sendNativeMessage("com.lagosystem.bridge", {action:"config.get"});
    if (!result?.ok || typeof result.siteUrl !== "string") throw new Error("not_configured");
    const parsed = new URL(result.siteUrl);
    if (parsed.protocol !== "https:" &&
        !(parsed.protocol === "http:" &&
          ["localhost","127.0.0.1"].includes(parsed.hostname))) throw new Error("invalid_url");
    configuredUrl = parsed.href;
    configuredLabel.textContent = configuredUrl;
    openSite.disabled = false;
  } catch {
    configuredLabel.textContent = "Não configurada ou host indisponível";
  }
}
openSite.addEventListener("click", () => {
  if (configuredUrl) api.tabs.create({url:configuredUrl});
});
loadConfiguredSite();
