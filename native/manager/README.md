# Lagosystem Bridge Manager

This executable is an independent wxWidgets GUI for local, per-user SQLite permissions. Unlike the Native Messaging stdio host, it can grant, revoke, disable and remove sites. No web message can call its editing API. Opening a site is denied by default; each capability is separately granted.

**Limitation:** human confirmation uses GUI dialogs and the OS session; OS-level reauthentication (Windows Hello, macOS LocalAuthentication, Linux polkit/PAM) is not yet implemented. Same-user malware can tamper with same-user data. A privileged broker would be required to protect against that threat model.

**No arbitrary file read, overwrite, deletion, or shell execution** is granted. Directory grants are export-only, using the existing safe new-file exporter. Native Messaging uses on-demand stdio and must never be configured as an autostart daemon.

Build after installing platform-specific wxWidgets and SQLite3 development packages, enabling `-DLAGOSYSTEM_BRIDGE_BUILD_MANAGER=ON`. See the root documentation for requirements.