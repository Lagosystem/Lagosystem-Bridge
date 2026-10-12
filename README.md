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

The host uses browser **Native Messaging** (length-prefixed JSON over stdin/stdout), not a local web server. An approved website communicates through `window.postMessage` to its extension's content script. Each site and serial device needs the applicable authorization. The C++ process intentionally rejects administrator/root execution.

### Available (experimental) interfaces

`detect` (extension-level), `ping`, `capabilities`, `devices.list`, `devices.paired`, `devices.pair`, `devices.unpair`, `devices.probe`, `devices.testPrint`, and constrained `exports.save`.

**Not yet validated:** real hardware compatibility, manufacturer-specific protocols, USB HID, OS printer spoolers, signed releases and end-to-end builds on each OS. A candidate scale weight is *not* a certified measurement; sending printer bytes does *not* prove printing.

## Community / Comunidade

- [Open a structured Issue / Abrir Issue com formulário](https://github.com/leonardo-nicolas/Lagosystem-Bridge/issues/new/choose) — bug, feature, hardware compatibility, protocol or driver.
- [Discussions / Discussões](https://github.com/leonardo-nicolas/Lagosystem-Bridge/discussions) — questions, ideas and integration support; general questions should not become bug reports.
- [Security policy / Política de segurança](SECURITY.md) — report vulnerabilities privately, never as a public Issue.

## Manual CI / CI manual

[CI/CD cost controls, execution steps and cleanup / Controles de custos, execução e limpeza](docs/CI.md). The GitHub Actions matrix runs only after explicit manual authorization, on standard public runners, without persistent caches or artifacts. / A matriz executa apenas com autorização manual, runners padrão públicos e nenhum cache ou artefato persistido.

## Beta release preparation / Preparação da versão beta

The [v0.2.0-beta.1 release workflow](https://github.com/leonardo-nicolas/Lagosystem-Bridge/actions/workflows/release-beta1.yml) is available **only via manual workflow_dispatch**. It builds experimental native packages for Windows x64, Ubuntu 24.04 x64 and macOS ARM64, plus unsigned developer ZIPs for Chromium and Firefox. It publishes a prerelease **only when all checks and five SHA-256-paired packages are present**. It does not start on commits.

O [workflow de publicação da beta](https://github.com/leonardo-nicolas/Lagosystem-Bridge/actions/workflows/release-beta1.yml) é **exclusivamente manual**. Ele prepara binários não assinados, pacotes de extensão para desenvolvimento e checksums SHA-256, mantendo a Release em rascunho se algum job falhar.

[Release notes EN/PT-BR](docs/releases/v0.2.0-beta.1.md) · [Release workflow documentation](docs/CI.md)

## Quick links

- [English: prerequisites, CMake commands, extension installation, JS example](docs/README.en.md)
- [Português BR: requisitos, CMake, extensões, integração JavaScript](docs/README.pt-BR.md)
- [Protocol reference](protocol/README.md)
- [Security policy](SECURITY.md) · [Contributing](CONTRIBUTING.md) · [Authors](AUTHORS.md)
- [Original architecture](docs/architecture.md) · [Hardware-validation checklist](docs/hardware-validation.md)

All software is distributed under **GNU GPL-3.0-or-later** (see [LICENSE](LICENSE)). Copyright attribution is described in [AUTHORS.md](AUTHORS.md).

### Git history migration

This repository contains a reconstructed **path-filtered 82-commit history** extracted from the Bridge files in the former private Lagosystem monorepo. Original commit messages are preserved, and each rebuilt commit has `Migrated-from` and `Original-author` trailers for traceability. Commit SHAs/dates are not identical because the paths and parent history changed; **no private ERP source or its unrelated commit graph was published**.

No CI, build, automated or physical hardware tests were executed as part of the extraction.