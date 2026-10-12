# Local Device Bridge protocol (v1)

**Channel in the page:** \`local-device-bridge-v1\`. Requests from the site have \`{channel,direction:"request",requestId,action,...args}\` and responses from the content script are \`{channel,direction:"response",requestId,payload:{ok,...}}\`.

**Native Messaging host ID:** \`com.lagosystem.bridge\`. This is a browser registration identifier, **not a DNS name or cloud service**. Browser background sends a whitelisted JSON object directly to the native host (without the page requestId). Native framing is 4-byte little-endian unsigned size followed by UTF-8 JSON. This native host currently caps input and output messages at about 1 MiB.

### Actions

| Action | Arguments | Behavior |
| --- | --- | --- |
| \`detect\` | — | Extension content script only; checks extension installation. Does not establish host availability. |
| \`ping\` | — | Checks native host registration/availability and reports \`protocolVersion:1\` and OS platform. |
| \`capabilities\` | — | Reports available and unavailable operations and protocol profiles. |
| \`devices.list\` | — | Lists enumerated serial interfaces, **not** confirmed printer/scale models. |
| \`devices.paired\` | — | Lists current OS user grants filtered to this website's own device permissions. |
| \`devices.pair\` | \`deviceId,kind,profile,baudRate\` | OS-native confirmation; browser extension also grants a device per website. |
| \`devices.unpair\` | \`deviceId\` | OS-native confirmation and revocation. |
| \`devices.probe\` | \`deviceId\` | Passive ASCII read for a previously paired serial scale; weight is a candidate, not certified. |
| \`devices.testPrint\` | \`deviceId\` | Additional confirmation then sends a short generic serial ESC/POS or ESC/P test. |
| \`exports.save\` | \`filename,base64\` | Writes a new PDF/XML file only under the current user's \`Lagosystem/Exports\` folder. |

Supported pair combinations: \`scale / ascii-stream\`, \`thermal / escpos\`, \`dotmatrix / escp\`. Accepted baud rates: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200. Actual device communication currently assumes serial transport. Unsupported operations must fail closed.

For an integration example and error states, consult [English developer guide](../docs/README.en.md) or [guia em português](../docs/README.pt-BR.md). The JSON request schema below is descriptive and does not replace native/extension runtime checks.

### Versioning

The browser-facing \`local-device-bridge-v1\` channel is distinct from the software version (currently 0.2.0 in source manifests). Clients must not infer that future product versions imply an unchanged protocol.