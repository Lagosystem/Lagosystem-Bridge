/* Browser-specific MV3 background entrypoint. No arbitrary host commands forwarded. */
const runtime = globalThis.browser?.runtime ?? globalThis.chrome.runtime;
const HOST = "com.lagosystem.bridge";
const ALLOWED = new Set(["ping", "capabilities", "exports.save"]);
const TRUSTED_ORIGINS = new Set([
  "https://sistema.lagosystem.com.br",
  "http://localhost:4200"
]);
runtime.onMessage.addListener((message, sender, sendResponse) => {
  try {
    const url = sender.url ?? sender.tab?.url;
    if (!url || !TRUSTED_ORIGINS.has(new URL(url).origin)
      || !message || message.channel !== "lagosystem-bridge-v1"
      || !ALLOWED.has(message.action)) {
      sendResponse({ ok: false, error: "origin_or_operation_denied" });
      return false;
    }
    const clean = { action: message.action };
    if (message.action === "exports.save") {
      if (typeof message.filename !== "string" || typeof message.base64 !== "string"
        || message.base64.length > 1024 * 1024) {
        sendResponse({ ok: false, error: "invalid_export_request" });
        return false;
      }
      clean.filename = message.filename;
      clean.base64 = message.base64;
    }
    runtime.sendNativeMessage(HOST, clean).then(sendResponse).catch(() => {
      sendResponse({ ok: false, error: "native_host_unavailable" });
    });
    return true;
  } catch (_) {
    sendResponse({ ok: false, error: "bridge_error" });
    return false;
  }
});
