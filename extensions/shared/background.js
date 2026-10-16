// SPDX-License-Identifier: GPL-3.0-or-later
// Generic local-device bridge: no Lagosystem login, backend or cloud dependency.
const runtime = globalThis.browser?.runtime ?? globalThis.chrome.runtime;
const storage = globalThis.browser?.storage ?? globalThis.chrome.storage;
const HOST = "com.lagosystem.bridge"; // Distribution identifier, not a cloud endpoint.
const CHANNEL = "local-device-bridge-v1";
const ALLOWED = new Set(["ping", "capabilities", "devices.list", "devices.paired",
  "devices.pair", "devices.unpair", "devices.probe", "devices.testPrint", "exports.save", "exports.folders"]);
const MAX_MESSAGE = 1024 * 1024;

async function originAllowed(origin) {
  // Must be explicitly approved in the extension's own popup (never from page messages).
  const data = await storage.local.get("approvedOrigins");
  const allowed = Array.isArray(data.approvedOrigins) ? data.approvedOrigins : [];
  return allowed.includes(origin);
}

runtime.onMessage.addListener((message, sender, reply) => {
  (async () => {
    const address = sender.url; // Only content-script sender origin, not a tab fallback.
    if (!address || !message || message.channel !== CHANNEL || !ALLOWED.has(message.action))
      return { ok: false, error: "invalid_request" };
    const url = new URL(address);
    if (!["https:", "http:"].includes(url.protocol) ||
        (url.protocol === "http:" && !["localhost", "127.0.0.1"].includes(url.hostname)) ||
        !await originAllowed(url.origin))
      return { ok: false, error: "origin_not_approved" };
    const request = { action: message.action, siteOrigin: url.origin }; // Never read siteOrigin from page payload.
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
      if (message.selectDirectory !== undefined && typeof message.selectDirectory !== "boolean")
        return { ok: false, error: "invalid_export" };
      request.selectDirectory = message.selectDirectory === true;
      if (message.folderId !== undefined) {
        if (!Number.isSafeInteger(message.folderId) || message.folderId < 1)
          return {ok:false,error:"invalid_export"};
        request.folderId = message.folderId;
      }
    }
    const saved = await storage.local.get("perOriginDevices");
    const grants = saved.perOriginDevices && typeof saved.perOriginDevices === "object"
      ? saved.perOriginDevices : {};
    const own = Array.isArray(grants[url.origin]) ? grants[url.origin] : [];
    if (["devices.probe", "devices.testPrint", "devices.unpair"].includes(message.action)
        && !own.includes(request.deviceId))
      return {ok:false,error:"device_not_paired_for_site"};
    try {
      const result = await runtime.sendNativeMessage(HOST, request);
      if (message.action === "devices.paired" && result?.ok) {
        return {...result, devices:(Array.isArray(result.devices) ? result.devices : [])
          .filter((item) => own.includes(item.deviceId))};
      }
      if (result?.ok && message.action === "devices.pair") {
        grants[url.origin] = Array.from(new Set([...own, request.deviceId]));
        await storage.local.set({perOriginDevices:grants});
      }
      if (result?.ok && message.action === "devices.unpair") {
        grants[url.origin] = own.filter(item => item !== request.deviceId);
        await storage.local.set({perOriginDevices:grants});
      }
      return result;
    } catch { return {ok:false,error:"native_host_unavailable"}; }
  })().then(reply).catch(() => reply({ ok: false, error: "bridge_error" }));
  return true;
});
