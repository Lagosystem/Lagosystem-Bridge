# Developer guide (English)

Lagosystem Bridge is an **experimental, GPL-3.0-or-later** native messaging bridge for connecting authorized web applications to local devices. Despite its product name, **neither its C++ host nor its browser extensions require Lagosystem accounts, APIs, tenant IDs, or a Lagosystem deployment**.

> Status: source code and synthetic test fixtures exist, but cross-platform builds, signed extension releases and physical hardware interoperability **have not been verified**. Treat all device results as diagnostics, not as certified measurements or proof of a printout.

## Monorepo layout

- `native/`: C++20 Native Messaging host, CMake, Windows MSVC project, platform dialogs, serial transport, installation scripts, fixtures.
- `extensions/chromium/`: Manifest V3 WebExtension for Chromium-based browsers.
- `extensions/firefox/`: Firefox Manifest V3 variant with its own extension ID.
- `protocol/`: protocol contract and message examples. The on-wire channel is `local-device-bridge-v1`.
- `docs/architecture.md` and `docs/hardware-validation.md`: original architecture and hardware validation checklist (Portuguese).
- `docs/README.pt-BR.md`: equivalent Brazilian Portuguese developer instructions.

Each extension directory is independently loadable as an unpacked extension. Common behavior is currently mirrored in the two browser directories; **do not assume the two different browser manifest formats are interchangeable**.

## Prerequisites

- CMake 3.24 or newer and a C++20 toolchain.
- **Windows 10/11**: Visual Studio Build Tools/Visual Studio with MSVC v143 and Windows SDK. The Windows resource manifest is built by CMake.
- **Linux desktop**: GCC or Clang, POSIX serial headers and optional GTK3 development files. GTK3 is needed for the implemented graphical pairing prompts; without GTK3 or an interactive desktop, consent-required operations fail closed.
- **macOS**: Xcode/Apple Clang with Objective-C++ and AppKit. A supported USB-to-serial driver is required for applicable peripherals.
- Internet during the first CMake configure, because `native/CMakeLists.txt` uses CMake FetchContent to download an SHA-256-pinned `nlohmann_json` 3.11.3 archive.

### Build from the repository root (instructions only)

Windows, with the Visual Studio CMake generator:

~~~powershell
cmake -S native -B build/bridge-win-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/bridge-win-x64 --config Release
~~~

Linux:

~~~sh
cmake -S native -B build/bridge-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/bridge-linux
~~~

macOS:

~~~sh
cmake -S native -B build/bridge-macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/bridge-macos
~~~

These are instructions for maintainers, not evidence of successful builds. Windows MSVC's `native/Lagosystem.Bridge.vcxproj` is retained for IDE integration, but **the CMake build is canonical** because its dependencies and platform-specific sources are declared there. On Linux, statically linking libstdc++/libgcc does not eliminate system libc or GTK runtime requirements.

To opt into **offline synthetic fixtures**, configure with `-DLAGOSYSTEM_BRIDGE_BUILD_TESTS=ON` and then run `ctest --test-dir <build-directory> --output-on-failure`. This does **not** test real devices.

## Browser extension + native host installation

1. Load `extensions/chromium/` as an unpacked extension in a compatible Chromium browser, or load `extensions/firefox/` through Firefox extension debugging for development. Production distribution requires browser-specific signing/publication as applicable.
2. Note the **actual Chromium extension ID** (32 characters, a–p). The Firefox extension ID in the source manifest is `bridge@lagosystem.com.br`.
3. Install a compiled native host for the **current standard OS user** (not elevated). Use `native/install-windows.ps1` from a non-elevated PowerShell terminal on Windows, or `native/install-unix.sh` on Linux/macOS. The script arguments must use the absolute path of the host binary and, for Chromium, the actual extension ID.
4. The scripts register the native host identifier `com.lagosystem.bridge` in per-user browser settings. The installer does not build the host, install printer drivers or grant arbitrary permissions.
5. Open your application's HTTPS page. Click the extension popup and explicitly **Approve this site**. HTTP is permitted only for localhost/127.0.0.1 during development.
6. Your web application can now query `ping` and `capabilities`. Each device still requires separate pairing and a local OS-native confirmation.

Chrome, Edge, Brave, Firefox, Opera and browser derivatives may use different registration directories and store policies. The supplied scripts cover only their specifically documented variants; verify each target browser independently. On macOS, Gatekeeper signing/notarization and device driver installation may be necessary.

## Call the extension from an arbitrary website

The site sends ordinary window messages; the extension's content script validates the request and the background checks the actual sender origin and device permissions. **Never expose the OS Native Messaging interface directly to untrusted page JavaScript.**

~~~js
const channel = "local-device-bridge-v1";
function localBridge(action, parameters = {}, timeoutMs = 10000) {
  return new Promise((resolve) => {
    const requestId = crypto.randomUUID();
    const timer = setTimeout(() => finish({ok: false, error: "timeout"}), timeoutMs);
    function receive(event) {
      if (event.source !== window || event.origin !== location.origin) return;
      if (event.data?.channel !== channel ||
          event.data?.direction !== "response" ||
          event.data?.requestId !== requestId) return;
      finish(event.data.payload);
    }
    function finish(result) {
      clearTimeout(timer);
      window.removeEventListener("message", receive);
      resolve(result);
    }
    window.addEventListener("message", receive);
    window.postMessage({
      channel, direction: "request", requestId, action, ...parameters
    }, location.origin);
  });
}
const extension = await localBridge("detect");  // extension installed?
const host = await localBridge("ping");          // approved origin + native host present?
const devices = await localBridge("devices.list");
~~~

If `detect` times out, the content script is probably missing or disabled. If `ping` returns `origin_not_approved`, the user must approve the page **from the extension popup**, not from your web app. If the native host is missing, register it and verify the browser-specific manifest.

For a serial scale, request `devices.pair` with `{deviceId:"COM3",kind:"scale",profile:"ascii-stream",baudRate:9600}` only for a discovered device. The OS dialog must be approved by the local user. `devices.probe` reads a passive sample and may return `candidateWeight` and `unit`; `calibrationVerified` and `stableWeightVerified` remain false. Serial thermal/dot-matrix printer profiles are `escpos` and `escp`, with a second OS confirmation for `devices.testPrint`. A successful byte write does not prove that paper printed.

Other current operations include `devices.paired`, `devices.unpair`, `capabilities`, and `exports.save` (new XML/PDF inside the user's `Lagosystem/Exports` folder; no arbitrary paths). Read the [protocol contract](../protocol/README.md).

## Security, privacy and contributions

No root/administrator host execution. No shell, network listener, arbitrary device paths or privileged configuration actions. Permissions are granted per site and per paired port. The browser-side origin grants are independent of the per-user native port policy. A vendor-specific driver must never disable those checks.

**Do not use generic ASCII weight parsing for legal-for-trade measurements**, fiscal certification or unattended industrial controls. Only transport/parsing diagnostics are available today. Review [hardware validation](hardware-validation.md), [security policy](../SECURITY.md) and [contribution guide](../CONTRIBUTING.md).

License: GNU GPL-3.0-or-later; see [LICENSE](../LICENSE).