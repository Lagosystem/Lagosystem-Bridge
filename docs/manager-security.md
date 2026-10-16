# Native policy threat model / Segurança do Bridge Manager

## Trust boundaries

- **Untrusted webpage**: arbitrary JavaScript and even XSS at a previously trusted origin. It may only request the limited protocol commands handled by the WebExtension.
- **Browser content script**: uses `window.postMessage` with same-window/same-origin checks, no privileged DOM-eval.
- **Trusted extension background**: derives `siteOrigin` solely from the browser-owned `sender.url` (never from the webpage payload). Checks popup `approvedOrigins` and `perOriginDevices`. The browser identifies the **extension**, not the website, to its Native Messaging Host; this origin claim is trusted only because the installed extension is an explicitly allowed publisher in the Native Messaging manifest. A malicious/replaced extension remains a high-impact threat.
- **Native host**: independently checks SQLite origin, capability, resource, and folder ID. Fail-closed for missing policy or unknown actions. No browser message can call `policy::GrantAction`, `AddSite`, or change policy.
- **Bridge Manager**: separate wxWidgets GUI within the same OS account. Its code alone calls mutation functions. Site origins start disabled; grants are exact (no wildcard for device identifiers), and changing origins destroys grants.
- **Filesystem**: only existing new-file XML/PDF export, with O_EXCL / CREATE_NEW and protected-path checks. No general filesystem read, delete, overwrite or web-supplied path. Folder grants carry opaque numeric IDs, not paths. Untrusted symlinks and reparse points must be reviewed on each platform.
- **Hardware**: serial port grants per website and operation. Native consent remains required for device pairing and printing. Device names such as COM3 can be reassigned; persistent physical identity verification still requires OS-specific implementation.

## Identity, limitations and authenticity

The OS account provides the basic isolation boundary, not a custom Lagosystem password. GUI Yes/No prompts request user presence but **do not prove Windows Hello, Touch ID or PAM authentication**. Same-user malware can manipulate SQLite policy files and inject into same-user processes; DPAPI or Keychain applied within the same account would not fully solve that. Avoid promising strong same-user malware isolation. A separately protected broker with OS-mediated access controls may require a privileged one-time installation; it is not included.

The installer verifies artifact SHA-256 against the adjacent release checksum. This verifies integrity only when that checksum is independently trustworthy; publication signing/attestation remains an outstanding milestone.

The native host is a stdin/stdout process launched on demand by the browser. Never register it as an autostart service. The GUI Manager is user-launched from Start Menu / XDG / macOS Applications.

Audit entries log native allow/deny policy decisions, not proof that a serial write or export completed. Windows paths are stored as UTF-8 in SQLite; disk access continues to use OS-native paths.

## Future review before distribution

1. Obtain specific approval to run all existing CI/build/tests and update fixture expectations where needed.
2. Manually validate Windows 10/11 x64; Linux GNOME/KDE/XFCE; macOS Apple Silicon, including actual SQLite and wxWidgets packaging.
3. Fuzz origin parsing, port names, policy corruption, DB symlink attacks and folder races.
4. Implement OS-mediated strong reauthentication if required, assessing each platform's installed agent and accessibility.
5. Sign/notarize binary releases; review store extension IDs and all browser variants.
6. Never interpret synthetic serial fixtures as legal-for-trade certification or fiscal printer homologation.
