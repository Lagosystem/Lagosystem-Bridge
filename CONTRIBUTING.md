# Contributing / Contribuindo

We welcome improvements to the GPLv3-or-later host, WebExtensions, protocol specifications, translations and hardware adapters. / Aceitamos contribuições ao host GPLv3, extensões, contratos, traduções e adaptadores de hardware.

## Issue Forms / Formulários de Issues

Use [New issue / Nova Issue](https://github.com/leonardo-nicolas/Lagosystem-Bridge/issues/new/choose) to open one of four bilingual structured forms (EN/PT-BR):

- **Bug Report** — reproducible errors, environment, steps and sanitized logs. / Falhas reproduzíveis, ambiente, etapas e logs sem segredos.
- **Feature Request** — problem, proposed change, component impact and security. / Problema, proposta, componentes e segurança.
- **Hardware Compatibility** — vendor, model, connection, serial parameters and availability for physical validation. / Fabricante, modelo, conexão, parâmetros seriais e acesso ao hardware.
- **Protocol / Driver Proposal** — framing, samples, legal access to documentation, permissions and validation plan. / Enquadramento, amostras, direitos de documentação, permissões e homologação.

General questions belong in [Discussions](https://github.com/leonardo-nicolas/Lagosystem-Bridge/discussions). Report security vulnerabilities **privately** through the instructions in [SECURITY.md](SECURITY.md). / Dúvidas gerais vão para o Discussions e vulnerabilidades devem ser comunicadas **privadamente**.

## Contribution guidelines / Diretrizes de contribuição

1. Open an issue describing the feature or hardware **manufacturer, model, firmware, serial settings, framing and expected response**. Avoid sharing proprietary manuals without authorization. / Abra uma issue com fabricante, modelo, firmware e especificações do protocolo, sem publicar documentação proprietária indevidamente.
2. Keep the native host independent of the Lagosystem SaaS, and maintain the native-message action allowlist, per-origin scope and explicit OS consent. / Preserve a independência do ERP e as autorizações por origem, dispositivo e sistema operacional.
3. Edit shared browser JavaScript in `extensions/shared/` and synchronize browser copies using `node scripts/sync-extensions.mjs` before opening a PR. Preserve browser-specific manifests. / Edite os scripts compartilhados e sincronize as cópias específicas de cada navegador.
4. If writing a new device driver, include synthetic fixtures and a manual real-device validation checklist. **Never claim a successful physical test when only serial bytes were written.** / Documente testes sintéticos e físicos separadamente.
5. Use descriptive Conventional Commits and update **both** `docs/README.en.md` and `docs/README.pt-BR.md` whenever a public API changes. / Documente alterações públicas nos dois idiomas.
6. Include SPDX headers and follow GPL-3.0-or-later compatibility obligations when adding dependencies. / Respeite atribuição autoral e compatibilidade das licenças.

**CI and builds are opt-in:** do not add automatic GitHub Actions triggers or execute workflows, builds or tests on behalf of the owner without that execution's express authorization. / **CI e compilação mediante autorização específica**, inclusive validação somente de build.

Keep vendor-specific protocols in isolated drivers; do not weaken the generic command allowlist to accommodate one manufacturer's hardware.