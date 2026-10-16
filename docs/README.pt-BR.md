# Guia do desenvolvedor (Português brasileiro)

O Lagosystem Bridge é uma ponte **experimental e open-source GPL-3.0-or-later** para comunicar aplicações Web autorizadas com periféricos locais. Apesar do nome, **o host C++ e as extensões não dependem de login, API, tenant ou infraestrutura Lagosystem**. Uma software house pode utilizá-lo em seu próprio sistema.

> Estado atual: o código-fonte e as fixtures sintéticas foram migrados; compilação multiplataforma, extensões assinadas e testes físicos com equipamentos reais **ainda não foram homologados**. Leituras e resultados de impressão são diagnósticos, não certificados de calibração nem provas de impressão.

## Organização do monorepo

- `native/`: host C++20, CMake, projeto MSVC, comunicação serial, diálogos nativos e scripts de instalação.
- `extensions/chromium/`: extensão Manifest V3 para navegadores Chromium-based.
- `extensions/firefox/`: extensão Manifest V3 para Firefox, com identificador próprio.
- `protocol/`: contrato público, ações e exemplos da comunicação `local-device-bridge-v1`.
- `docs/architecture.md` e `docs/hardware-validation.md`: arquitetura e roteiro de homologação.
- `docs/README.en.md`: guia equivalente em inglês.

As duas extensões podem ser carregadas diretamente a partir de suas pastas. Há código comum espelhado; os manifestos de Chromium e Firefox **não devem ser trocados**.

## Requisitos

- CMake 3.24 ou superior, compilador compatível com C++20.
- **Windows 10/11**: MSVC v143 (Visual Studio ou Build Tools) e Windows SDK.
- **Linux**: GCC ou Clang, interface serial POSIX e, para confirmação gráfica, GTK3. Sem interface GTK3 compilada ou sessão gráfica, a autorização local é recusada.
- **macOS**: Apple Clang/Xcode, Objective-C++ e AppKit; é preciso driver adequado para periféricos USB-serial.
- Conexão na configuração inicial do CMake: `FetchContent` obtém `nlohmann_json` 3.11.3 com verificação SHA-256.

### Instruções de compilação (não executadas nesta migração)

Windows x64 com MSVC:

~~~powershell
cmake -S native -B build/bridge-win-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/bridge-win-x64 --config Release
~~~

Linux:

~~~sh
cmake -S native -B build/bridge-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/bridge-linux
~~~

macOS:

~~~sh
cmake -S native -B build/bridge-macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/bridge-macos
~~~

O CMake é a **configuração oficial de compilação**; o arquivo `native/Lagosystem.Bridge.vcxproj` foi preservado para integração com IDE, mas não substitui as dependências resolvidas pelo CMake. No Linux, `-static-libstdc++` não torna o binário universal: ainda pode depender de glibc, GTK e bibliotecas do sistema.

Para habilitar apenas fixtures sintéticas, acrescente `-DLAGOSYSTEM_BRIDGE_BUILD_TESTS=ON` ao CMake; execute `ctest --test-dir <diretório-build> --output-on-failure` somente quando quiser realizar esse teste. Não equivale a testar equipamentos reais.

## Instalação e registro por usuário

1. Em Chromium, carregue `extensions/chromium/` como extensão descompactada; no Firefox, use o modo de depuração com `extensions/firefox/`. Para publicar nas lojas, siga as respectivas políticas de assinatura.
2. Obtenha o **ID real** da extensão Chromium (32 letras de a a p). No Firefox, o manifesto usa `bridge@lagosystem.com.br`.
3. Compile o host e instale sua cópia em um local acessível ao usuário. Execute `native/install-windows.ps1` em PowerShell **não elevado**, ou `native/install-unix.sh` no Linux/macOS **sem sudo/root**, informando o caminho absoluto do executável e o ID Chromium quando aplicável.
4. Os scripts registram o host `com.lagosystem.bridge` no navegador por usuário. Não compilam o executável, não instalam drivers e não autorizam dispositivos automaticamente.
5. Abra o seu sistema por HTTPS, clique no **popup da extensão** e autorize explicitamente o domínio. HTTP é permitido somente para localhost/127.0.0.1 de desenvolvimento.
6. Seu frontend pode consultar `ping` e `capabilities`. Para parear um periférico é necessária outra confirmação, exibida nativamente no Windows (TaskDialog), Linux (GTK3) ou macOS (NSAlert).

O diretório de Native Messaging varia conforme navegador e sistema operacional. Não presuma que Edge, Brave, Opera GX, Chrome, Firefox e demais derivados utilizem os mesmos caminhos. No macOS também podem existir exigências de assinatura e notarização.

## Consumindo em uma aplicação que não seja Lagosystem

O protocolo utiliza `window.postMessage` entre o site e o content script da extensão. O background confirma a origem real, aplica a lista de ações permitidas e acessa o host Native Messaging. Uma aplicação Web **não** recebe acesso direto às APIs privilegiadas do navegador.

~~~js
const channel = "local-device-bridge-v1";
function localBridge(action, parameters = {}, timeoutMs = 10000) {
  return new Promise((resolve) => {
    const requestId = crypto.randomUUID();
    const timer = setTimeout(() => finish({ok: false, error: "timeout"}), timeoutMs);
    function receive(event) {
      if (event.source !== window || event.origin !== location.origin) return;
      if (event.data?.channel !== channel ||
          event.data?.direction !== "response" ||
          event.data?.requestId !== requestId) return;
      finish(event.data.payload);
    }
    function finish(result) {
      clearTimeout(timer);
      window.removeEventListener("message", receive);
      resolve(result);
    }
    window.addEventListener("message", receive);
    window.postMessage({
      channel, direction: "request", requestId, action, ...parameters
    }, location.origin);
  });
}
const extension = await localBridge("detect"); // extensão instalada?
const host = await localBridge("ping");         // host instalado e origem autorizada?
const devices = await localBridge("devices.list");
~~~

Se `detect` expirar, a extensão pode estar ausente ou desabilitada. Se `ping` retornar `origin_not_approved`, o próprio usuário deverá autorizar o domínio **no popup da extensão**, nunca por um comando da página. Se o host estiver indisponível, confirme seu executável e o manifesto registrado para aquele navegador.

Exemplo de pareamento de balança serial descoberta: `devices.pair` com `{deviceId:"COM3",kind:"scale",profile:"ascii-stream",baudRate:9600}`. O usuário deverá autorizar na janela nativa do SO. `devices.probe` retorna amostra serial passiva e, quando possível, `candidateWeight`/`unit`; a estabilidade e calibração **não** são verificadas. Para impressoras seriais genéricas, utilize `thermal/escpos` ou `dotmatrix/escp`; `devices.testPrint` pede confirmação adicional, e apenas indica envio dos bytes de teste.

Também existem `devices.paired`, `devices.unpair`, `capabilities` e `exports.save` (gera novos XML/PDF na pasta `Lagosystem/Exports` do usuário por padrão; com `selectDirectory:true`, abre o seletor nativo para escolher uma pasta em qualquer unidade acessível, sem aceitar caminho fornecido pelo site). Consulte o [contrato do protocolo](../protocol/README.md).

## Segurança e participação

Não execute o host como administrador/root. Não retire as autorizações por origem e periférico. O host não oferece shell, servidor HTTP, acesso a qualquer arquivo ou alteração privilegiada do SO.

**A leitura ASCII genérica de balança não é metrologia legal e não deve ser usada isoladamente em operações comerciais, fiscais ou industriais.** Novas marcas/modelos precisam de documentação de protocolo e testes físicos documentados. Veja [homologação](hardware-validation.md), [segurança](../SECURITY.md) e [contribuições](../CONTRIBUTING.md).

Licença: GNU GPL-3.0-or-later, texto em [LICENSE](../LICENSE).
### Exportar em outra pasta, pendrive, HD externo ou nuvem montada

```js
const result = await localBridge("exports.save", {
  filename: "nota.pdf",
  base64: pdfBase64,
  selectDirectory: true
});
```

O usuário seleciona **pessoalmente** a pasta a cada exportação na janela do Windows/Linux/macOS; a página não informa o caminho. O host exige leitura do diretório, permissão de criação e acesso de leitura/escrita ao novo arquivo; recusa pastas reconhecidamente privadas do sistema/aplicações, links inseguros e arquivos existentes. O resultado retorna somente `filename` e `location:"selected_directory"`, nunca o caminho absoluto. Sem desktop/GTK3 (Linux), a seleção falha de modo seguro. A operação não concede acesso para listar, ler, apagar ou sobrescrever outros arquivos. Montagens de Google Drive/OneDrive/NAS são tratadas como pastas comuns quando o sistema operacional realmente permite as operações; permissões do provedor podem acrescentar restrições.


## Novo instalador automatizado por usuário

Para Linux/macOS: execute `bash installers/install.sh --url https://seu-sistema.exemplo:8443/app --chromium-ids ID_DA_EXTENSAO` (IDs opcionais; use os IDs reais das extensões instaladas). No macOS também existe `install-macos.sh`. Para Windows, compile o script `installers/Lagosystem-Bridge.iss` no Inno Setup 6; o assistente chama `installers/install-windows.ps1` sem solicitar privilégios administrativos. Todas as plataformas verificam SHA-256 do download do último GitHub Release público (inclusive pré-lançamentos). Execute o desinstalador equivalente para remover o host e os registros por usuário.

**Importante:** a URL fica salva localmente e aparece no popup das novas extensões; ela não é aprovada automaticamente. Instalar/assinar extensões Chromium e Firefox continua dependendo do navegador ou da loja. O processo Native Messaging é ativado sob demanda, não no login. Consulte [Guia dos instaladores](installers.md) para requisitos e limitações.
