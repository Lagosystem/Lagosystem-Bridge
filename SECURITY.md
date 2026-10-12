# Security policy / Política de segurança

**EN:** This is experimental native-device software. Report vulnerabilities privately through **GitHub → Security → Report a vulnerability**, when enabled, or contact the maintainers through a non-public channel before sharing exploit details. Please include affected version/OS/browser, steps to reproduce, attack scenario and non-sensitive logs. Never disclose customer credentials, certificate private keys or fiscal data in a public issue.

**PT-BR:** Este software de comunicação com periféricos ainda é experimental. Relate vulnerabilidades de forma privada pelo **GitHub → Security → Report a vulnerability**, se habilitado, ou por canal reservado com os mantenedores. Informe versão/SO/navegador e passos de reprodução sem divulgar segredos, chaves fiscais ou dados de clientes em issues públicas.

## Non-negotiable guarantees / Requisitos obrigatórios

- No elevated administrator/root host process or fallback to \`sudo\`/UAC.
- Do not add arbitrary shell execution, unrestricted filesystem access, global network services, wildcard extension IDs or browser-origin wildcard authorization.
- Origin approval must be performed inside the extension UI, not inferred from incoming web-page messages.
- Device permission must be scoped to an explicitly approved site and a locally confirmed device; revocation must work.
- Filesystem exports must remain scoped to the user's application folder, without untrusted path traversal.
- Physical operations with side effects (printing, device configuration) require appropriate user consent.
- Scale readings are not legally certified; vendor-specific protocols require independent verification.
- Avoid printing fiscal, personal or financial content as a diagnostic sample.

No audited release or guaranteed support window is asserted. Third-party browser IDs, native host registration, signed installers and vendor drivers are deployment concerns, not credentials to commit.