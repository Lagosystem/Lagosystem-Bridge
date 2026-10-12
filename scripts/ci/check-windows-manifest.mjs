// SPDX-License-Identifier: GPL-3.0-or-later
// Inspect the PE embedded manifest using mt.exe from the installed Windows SDK.
// Fails before IPC tests when the executable would load the legacy Comctl32 v5.
import assert from 'node:assert/strict';
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { spawnSync } from 'node:child_process';

assert.equal(process.platform, 'win32', 'Windows manifest check must run on Windows');
const buildDir = process.env.BRIDGE_CI_BUILD_DIR ?? '.ci-build/native';
const configuration = process.env.BRIDGE_CI_CONFIGURATION ?? 'Debug';
const binary = resolve(buildDir, configuration, 'lagosystem-bridge.exe');
assert.ok(existsSync(binary), 'Compiled Windows host executable is absent: ' + binary);

const sdkBase = join(process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)',
  'Windows Kits', '10', 'bin');
assert.ok(existsSync(sdkBase), 'Windows SDK bin directory not found: ' + sdkBase);
const versions = readdirSync(sdkBase, { withFileTypes: true })
  .filter(item => item.isDirectory() && /^\d+\.\d+\.\d+\.\d+$/.test(item.name))
  .map(item => item.name)
  .sort((a, b) => {
    const aa = a.split('.').map(Number), bb = b.split('.').map(Number);
    for (let i=0;i<aa.length;i++) if (aa[i] !== bb[i]) return bb[i]-aa[i];
    return 0;
  });
const mt = versions
  .map(version => join(sdkBase, version, 'x64', 'mt.exe'))
  .find(path => existsSync(path));
assert.ok(mt, 'Windows SDK Manifest Tool (x64/mt.exe) not found');

const directory = mkdtempSync(join(tmpdir(), 'bridge-manifest-'));
try {
  const output = join(directory, 'embedded.xml');
  const result = spawnSync(mt, [
    '-nologo', '-inputresource:' + binary + ';#1', '-out:' + output
  ], { encoding:'utf8', timeout: 10000 });
  if (result.error) throw result.error;
  assert.equal(result.status, 0,
    'Failed to extract RT_MANIFEST ID 1: ' + (result.stderr || result.stdout));
  const manifest = readFileSync(output, 'utf8');
  assert.match(manifest, /Microsoft\.Windows\.Common-Controls/);
  assert.match(manifest, /version=["']6\.0\.0\.0["']/);
  assert.match(manifest, /requestedExecutionLevel[^>]*level=["']asInvoker["']/);
  console.log('Embedded PE manifest: Common Controls v6 + asInvoker verified');
} finally {
  rmSync(directory, { recursive:true, force:true });
}
