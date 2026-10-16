// SPDX-License-Identifier: GPL-3.0-or-later
// Static validation: no npm dependencies or browser/device access.
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { Script } from 'node:vm';

const read = path => readFileSync(resolve(path), 'utf8');
const permissions = ['nativeMessaging','storage','activeTab'];
for (const family of ['chromium','firefox']) {
  const base = 'extensions/' + family + '/';
  const manifest = JSON.parse(read(base + 'manifest.json'));
  assert.equal(manifest.manifest_version, 3);
  assert.equal(manifest.version, '0.2.0');
  assert.deepEqual([...manifest.permissions].sort(), [...permissions].sort(),
    'Unexpected browser permission in ' + family);
  assert.ok(manifest.content_scripts.some(item =>
    item.js.includes('content.js') &&
    item.matches.includes('https://*/*') &&
    item.matches.includes('http://localhost/*') &&
    item.matches.includes('http://127.0.0.1/*')));
  if (family === 'chromium') {
    assert.equal(manifest.background.service_worker, 'background.js');
    assert.equal(manifest.browser_specific_settings, undefined);
  } else {
    assert.ok(manifest.background.scripts.includes('background.js'));
    assert.equal(manifest.browser_specific_settings.gecko.id, 'bridge@lagosystem.com.br');
  }
  for (const script of ['background.js','content.js','popup.js']) {
    const path = base + script;
    assert.ok(existsSync(path), 'Missing ' + path);
    new Script(read(path), { filename:path });
  }
  assert.ok(existsSync(base + 'popup.html'));
  assert.ok(read(base + 'background.js').includes('siteOrigin: url.origin'),
    family + ' must derive native origin from sender.url');
  assert.ok(!read(base + 'background.js').includes('message.siteOrigin'),
    family + ' must ignore siteOrigin supplied by webpages');
  assert.ok(existsSync(base + 'icons/icon.png'), 'Missing extension icon for ' + family);
  assert.equal(manifest.icons['128'], 'icons/icon.png');
  assert.equal(manifest.action.default_icon['16'], 'icons/icon.png');
  for (const common of ['background.js','content.js']) {
    assert.equal(read(base + common), read('extensions/shared/' + common),
      family + ' must use synchronized shared/' + common);
  }
  console.log(family + ': Manifest V3, permissions, JS syntax and shared parity verified');
}
for (const family of ['chromium','firefox']) {
  const host = JSON.parse(read('native/manifests/' + family + '.json'));
  assert.equal(host.name,'com.lagosystem.bridge');
  assert.equal(host.type,'stdio');
  if (family==='firefox')
    assert.deepEqual(host.allowed_extensions,['bridge@lagosystem.com.br']);
  else
    assert.equal(host.allowed_origins.length,1);
}
console.log('Native Messaging template manifests: valid');
