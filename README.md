# Lagosystem Bridge

**Standalone, open-source (GNU GPLv3-or-later) cross-platform native bridge for browser applications and local peripherals.**

**Português brasileiro:** [Guia de desenvolvimento](docs/README.pt-BR.md) · **English:** [Developer guide](docs/README.en.md)

This project may carry the Lagosystem name but **does not require any Lagosystem ERP/SaaS account, server, tenant, API or authentication**. Other web applications can use the browser extensions and C++ host after **explicit local user authorization**.

## Components

| Directory | Purpose |
| --- | --- |
| [native/](native/) | C++20 host, CMake/MSVC, serial transport, Windows/Linux/macOS consent dialogs and registration scripts |
| [extensions/chromium/](extensions/chromium/) | Chromium-family Manifest V3 browser extension |
| [extensions/firefox/](extensions/firefox/) | Firefox Manifest V3 browser extension |
| [extensions/shared/](extensions/shared/) | Shared extension logic; synchronized into standalone browser packages |
| [protocol/](protocol/) | Protocol contract and machine-readable schema |
| [docs/](docs/) | Bilingual integration guides, architecture and device-validation plan |

The host uses browser **Native Messaging** (length-prefixed JSON over stdin/stdout), not a local web server. An approved website communicates through \`window.postMessage\` to its extension's content script. Each site and serial device needs the applicable authorization. The C++ process intentionally rejects administrator/root execution.

### Available (experimental) interfaces

\`detect\` (extension-level), \`ping\`, \`capabilities\`, \`devices.list\`, \`devices.paired\`, \`devices.pair\`, \`devices.unpair\`, \`devices.probe\`, \`devices.testPrint\`, and constrained \`exports.save\`.

**Not yet validated:** real hardware compatibility, manufacturer-specific protocols, USB HID, OS printer spoolers, signed releases and end-to-end builds on each OS. A candidate scale weight is *not* a certified measurement; sending printer bytes does *not* prove printing.

## Quick links

- [English: prerequisites, CMake commands, extension installation, JS example](docs/README.en.md)
- [Português BR: requisitos, CMake, extensões, integração JavaScript](docs/README.pt-BR.md)
- [Protocol reference](protocol/README.md)
- [Security policy](SECURITY.md) · [Contributing](CONTRIBUTING.md) · [Authors](AUTHORS.md)
- [Original architecture](docs/architecture.md) · [Hardware-validation checklist](docs/hardware-validation.md)

All software is distributed under **GNU GPL-3.0-or-later** (see [LICENSE](LICENSE)). Copyright attribution is described in [AUTHORS.md](AUTHORS.md).

### Git history migration

This repository contains a reconstructed **path-filtered 82-commit history** extracted from the Bridge files in the former private Lagosystem monorepo. Original commit messages are preserved, and each rebuilt commit has \`Migrated-from\` and \`Original-author\` trailers for traceability. Commit SHAs/dates are not identical because the paths and parent history changed; **no private ERP source or its unrelated commit graph was published**.

No CI, build, automated or physical hardware tests were executed as part of the extraction.