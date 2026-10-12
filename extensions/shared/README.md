# Shared WebExtension source

The browser runtime JavaScript is currently **identical** between Chromium and Firefox, while their \`manifest.json\` entries differ. Edit \`background.js\` and \`content.js\` in this directory first, then run \`node scripts/sync-extensions.mjs\` to refresh the two **self-contained, unpacked** browser directories.

Do not publish \`extensions/shared/\` as an extension package. Publish the \`chromium/\` or \`firefox/\` folder appropriate to the store. This sync script does not compile, test or sign the extension.

Fonte canônica dos scripts compartilhados: edite aqui e sincronize as pastas \`chromium/\` e \`firefox/\`; preserve os manifestos específicos de cada família.