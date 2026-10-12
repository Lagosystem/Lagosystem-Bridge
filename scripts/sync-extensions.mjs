// SPDX-License-Identifier: GPL-3.0-or-later
// Synchronizes shared source into self-contained browser-extension directories.
// This is not a compiler or a test runner; call it manually before packaging.
import { copyFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
for (const family of ['chromium','firefox']) {
  for (const file of ['background.js','content.js']) {
    await copyFile(resolve(root,'extensions','shared',file),
      resolve(root,'extensions',family,file));
  }
}
process.stdout.write('Synchronized shared extension scripts. Review the diff before committing.\n');
