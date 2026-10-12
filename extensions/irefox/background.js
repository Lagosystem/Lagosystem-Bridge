// SPDX-License-Identifier: GPL-3.0-or-later
// Generic local-device bridge: no Lagosystem login, backend or cloud dependency.
const runtime = globalThis.browser?.runtime ?? globalThis.chrome.runtime;
const storage = globalThis.browser?.storage ?? globalThis.chrome.storage;
const HOST = "com.lagosystem.bridge"; // Distribution identifier, not a cloud endpoint.
const CHANNEL = "local-device-bridge-v1";
const ALLOWED = new Set(["ping", "capabilities", "devices.list", "devices.paired",
  "devices.pair", "devices.unpair", "devices.probe", "devices.testPrint", "exports.save"]);
const MAX_MESSAGE = 1024 * 1024;

async function originAllowed(origin) {
  // Must be explicitly approved in the extension's own popup (never from page messages).
  const data = await storage.local.get("approvedOrigins");
  const allowed = Array.isArray(data.approvedOrigins) ? data.approvedOrigins : [];
  return allowed.includes(origin);
}

runtime.onMessage.addListener((message, sender, reply) => {
  (async () => {
    const address = sender.url ?? sender.tab?.url;
    if (!address || !message || message.channel !== CHANNEL || !ALLOWED.has(message.action))
      return { ok: false, error: "invalid_request" };
    const url = new URL(address);
    if (!["https:", "http:"].includes(url.protocol) ||
        (url.protocol === "http:" && !["localhost", "127.0.0.1"].includes(url.hostname)) ||
        !await originAllowed(url.origin))
      return { ok: false, error: "origin_not_approved" };
    const request = { action: message.action };
    // Whitelist exact serializable arguments, never pass arbitrary objects/commands.
    if (["devices.pair", "devices.unpair", "devices.probe", "devices.testPrint"].includes(message.action)) {
      if (typeof message.deviceId !== "string" || message.deviceId.length > 180)
        return { ok: false, error: "invalid_device" };
      request.deviceId = message.deviceId;
      if (message.action === "devices.pair") {
        if (!["scale", "thermal", "dotmatrix"].includes(message.kind) ||
            !["ascii-stream", "escpos", "escp"].includes(message.profile))
          return { ok: false, error: "invalid_profile" };
        request.kind = message.kind;
        request.profile = message.profile;
        request.baudRate = message.baudRate;
      }
    }
    if (message.action === "exports.save") {
      if (typeof message.filename !== "string" || typeof message.base64 !== "string"
        || message.base64.length > MAX_MESSAGE)
        return { ok: false, error: "invalid_export" };
      request.filename = message.filename;
      request.base64 = message.base64;
    }
    try { return await runtime.sendNativeMessage(HOST, request); }
    catch { return { ok: false, error: "native_host_unavailable" }; }
  })().then(reply).catch(() => reply({ ok: false, error: "bridge_error" }));
  return true;
});
