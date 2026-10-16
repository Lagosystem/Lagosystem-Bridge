// SPDX-License-Identifier: GPL-3.0-or-later
// Generic webpage adapter. Permission decisions live exclusively in the extension background.
const CHANNEL = "local-device-bridge-v1";
const VALID = new Set(["detect", "ping", "capabilities", "devices.list", "devices.paired",
  "devices.pair", "devices.unpair", "devices.probe", "devices.testPrint", "exports.save", "exports.folders"]);
const origin = location.origin;
window.addEventListener("message", (event) => {
  if (event.source !== window || event.origin !== origin) return;
  const request = event.data;
  if (!request || request.channel !== CHANNEL || request.direction !== "request"
      || typeof request.requestId !== "string" || request.requestId.length > 100
      || !VALID.has(request.action)) return;
  if (request.action === "detect") {
    window.postMessage({ channel: CHANNEL, direction: "response", requestId: request.requestId,
      payload: {ok:true, extensionInstalled:true, bridgeVersion:"0.2.0"} }, origin);
    return;
  }
  const runtime = globalThis.browser?.runtime ?? globalThis.chrome.runtime;
  const payload = { channel: CHANNEL, action: request.action, deviceId: request.deviceId,
    kind: request.kind, profile: request.profile, baudRate: request.baudRate,
    filename: request.filename, base64: request.base64, selectDirectory: request.selectDirectory, folderId: request.folderId };
  runtime.sendMessage(payload).then((response) => {
    window.postMessage({ channel: CHANNEL, direction: "response",
      requestId: request.requestId, payload: response }, origin);
  }).catch(() => {
    window.postMessage({ channel: CHANNEL, direction: "response",
      requestId: request.requestId, payload: { ok: false, error: "extension_unavailable" } }, origin);
  });
});
