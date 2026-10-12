# Local Device Bridge · WebExtensions para Chromium e Firefox

## Independência do ecossistema Lagosystem

Este projeto usa "Lagosystem" apenas como marca de distribuição. **Nenhum login, API, token de sessão, tenant, usuário ou endpoint do SaaS Lagosystem é necessário** para sua instalação ou funcionamento. Sites HTTPS de outros desenvolvedores podem usar a extensão depois de o usuário **autorizar aquele domínio no popup** e confirmar localmente o pareamento do periférico.

- Código-fonte das duas variantes: GNU GPL v3, licença integral em `chromium/LICENSE` e `firefox/LICENSE`.
- Host nativo também: GPL v3, em `native/Lagosystem.Bridge/LICENSE`.
- O ERP SaaS, a API ASP.NET Core e os módulos empresariais **não** são publicados nem automaticamente relicenciados, pois são componentes distribuídos separadamente com comunicação protocolar. A separação técnica não determina, sozinha, a análise jurídica de obra derivada/combinação GPL; revisar licenças de terceiros antes de lançar builds públicas.
- O nome `com.lagosystem.bridge` identifica o host perante o navegador, **não é um endereço de rede** e não implica dependência da marca.

## Proteções

1. Content script usa `window.postMessage` para comunicar o site com a extensão, inclusive no Firefox, que não oferece a API externa de páginas Web equivalente à do Chrome. Esse canal não dá privilégios por si só.
2. O popup só permite adicionar/remover a origem HTTPS (ou localhost HTTP de desenvolvimento). Nenhuma página pode adicionar sua própria origem aprovada por `postMessage`.
3. O background consulta a origem real em `sender.url` e confere uma lista fechada de ações, sem executar comandos arbitrários.
4. Mesmo entre sites já autorizados, o background mantém **pares de dispositivos separados por origem**. Um segundo site precisa pedir novo pareamento e confirmação nativa.
5. O host pode se comunicar com **extensões de outros autores** se o instalador do host configurar **IDs individuais** autorizados em `allowed_origins` (Chromium) ou `allowed_extensions` (Firefox). Não permitir wildcard no host.
6. O host C++ funciona sem root/UAC administrador, pede confirmação nativa Windows/GTK/macOS, não disponibiliza operações de execução remota, de sistema ou de leitura de arquivos arbitrários.

## Instalação do código-fonte

- Pastas `chromium/` e `firefox/` são pacotes distintos, cada um com `manifest.json`, `background.js`, `content.js`, `popup.html`, `popup.js` e `LICENSE`.
- Para testar a extensão no Chrome/Edge, carregar a pasta `chromium/` como extensão descompactada. No Firefox, usar ferramenta de depuração de extensões e pasta `firefox/`; para distribuição pública, validar empacotamento/assinatura e política AMO.
- Para usar o host, criar arquivo Native Messaging próprio **por navegador**, substituindo os marcadores de caminho e ID em `native/Lagosystem.Bridge/manifests/`. Sem isso, a extensão é detectada mas `ping` informa host ausente.
- Para macOS, a instalação do manifest nativo e os executáveis assinados/notarizados podem variar segundo o navegador e política Gatekeeper. Drivers seriais só aparecem se o sistema tiver suporte a eles.
- Opera GX, Brave e navegadores derivados devem ser testados individualmente; não presumir que leiam a mesma pasta de manifestos do Chrome.

## API JavaScript para qualquer aplicação Web

```js
const channel = "local-device-bridge-v1";
function requestLocalDevice(action, parameters = {}) {
  return new Promise(resolve => {
    const requestId = crypto.randomUUID();
    const receive = event => {
      if (event.source !== window || event.origin !== location.origin) return;
      if (event.data?.channel !== channel || event.data?.direction !== "response" ||
          event.data?.requestId !== requestId) return;
      window.removeEventListener("message", receive);
      resolve(event.data.payload);
    };
    window.addEventListener("message", receive);
    window.postMessage({channel, direction:"request", requestId, action, ...parameters}, location.origin);
  });
}

const ext = await requestLocalDevice("detect");        // extensão instalada?
const host = await requestLocalDevice("ping");         // host registrado e disponível?
const ports = await requestLocalDevice("devices.list");
const result = await requestLocalDevice("devices.pair", {
  deviceId: "COM3", kind: "scale", profile: "ascii-stream", baudRate: 9600
});
// O usuário também verá a autorização local do SO.
```

O código de exemplo pressupõe que o **site já foi autorizado no popup**, que o host esteja instalado e que uma porta COM exista na máquina. Não garante nem realiza pesagem real ou testes físicos por si só.

Documentação mais detalhada: `docs/LOCAL-BRIDGE.md` e `docs/LOCAL-BRIDGE-VALIDATION.md`.
