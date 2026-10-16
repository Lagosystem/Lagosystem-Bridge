# Lagosystem Bridge — instalação e remoção / Install and uninstall

A integração funciona sem conta Lagosystem, domínio obrigatório ou backend próprio. Use uma URL HTTPS do seu produto, **incluindo portas ou caminhos personalizados**. Somente `http://localhost` e `http://127.0.0.1` podem usar HTTP durante desenvolvimento.

## Instalação Linux

```bash
bash installers/install.sh
# Ou não interativo, com IDs reais (32 letras entre a-p):
bash installers/install.sh --url "https://erp.exemplo.com:8443/app" --chromium-ids "abcdefghijklmnopabcdefghijklmnop" --yes
```

O script baixa o pacote para a arquitetura local do **último Release público**, inclusive pré-release; baixa o arquivo `.sha256` correspondente e confere SHA-256 antes da extração. Instala o host em `~/.local/share/lagosystem-bridge/bin/` (ou `$XDG_DATA_HOME`), grava URL em `~/.config/lagosystem-bridge/site-url` (ou `$XDG_CONFIG_HOME`), cria manifests Native Messaging por usuário para Firefox e para Chromium quando há IDs, e registra um atalho conforme a especificação XDG em `~/.local/share/applications/`. O atalho aparece em menus compatíveis GNOME, KDE, XFCE, LXDE, etc., e agora **abre a interface nativa Bridge Manager**, onde o site sugerido fica desativado até confirmação local.

Remova com `bash installers/uninstall.sh`. Nenhum dos scripts usa sudo/root. Não remove as exportações nem dados de periféricos. Verifique a presença de GTK3 e permissões USB/serial individualmente.

## macOS

```bash
bash installers/install-macos.sh --url "https://erp.exemplo.com/app" --chromium-ids "abcdefghijklmnopabcdefghijklmnop"
bash installers/uninstall-macos.sh
```

A instalação usa `~/Library/Application Support/Lagosystem Bridge` e manifests nas pastas específicas do navegador. Cria o aplicativo-atalho **Lagosystem Bridge.app** no `~/Applications`, que abre o Bridge Manager. O host C++ continua sendo um processo Native Messaging sob demanda, não um `LaunchAgent` e não precisa de login autostart.

O fluxo atual é shell + atalho `.app`, não um instalador DMG assinado. Para distribuição a consumidores macOS, preparar `pkg` ou `dmg` **assinado/notarizado** é a evolução indicada. Binários não assinados podem ser bloqueados pelo Gatekeeper. A Release beta original tem somente macOS ARM64, não Intel.

## Windows

Abra `installers/Lagosystem-Bridge.iss` no **Inno Setup 6** e compile o instalador usando `ISCC.exe`. Execute-o como **usuário padrão**; o assistente pede a URL e os IDs reais da extensão Chromium. A instalação baixa a Release apropriada (Windows x64/ARM64, se publicada), valida SHA-256, copia `lagosystem-bridge.exe` para `%LOCALAPPDATA%\\Lagosystem.Bridge\\bin`, registra os manifests em `HKCU` e adiciona atalho de website ao Menu Iniciar. A desinstalação do Inno chama `install-windows.ps1 -Uninstall` sem exigir administrador.

Para executar o bootstrapper separadamente:
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\installers\\install-windows.ps1 -SiteUrl "https://erp.exemplo.com:8443/app" -ChromiumExtensionIds "abcdefghijklmnopabcdefghijklmnop" -NonInteractive
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\installers\\install-windows.ps1 -Uninstall
```

O arquivo `.ico` já está referenciado no `native/resources/windows.rc`; a inclusão no PE ocorrerá **na próxima compilação Windows**. Este patch não compila nem publica executáveis.

## Fluxo de autorização e extensões

1. Instale a extensão para o seu navegador via loja ou ferramenta de desenvolvimento (o instalador não ignora assinaturas nem políticas de lojas).
2. Descubra o **ID Chromium real** em `chrome://extensions` / `edge://extensions`, ative o modo desenvolvedor quando necessário e use esse ID ao instalar ou reconfigurar o host. O Firefox usa `bridge@lagosystem.com.br`.
3. No popup de uma extensão atualizada, use **Abrir site configurado**. Depois clique em **Autorizar este site**. O instalador **nunca** preenche `approvedOrigins` sem consentimento explícito.
4. O navegador cria o processo do host quando há chamada Native Messaging. Cada periférico exige confirmação local separada.

**Não usar autostart de sistema:** o host atual é um processo de protocolo `stdin/stdout`, não um serviço residente. Configurar systemd user, LaunchAgent ou Startup do Windows para iniciá-lo sem canal de mensagens não mantém uma ponte funcional. Se no futuro houver processo residente/tray, ele precisará ser desenvolvido e auditado separadamente.

## Segurança, integridade e limitações

- Os arquivos `.sha256` ficam junto dos artefatos no GitHub Release. A comparação detecta arquivo modificado ou corrompido, mas **não substitui** uma assinatura do publicador: um atacante que comprometa o Release pode alterar ambos. Para hardening futuro: assinatura de checksum com chave pública pinada, Sigstore ou atestação de proveniência.
- Não há privilégio administrativo ou instalação global. Snap/Flatpak podem usar caminhos ou portais Native Messaging específicos e **não estão cobertos** por todos os diretórios tradicionais.
- IDs de extensões Chromium dependem da instalação/loja; não são adivinhados nem forçados pelo script. Sem IDs, o manifesto Firefox funciona, mas o Chromium precisará de nova configuração.
- A Release publicada atual pode ter sido criada **antes** das mudanças deste patch. O arquivo `installer` sempre obtém o último artefato **que de fato foi publicado**. Para distribuir as novidades em binário será preciso compilar e lançar uma **nova Release**, com autorização explícita para qualquer CI.
- O programa open-source é licenciado em `GPL-3.0-or-later`; respeite permissões por site e por periférico e as verificações anti-root.


## Nova política local e dependências do Manager

O arquivo SQLite de políticas por usuário é preservado em atualizações e desinstalação padrão. Novos Releases precisam conter **ambos os binários** `lagosystem-bridge` e `lagosystem-bridge-manager`. O instalador recusa Releases antigos que não contenham o Manager, incluindo o beta anterior. Para criar um novo Release, compilar com `-DLAGOSYSTEM_BRIDGE_BUILD_MANAGER=ON` e dependências SQLite3 + wxWidgets; libsqlite e wx podem requerer distribuição compatível com cada sistema. Cuidado com vínculos dinâmicos no Linux/macOS e com Windows vcpkg. Para macOS, assinatura/notarização continuam pendentes.

**Atenção:** até que seja produzido um novo Release compilado e validado, os bootstrappers não conseguirão instalar a nova arquitetura a partir da beta pública anterior. A verificação SHA-256 detecta corrupção, mas não fornece autenticidade criptográfica independente se o próprio Release for comprometido.
