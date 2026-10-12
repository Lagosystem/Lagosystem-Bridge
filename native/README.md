# Local Device Bridge · C++20 · GPLv3

A marca de distribuição é Lagosystem; **a API e o host não consultam nenhum servidor, conta, tenant ou serviço Lagosystem**. Qualquer aplicativo Web HTTPS pode solicitar acesso após autorização independente na extensão e confirmação física local de cada porta. Outros desenvolvedores podem publicar extensões compatíveis, desde que o identificador esteja autorizado explicitamente no manifesto do host, sem wildcard.

## Capacidades em desenvolvimento

- `ping` / `capabilities`: descobrir presença, plataforma (Windows, Linux, macOS), protocolos genéricos e funcionalidades desabilitadas.
- `devices.list`: enumerar portas COM no Windows, `/dev/ttyUSBn` e `/dev/ttyACMn` no Linux, `/dev/cu.*` no macOS.
- `devices.pair`: após confirmação local, salvar autorização por usuário e porta para perfis `scale/ascii-stream`, `thermal/escpos`, `dotmatrix/escp`. A extensão implementa autorização adicional por origem Web.
- `devices.paired` / `devices.unpair`: consultar/revogar autorizações.
- `devices.probe`: abrir porta serial de balança e ler até 256 bytes passivamente; converter um possível peso ASCII com unidade, **sem certificar calibração, estabilidade ou validade fiscal**.
- `devices.testPrint`: pedir nova confirmação nativa e enviar texto de teste curto com inicialização ESC @ para impressoras serial térmicas ou matriciais compatíveis. Não envia comandos de corte, gaveta, NF-e ou ajuste de impressora do SO.
- `exports.save`: XML/PDF no perfil do usuário, sem acesso arbitrário a outras pastas ou execução de shell.

**Software host e extensões:** GPL-3.0, licença nos respectivos subdiretórios. ERP e SaaS não são publicados nem relicenciados por esta inclusão.

### Interfaces nativas

- **Windows 10/11:** Win32 COM + `TaskDialogIndirect` para caixa de confirmação com visual moderno do Windows; registro do manifesto por usuário.
- **Linux:** POSIX termios + GTK3 caso disponível na compilação e sessão gráfica. Sem GTK/tela interativa o consentimento é recusado por segurança. Não precisa root.
- **macOS:** POSIX termios em `/dev/cu.*` + `NSAlert` no AppKit. Uma interface USB sem driver de serial no macOS **não aparecerá** como porta COM; suporte a HID/USB proprietário requer adaptador específico, entitlements e certificação.

### Configuração CMake (não executada)

```sh
cmake -S native/Lagosystem.Bridge -B build/native-bridge -DLAGOSYSTEM_BRIDGE_BUILD_TESTS=ON
cmake --build build/native-bridge --config Release
ctest --test-dir build/native-bridge --output-on-failure
```

Dependência: `nlohmann_json >= 3.11` no CMake. Se GTK3 não estiver disponível no Linux, o host pode compilar, mas não consegue autorizar novos dispositivos via interface gráfica. Não abrir permissão para todos os usuários como substituto.

**Testes automáticos/fixtures não foram executados**: CI/build exigem autorização específica. A execução do testador físico requer dispositivo e usuário presente na máquina, vide `docs/LOCAL-BRIDGE-VALIDATION.md`.

## Instalação Native Messaging sem privilégios de administrador

Após compilar e assinar o executável, o usuário deve instalar sua própria cópia do host. Os scripts seguintes somente criam manifestos por usuário; nenhum teste de dispositivo ou CI é iniciado por eles.

**Windows (PowerShell 7, terminal NÃO elevado):**

    .\\native\\Lagosystem.Bridge\\install-windows.ps1 -Executable 'C:\\Users\\Public\\bin\\lagosystem-bridge.exe' -ChromeExtensionId '<ID_PUBL_CHROME>' -EdgeExtensionId '<ID_PUBL_EDGE>'

**Linux/macOS (Bash sem sudo/root):**

    bash native/Lagosystem.Bridge/install-unix.sh /caminho/absoluto/lagosystem-bridge <ID_PUBL_CHROME>

O ID Chromium deve ser realmente atribuído à extensão instalada (32 letras a–p); o Firefox usa o ID fixo declarado no manifesto. Os exemplos acima contêm placeholders: substituí-los por paths e IDs reais. No macOS usar o caminho real do binário compilado para sua arquitetura; distribuir uma aplicação apropriadamente assinada/notarizada pode ser necessário para evitar bloqueios do Gatekeeper.
