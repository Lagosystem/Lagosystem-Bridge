/* Only injected on trusted configured SaaS origins; never expose chrome.runtime to the page. */
const origin = location.origin;
const channel = "lagosystem-bridge-v1";
window.addEventListener("message", (event) => {
  if (event.source !== window || event.origin !== origin) return;
  const input = event.data;
  if (!input || typeof input !== "object" || input.channel !== channel
    || input.direction !== "request" || typeof input.requestId !== "string"
    || input.requestId.length > 100) return;
  const allowed = ["ping", "capabilities", "exports.save"];
  if (!allowed.includes(input.action)) return;
  const runtime = globalThis.browser?.runtime ?? globalThis.chrome.runtime;
  runtime.sendMessage({
    channel, action: input.action, filename: input.filename, base64: input.base64
  }).then((response) => {
    window.postMessage({ channel, direction: "response", requestId: input.requestId,
      payload: response }, origin);
  }).catch(() => {
    window.postMessage({ channel, direction: "response", requestId: input.requestId,
      payload: { ok: false, error: "bridge_unavailable" } }, origin);
  });
});
