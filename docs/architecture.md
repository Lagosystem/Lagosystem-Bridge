# Lagosystem.Bridge: extensões WebExtensions e host C++20

## Arquitetura

```text
Qualquer SPA/site HTTPS autorizado no popup da extensão
   ↓ window.postMessage (requestId, canal fixo)
content.js da extensão (same-origin e detecção de extensão)
   ↓ runtime.sendMessage (lista fechada de ações)
background.js (autorização de site e de dispositivo por origem)
   ↓ browser.runtime.sendNativeMessage("com.lagosystem.bridge", JSON)
Native Messaging (manifest por navegador e ID exato da extensão)
   ↓ stdin/stdout: uint32 little-endian + JSON UTF-8
Lagosystem.Bridge (apenas marca, API agnóstica, sem root/UAC elevado)
   ↓ adaptadores C++: arquivos, serial, impressoras, balanças
   ⇢ SOMENTE capacidades locais explicitamente pareadas/habilitadas
```

O browser lança o **host nativo por usuário**, sem serviços elevados ou porta HTTP local. A comunicação usa protocolo Native Messaging de 4 bytes de comprimento (inteiro sem sinal, ordem nativa usada por plataformas Windows/Linux convencionais) e payload JSON UTF-8. Os limites da API do Chrome são diferentes por direção; a ponte impõe teto de 1 MiB para mensagens recebidas e enviadas. Debug somente em `stderr`; `stdout` exclusivo para mensagens.

## Implementação inicial

- C++20 com dependência `nlohmann_json >= 3.11` via CMake.
- Windows: recusa token elevado/UAC administrador; Linux: recusa UID 0/root. Não faz chamadas shell, PowerShell, `sudo` ou `cmd`.
- Ações atuais: `ping`, `capabilities`, `exports.save`, `devices.list`, `devices.paired`, `devices.pair`, `devices.unpair`, `devices.probe` e `devices.testPrint`.
- `exports.save` aceita somente `filename` simples e bytes base64 de `.xml` e `.pdf` com assinatura básica, até 750 KiB, criando arquivo **novo** em `~/Lagosystem/Exports` ou `%USERPROFILE%\\Lagosystem\\Exports`. Não sobrescreve arquivos, impede caminho com `..`, separadores ou symlink das pastas administradas. Arquivos fora dessa pasta não são acessados.
- Transporte serial em Windows COM, Linux ttyUSB/ttyACM e macOS cu.*. A extensão só utiliza portas pareadas com confirmação local e autorizadas por origem. VID/PID, drivers proprietários e escopos detalhados ainda são pendentes.
- Impressora térmica (ESC/POS, TSPL, ZPL ou proprietário), impressora matricial (ESC/P e variantes) e balança (Toledo, Filizola, Urano e outros protocolos) **não são automaticamente compatíveis** só porque têm serial. Cada driver precisa interpretar bytes, framing, soma de verificação, confirmação, estabilidade da pesagem e medidas de erro. Não usar protocolos de PDV como padrão indiscriminado para impressão de NF-e.
- As configurações do SO que exigirem elevação serão feitas pelo instalador/administrador em **etapa separada**, nunca pelo host da extensão. Não permitir elevação silenciosa para mudar impressoras, permissões de porta, UDEV ou drivers.

## Distribuição e instalação

### Extensão

As pastas `extensions/lagosystem-bridge/chromium/` e `extensions/lagosystem-bridge/firefox/` possuem manifestos MV3 próprios e scripts incluídos em cada pacote.

- Chrome, Edge, Brave e outros Chromium usam `background.service_worker`; o manifesto de host utiliza `allowed_origins` com o ID real da extensão publicada **sem wildcard**.
- Firefox usa `background.scripts` no MV3 e a identidade `bridge@lagosystem.com.br`; seu manifesto de host usa `allowed_extensions` com a ID exata.
- Antes de publicar, validar os IDs definitivos da Chrome Web Store/Edge e Firefox AMO, declarar origens do portal Lagosystem **reais** (os domínios no repositório são modelos), configurar política corporativa e passar por revisão de permissões.
- Opera GX e navegadores derivados dependem do suporte efetivo a Native Messaging e instalação local de manifestos. Não prometer compatibilidade universal sem homologação individual.
- Devs terceiros podem usar a extensão GPL em sites HTTPS próprios ou criar extensões alternativas; cada ID de extensão deve ser permitido explicitamente no manifesto nativo, nunca wildcard.

### Host

No Windows, instalar o executável no perfil do usuário e registrar caminhos JSON completos por navegador em `HKCU\\Software\\Google\\Chrome\\NativeMessagingHosts\\com.lagosystem.bridge` e `HKCU\\Software\\Mozilla\\NativeMessagingHosts\\com.lagosystem.bridge`. Para Edge e outros navegadores, verificar a chave específica por fornecedor, **não** assumir todas as chaves como iguais. O instalador nunca precisa rodar o host como administrador.

No Linux, salvar manifestos por usuário nos diretórios NativeMessagingHosts definidos pelo browser (Chrome/Chromium em `~/.config/.../NativeMessagingHosts/`, Firefox conforme distribuição). Firefox Snap/Flatpak pode precisar de portal/proxy Native Messaging específico da distribuição. Rodar sem root e dar acesso a portas seriais somente pelo grupo/política do usuário, com mínimos privilégios. Nunca adicionar Linux `root` ao grupo de execução ou conceder permissões globais de escrita em `/dev`.

Os arquivos de `native/Lagosystem.Bridge/manifests/` são **templates**; `REPLACE_WITH_ABSOLUTE_PATH_TO_BINARY` e `REPLACE_WITH_PUBLISHED_EXTENSION_ID` não devem ser enviados a cliente sem substituição. Um instalador assinado deve fazer registro por usuário e permitir remoção. Não incluí um instalador executável nesta fase.

### Compilação (referência, não executado nesta entrega)

```text
cmake -S native/Lagosystem.Bridge -B build/Lagosystem.Bridge
cmake --build build/Lagosystem.Bridge --config Release
```

Instalar `nlohmann_json` via gerenciador de pacotes/CMake antes de compilar. A árvore de produção não deve depender de download automático de bibliotecas em tempo de execução.

## Segurança antes de expor periféricos

1. O pareamento local de portas seriais com confirmação do usuário está implementado, com tipo, perfil e baud; pode ser revogado. Faltam VID/PID, ID físico estável, expiração, firmware e escopos refinados.
2. Pareamento, revogação e impressão de teste pedem confirmação nativa TaskDialog/GTK3/NSAlert; a leitura passiva é permitida apenas após pareamento. Não permitir executar comandos arbitrários, acessar arquivos arbitrários ou alterar o sistema operacional.
3. Não aceitar caminhos absolutos ou arbitrários do navegador. Para salvar XML/PDF em outros locais, usar **file picker nativo** da sessão interativa, retornar identificador opaco temporário e permitir somente a ação aprovada.
4. Se houver operação em daemon/serviço Windows/Linux, separar um **broker de máquina** de um host por usuário com autenticação IPC e direitos por usuário; não compartilhar o serviço como “root de todos” nem aceitar comandos de extensões na sessão de serviço.
5. Dispositivos via serial transportam **bytes**, não necessariamente texto. Os adapters devem suportar respostas binárias, paridade, RTS/CTS ou XON/XOFF, CRC e framing específicos do fabricante. Uma balança industrial deve fornecer estabilidade/tara/calibração e a impressão matricial precisa de perfil físico, margens e codificação antes de gravar documentos legais.
6. XML fiscal pode conter informações sensíveis. O serviço deve impedir leitura silenciosa, aplicar validação do formato antes da exportação, limitar tamanho e registrar consentimento sem salvar dados fiscais completos em logs de diagnóstico. O cabeçalho XML/PDF inicial é apenas uma validação superficial.
7. Registrar auditoria local com IDs correlacionáveis, sem senhas bancárias, tokens, conteúdo integral das notas ou dispositivos sem autorização. Assinar releases, conferir hash, atualizar por canal confiável e preservar a capacidade de desinstalação.

## Pode ser open source?

**Sim, como opção de produto, mas não há obrigação de publicar nada agora.** Recomendo separar:

- **Protocolo + SDKs + núcleo do transporte** potencialmente Apache-2.0 ou MIT, para permitir extensões de terceiros;
- **Drivers específicos e módulos de segurança/pareamento** publicados ou privados segundo viabilidade técnica e licenças;
- **ERP, rede comercial e segredos de integração** seguem privados, salvo decisão futura.

Foi escolhida GPLv3 para **as duas extensões e o host C++**, em subdiretórios separados, sem alterar a licença do ERP. Publicar código não concede acesso ao computador: cada instalação exige IDs e autorizações locais explícitos.

## Fontes técnicas verificadas

- [Chrome Native Messaging](https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging)
- [Mozilla Native Messaging](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging)
- [MDN MV3 background para Chrome e Firefox](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background)
- [Firefox confinado por Snap/Flatpak](https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/native-messaging-portal-design.html)

**Status:** base de código submetida, sem compilação, assinatura, empacotamento, instalação em cliente, validação de driver ou teste físico. Os drivers e permissões de hardware exigem implementação adicional.


## Atualização de implementação: GPL e agnosticidade

A marca Lagosystem permanece apenas nos nomes de distribuição. Extensões funcionam em sites HTTPS aprovados pelo usuário via popup; as permissões de dispositivos são específicas para cada origem.

As caixas de confirmação são nativas: TaskDialogIndirect no Windows 10/11, GTK3 no Linux (falha fechada sem GUI) e NSAlert/AppKit no macOS. No Mac a descoberta suporta /dev/cu.* se o driver fornecer uma porta serial.

O host dispõe de leitura passiva de balanças ASCII, peso candidato não certificado, inicialização limitada de impressoras ESC/POS ou ESC/P e texto de teste; não há drivers de marca, HID/USB proprietário, impressão fiscal homologada ou alterações privilegiadas do SO.

Ver docs/LOCAL-BRIDGE-VALIDATION.md para testes físicos e extensions/lagosystem-bridge/README.md para instalação GPL por navegador. Nenhum ensaio físico, build ou CI foi executado.
