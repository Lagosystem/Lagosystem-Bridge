# Plano de homologação da ponte local e de periféricos físicos

Esta documentação distingue **testes de software**, **testes com portas simuladas** e **testes físicos com equipamento conectado**. Até esta revisão, **nenhum teste executável ou ensaio físico foi efetivamente realizado**.

## Critérios de aceitação por plataforma

| Cenário | Windows 10/11 | Linux desktop | macOS |
| --- | --- | --- | --- |
| Extensão detectada / host ausente | Guia desabilitada e explicação | Mesmo comportamento | Mesmo comportamento |
| Extensão + host | GUI habilita Configurações → Periféricos | Idem | Idem |
| Origem não aprovada | Solicita usar popup da extensão | Idem | Idem |
| Discovery serial | COM1–COM128 via QueryDosDevice | ttyUSB/ttyACM | /dev/cu.* |
| Pareamento de porta | TaskDialog Win32 com Sim/Não, sem UAC | GTK3 se disponível; sem tela, falha fechada | NSAlert AppKit, sem root |
| Revogação | Requer autorização local | Idem | Idem |
| Leitura passiva | 8N1 no perfil ASCII | POSIX termios | termios e driver USB-serial presente |
| Impressão de teste | Serial com texto ESC @ + CRLF | Idem | Idem, se existir porta serial |
| Configurações do SO privilegiadas | Nunca executadas pela ponte | Nunca executadas pela ponte | Nunca executadas pela ponte |

Navegadores: Chrome, Edge, Brave, Firefox, Opera GX e versões derivados requerem **instalação própria** do manifesto Native Messaging e ID real da extensão. Compatibilidade só é considerada homologada após execução naquele navegador/sistema. Firefox não oferece comunicação direta página→extensão pelo mesmo mecanismo do Chromium; a implementação utiliza um **content script** comum e `postMessage`, com origem autorizada pela extensão.

## Testes de software reproduzíveis sem fabricante

1. **Fixtures ASCII:** amostra `ST,GS,+0012.345kg`, amostra `+250,50g`, e sequência binária que não deve produzir peso candidato. `tests/scale_protocol_fixtures.cpp` contém asserts e é ativado opcionalmente com `-DLAGOSYSTEM_BRIDGE_BUILD_TESTS=ON`.
2. **Native Messaging:** validar enquadramento little-endian de 32 bits em Windows/macOS/Linux usuais, limite de 1 MiB, JSON inválido, comandos desconhecidos, mensagens truncadas e ausência de stdout de debug.
3. **Política de segurança:** usuário root/admin deve ser recusado; diretórios proibidos e `..` não são aceitos; não permitir arquivos fora do perfil; não habilitar `execute`, `shell`, `os.configure`, `openAnyFile`; pares por origem devem permanecer separados; revogação deve bloquear novas operações.
4. **Simulação de serial:** no Linux/macOS, utilizar PTY e testes de `termios` somente em ambiente isolado com device virtual explicitamente allowlisted. A implementação atual enumera **apenas dispositivos /dev reais específicos**, por isso **não** permite PTY arbitrário a pedido da página. Testes PTY deverão utilizar harness de injeção no driver de testes, nunca enfraquecer o host de produção.
5. **Teste de API Web:** em um site HTTPS autorizado manualmente pela extensão, confirmar `detect`, `ping`, `devices.list`, estados `native_host_unavailable`, `origin_not_approved`, teste recusado pelo usuário e expiração/revogação. Não autorizar origem `*://*/*` no backend nem IDs de extensão genéricos nos manifestos.

## Protocolo por família

### Balanças

- **ASCII passivo** (`scale/ascii-stream`): ler amostra serial após configurar velocidade, 8N1 e timeout; exibir bytes recebidos e possível peso/unidade. **Não classificar a medição como estável, calibrada ou legalmente apta**.
- Protocolos proprietários Toledo, Filizola, Urano, Micheletti, Mettler Toledo, indicadores rodoviários e industriais: exigem documentação do modelo, parâmetros reais (baud, paridade, stop bits), framing, polling/ENQ, CRC e sinal explícito de estabilidade. **Não foram implementados**, para evitar assumir que todos respondem da mesma forma.
- Teste físico obrigatório antes do uso fiscal/logístico: peso nulo, tara, carga conhecida, peso instável, sobrepeso, desconexão e reconexão, medidas diferentes e leitura de bytes inválidos. Registrar modelo, firmware, data, técnico e assinatura de aferição quando aplicável.

### Impressora térmica serial

- **ESC/POS genérico:** `ESC @` e texto curto ASCII/CRLF, sem papel fiscal, corte automático, gaveta ou buzzer. Exige confirmação TaskDialog/GTK/NSAlert mesmo após parear. Só marca envio de bytes, **não prova impressão nem compatibilidade do modelo**.
- Impressoras por USB/HID/driver spooler, rede TCP/IP, ZPL/TSPL, Epson específicos: não são testados pela porta serial genérica; criar drivers e permissões próprios, com simulação e depois hardware real.
- Teste físico: papel, idioma/codificação, largura, margens, cabeçalho, resultado impresso, buffer e erro de comunicação.

### Impressora matricial serial

- **ESC/P genérico:** `ESC @` e texto ASCII/CRLF. Não imprimir documentos fiscais reais nem texto longo até validar a impressora específica.
- Teste físico: continuidade do formulário, margem, página, posição da fita/cabeça, codificação, caracteres especiais, velocidade, fluxo RTS/CTS e confirmação visual.

## Roteiro para o operador na guia

1. Instalar a extensão adequada ao navegador e abrir o **popup** da extensão.
2. Autorizar explicitamente o domínio da aplicação que solicitou a ponte (não é necessário login na Lagosystem).
3. Instalar o host no perfil do usuário e registrar manifesto com ID exato da extensão; reiniciar navegador conforme necessidade.
4. Abrir Configurações → Periféricos e clicar **Verificar novamente** até que extensão e host respondam `ping`.
5. Escolher porta, tipo, perfil genérico e velocidade. Clicar **Parear**; responder à caixa de confirmação nativa.
6. Em balança, clicar **Ler amostra**, observar peso candidato e dados crus e verificar estabilidade manualmente.
7. Em impressora, colocar papel/cuidar do ambiente e clicar **Testar impressão**, confirmar novamente no SO e conferir fisicamente o papel.
8. Clicar **Revogar** e verificar que futuras solicitações são recusadas; desautorizar o domínio no popup quando não precisar mais.

## Critérios para registro de homologação

Guardar `OS, versão, arquitetura, navegador, extensão, SHA do host, fabricante, modelo, firmware, identificação física, protocolo, velocidade, bit/paridade, resultado real, responsável, data`. Nunca registrar dados completos de NF-e, CPF/CNPJ, chaves/certificados fiscais, conteúdo de documento ou credenciais bancárias nos logs do teste.

**A ausência de hardware conectado neste ambiente impede afirmar aprovação de impressoras e balanças reais.** As fixtures constituem testes de software e permanecem não executadas até autorização de build/CI.

## Registro do host no computador de teste

Existem scripts de registro por usuário:

- Windows PowerShell: native/Lagosystem.Bridge/install-windows.ps1, recebendo path absoluto do .exe e IDs da extensão Chrome/Edge/Brave pertinentes; Firefox é registrado com ID fixo.
- Linux/macOS Bash: native/Lagosystem.Bridge/install-unix.sh, recebendo path absoluto do host executável e ID Chromium opcional; registra Firefox no perfil do usuário e, quando informado, Chrome/Chromium.

Nenhum script compila, testa, chama periféricos nem modifica configurações globais. A extensão só indicará host pronto se ele já estiver compilado, registrado e disponível. Os scripts foram revisados estaticamente, não executados.
