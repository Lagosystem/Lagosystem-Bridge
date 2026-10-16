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
const configuredSites = [];
async function loadConfiguredSite() {
  try {
    const result = await api.runtime.sendNativeMessage("com.lagosystem.bridge", {action:"config.list"});
    if (!result?.ok || !Array.isArray(result.sites)) throw new Error("not_configured");
    for (const site of result.sites) {
      if (!site.enabled || typeof site.url !== "string" || typeof site.name !== "string") continue;
      const candidate = new URL(site.url);
      if (candidate.protocol !== "https:" && !(candidate.protocol === "http:" &&
          ["localhost", "127.0.0.1"].includes(candidate.hostname))) continue;
      configuredSites.push({name:site.name, url:candidate.href});
    }
    const selector = document.getElementById("configured-sites");
    selector.textContent = "";
    configuredSites.forEach((site, index) => {
      const option = document.createElement("option");
      option.value = String(index);
      option.textContent = site.name + " (" + site.url + ")";
      selector.append(option);
    });
    if (configuredSites.length === 0) throw new Error("not_configured");
    configuredUrl = configuredSites[0].url;
    configuredLabel.textContent = configuredUrl;
    openSite.disabled = false;
    selector.disabled = false;
    selector.addEventListener("change", () => {
      const index = Number(selector.value);
      configuredUrl = configuredSites[index]?.url ?? "";
      configuredLabel.textContent = configuredUrl || "Nenhuma URL";
      openSite.disabled = !configuredUrl;
    });
  } catch {
    configuredLabel.textContent = "Não configurada ou host indisponível";
  }
}
openSite.addEventListener("click", () => {
  if (configuredUrl) api.tabs.create({url:configuredUrl});
});
loadConfiguredSite();
