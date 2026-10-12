# Manual CI/CD · Zero persisted build cache / CI/CD manual · Sem cache persistente

**English / Português brasileiro**

[Run Bridge CI / Executar Bridge CI](https://github.com/leonardo-nicolas/Lagosystem-Bridge/actions/workflows/bridge-ci.yml)

## English

The workflow at [bridge-ci.yml](../.github/workflows/bridge-ci.yml) is **CI only, not automatic deployment**. It can run only via `workflow_dispatch` on the GitHub Actions page. Pushes, pull requests, tags and schedules **never start it**. The repo owner's explicit approval is required for every new run.

After approval: GitHub → **Actions** → **Bridge CI (manual, no persistent caches)** → **Run workflow** → branch **main** → **Run workflow**. Wait for all three jobs and inspect any failing job logs. Do **not** mark a platform supported merely because it compiled.

The workflow uses **only standard GitHub-hosted runners**: `ubuntu-24.04`, `windows-2022` (MSVC) and `macos-15` (Apple Clang), with up to two running at a time and a 25-minute timeout per job. GitHub lists standard runners for public repositories as free of billed Actions minutes; **this does not mean infinite concurrency or unlimited overall GitHub resources**. Larger runners may incur cost and are intentionally not used.

Each job performs, without real hardware:

1. Fetch the public repository with read-only checkout permissions.
2. Set up Node.js 22 with **no npm/pnpm install and no Actions dependency cache**.
3. Use CMake Debug configuration and download SHA-256-pinned `nlohmann_json` to the **temporary runner workspace**.
4. Compile the C++20 host and synthetic scale-protocol fixtures. Run CTest while assertions are active (Debug).
5. Validate Chromium and Firefox Manifest V3 contents, minimal permissions, host manifest identities and matching shared JavaScript.
6. Simulate both browser background/content adapters with Node's built-in `vm` and send real length-prefixed JSON messages to the **compiled native C++ host**. Check ping, capabilities, unapproved-origin rejection, prohibited commands, non-existent device denial and Native Messaging framing. **This does not launch actual Firefox/Chrome, open devices, print or run calibration tests**.
7. Execute `scripts/ci/cleanup.mjs` with `if: always()`, deleting `.ci-build`, `build`, `native/build` and CMake-generated `CMakeCache.txt`, `CMakeFiles`, `_deps`, `Testing`, and generated scripts even when a previous step failed.

### Quota/cost protection

- No `actions/cache`, no `actions/upload-artifact`, no release/deployment, no Docker images, no package registry publication.
- No persistent CMake cache: dependencies are downloaded again at the next run; this trades a small amount of runner time and network use for **zero retained cache storage**.
- `permissions: contents: read` prevents jobs from writing repository content with their built-in token.
- Workspace and transient runner files are also discarded by GitHub when hosted runners shut down. Workflow logs and usage metadata can remain visible in the GitHub UI under GitHub's retention policies; the workflow creates no persisted build artifacts or Actions caches.
- Billing/free-runner rules can change; verify [GitHub Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions) before enabling additional services.
- No tests in this workflow can certify real printers, scales or vendor-specific protocols.

No new job should be executed without fresh permission. The presence of a YAML file, or a previous run, does not authorize another run.

## Português brasileiro

O arquivo [bridge-ci.yml](../.github/workflows/bridge-ci.yml) configura **integração contínua manual, sem implantação automática**. Ele usa exclusivamente `workflow_dispatch`. Commits, PRs, tags e agendamentos **não iniciam o CI**. Cada execução exige autorização expressa do proprietário.

Para iniciar após autorizar: GitHub → **Actions** → **Bridge CI (manual, no persistent caches)** → **Run workflow** → branch **main** → **Run workflow**.

A matriz utiliza apenas runners padrão do GitHub para **Ubuntu 24.04, Windows Server 2022 com MSVC e macOS 15 com Apple Clang**, no máximo duas tarefas em paralelo e limite de 25 minutos por tarefa. Nos repositórios públicos, o GitHub oferece runners padrão sem cobrança de minutos; isso não libera runners maiores nem elimina os demais limites do serviço.

Cada plataforma compila o host C++20 via CMake em Debug, executa as fixtures sintéticas do CTest, confere os manifestos e permissões das extensões Chromium/Firefox e simula a comunicação das duas extensões com o **executável C++ compilado**, usando JSON Native Messaging e o ambiente Node.js. Verifica autorização por origem, lista de comandos, framing, resposta de `ping` e negativa para uma porta inexistente.

**Não é um teste em Chrome ou Firefox reais nem em periféricos físicos**. O CI não envia impressão, não efetua pareamento de portas existentes, não emite documentos fiscais e não pode homologar protocolos de fabricante.

### Limpeza e consumo

O workflow **não usa** `actions/cache`, `upload-artifact`, publicação de releases, Docker ou pacotes npm. As dependências obtidas por CMake FetchContent ficam apenas no workspace temporário. A etapa final (`if: always()`) remove mesmo em caso de falha toda a árvore `.ci-build`, as pastas `build` e os diretórios/arquivos de cache CMake, incluindo `CMakeCache.txt`, `CMakeFiles` e `_deps`.

Consequentemente, o fluxo foi projetado para **não acumular cache ou artefatos cobrados entre execuções**, e utilizar somente minutos gratuitos dos runners padrão para repositórios públicos. Logs e metadados de execução podem permanecer no GitHub conforme as políticas do serviço.

Para verificar o consumo e as regras vigentes, consulte [GitHub Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions). Nenhum workflow deve ser iniciado ou repetido sem a autorização específica do proprietário.


## Windows TaskDialog and elevated runner / Windows TaskDialog e runner elevado

The Windows executable depends on `TaskDialogIndirect` from Common Controls v6.
The Windows CI checks the embedded PE manifest (`RT_MANIFEST` resource ID 1)
with the Windows SDK `mt.exe` before executing the native host. Both the CMake
build and the standalone MSVC `.vcxproj` must include
`native/resources/windows-app.manifest`, not merely the VERSIONINFO `.rc` file.

GitHub-hosted Windows runners use elevated administrator contexts. The production
host intentionally rejects elevated execution with exit code **3**. The Windows
test therefore verifies that explicit security refusal; it never bypasses it.
Linux and macOS validate the full Native Messaging IPC integration with the C++
binary and simulated Chromium/Firefox adapters. The Windows C++ build, synthetic
C++ fixtures, browser manifests and shared scripts still run normally.

**PT-BR:** O `TaskDialogIndirect` exige o Common Controls v6 incorporado no
manifesto do EXE; o CI confere esse recurso com `mt.exe` antes do teste IPC.
Os runners Windows hospedados usam contexto administrativo e o Bridge deve
recusar esse privilégio (código **3**). Portanto, o job Windows valida a
recusa segura e executa compilação, fixtures e verificação das extensões,
enquanto Ubuntu/macOS fazem a integração IPC real entre extensões simuladas
e o executável. **Não há exceção de segurança ou desativação de validação
de elevação em builds de produção.**

## Manual beta publishing / Publicação manual da beta

**Not the normal CI:** [Publish v0.2.0-beta.1 (manual)](https://github.com/leonardo-nicolas/Lagosystem-Bridge/actions/workflows/release-beta1.yml) is a separate `workflow_dispatch` action. It will **publish a public GitHub pre-release** if all jobs succeed.

On branch `main`, after **specific approval to perform the release build and publication**, open the workflow and click **Run workflow**. The first job creates an *unpublished draft* linked to the current commit SHA. Next, Windows x64, Ubuntu x64 and macOS ARM64 compile C++ Release binaries, run CTest fixtures and Native Messaging checks, then upload packages and SHA-256 files **directly to the draft Release**. Another job validates and uploads the unsigned Chromium/Firefox development ZIPs. The final job checks all ten named files and publishes `v0.2.0-beta.1` with pre-release status. A failure leaves an unpublished draft for investigation; a subsequent run is intentionally blocked until that draft/tag is resolved manually.

**PT-BR:** Este workflow não é o CI normal. Ao executar **Run workflow**, você autoriza compilar os três sistemas e **publicar publicamente a beta** somente se todos os jobs forem aprovados. O primeiro job prepara uma Release em rascunho vinculada ao SHA da execução; os demais compilam, validam, produzem os cinco pacotes e seus cinco checksums e enviam diretamente ao GitHub Releases. A publicação só acontece depois da conferência dos dez arquivos. Se algum job falhar, o rascunho não é publicado e a próxima tentativa exigirá resolver manualmente esse rascunho/tag.

**Quota and cleanup:** only standard public GitHub-hosted runners. No `actions/cache`, `upload-artifact`, external registries, privileged physical devices or scheduled triggers. The release packages are deliberately retained as **GitHub Release assets**, not ephemeral Actions artifacts. The `scripts/release/cleanup.mjs` step removes `.release-build`, `.release-staging`, `.release-dist` and ordinary CMake caches with `if: always()`. Github may retain logs, release assets and operational metadata under its normal service rules. This cannot guarantee literally zero storage anywhere on GitHub.

**Limitações:** arquivos de extensão não assinados; Windows sem Authenticode; macOS sem notarização; nenhum teste físico de balança/impressora; no CI dos runners Windows o host permanece protegido contra privilégios elevados. Consulte as [notas da beta](releases/v0.2.0-beta.1.md). 
