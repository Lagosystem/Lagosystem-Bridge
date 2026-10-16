// SPDX-License-Identifier: GPL-3.0-or-later
// Packages experimental releases on ephemeral GitHub-hosted runners.
// No device access, signing, publishing or Actions artifact uploads happen here.
import assert from 'node:assert/strict';
import {copyFileSync,existsSync,mkdirSync,readFileSync,writeFileSync,chmodSync} from 'node:fs';
import {basename,dirname,join,resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';

const tag=process.env.RELEASE_TAG;
assert.match(tag??'',/^v[0-9]+\.[0-9]+\.[0-9]+(?:[-.][0-9A-Za-z.-]+)?$/,'RELEASE_TAG must be a validated semver tag');
const notes=join('docs','releases',tag+'.md');
assert.ok(existsSync(notes),'Add release notes before packaging: '+notes);
const kind=process.argv[2];
assert.ok(kind==='native'||kind==='extensions','Usage: package.mjs native|extensions');
const build=process.env.BRIDGE_CI_BUILD_DIR??'.release-build/native';
const staging=resolve('.release-staging');
const dist=resolve('.release-dist');
mkdirSync(staging,{recursive:true});
mkdirSync(dist,{recursive:true});

function copy(source,target) {
  assert.ok(existsSync(source),'Missing required release file: '+source);
  mkdirSync(dirname(target),{recursive:true});
  copyFileSync(source,target);
}
function pack(folder,name,isZip) {
  const output=join(dist,name);
  const args=['-E','tar',isZip?'cf':'czf',output];
  if(isZip) args.push('--format=zip');
  args.push('--',folder);
  const result=spawnSync('cmake',args,{cwd:staging,encoding:'utf8'});
  if(result.error) throw result.error;
  assert.equal(result.status,0,'Archive creation failed: '+result.stderr);
  const checksum=createHash('sha256').update(readFileSync(output)).digest('hex');
  writeFileSync(output+'.sha256',checksum+'  '+basename(output)+'\n','utf8');
  console.log(name+'  sha256='+checksum);
}
const notice=[
  'Lagosystem Bridge '+tag+' — EXPERIMENTAL',
  'License: GPL-3.0-or-later. Read LICENSE and RELEASE-NOTES.md.',
  'Not certified for real hardware, legal metrology or fiscal use.',
  'Unsigned Windows/macOS binaries; macOS is not notarized.',
  'No Lagosystem SaaS service is required.',
  'https://github.com/leonardo-nicolas/Lagosystem-Bridge',
  ''
].join('\n');

if(kind==='native') {
  const os=({win32:'windows',linux:'linux',darwin:'macos'})[process.platform];
  assert.ok(os,'Unsupported OS: '+process.platform);
  const arch=process.arch;
  assert.equal(arch,os==='macos'?'arm64':'x64','Unexpected runner architecture');
  const name='lagosystem-bridge-'+tag+'-'+os+'-'+arch;
  const root=join(staging,name);
  const executable=os==='windows'?'lagosystem-bridge.exe':'lagosystem-bridge';
  const binary=os==='windows'
    ?join(build,'Release',executable):join(build,executable);
  copy(binary,join(root,'bin',executable));
  copy('assets/f5987213-e8f8-4cd7-a10f-17bbebac061b.png',join(root,'assets','lagosystem-bridge.png'));
  copy('assets/lagosystem_native_ico.ico',join(root,'assets','lagosystem_native_ico.ico'));
  for(const installer of ['install.sh','uninstall.sh','install-macos.sh','uninstall-macos.sh','install-windows.ps1','Lagosystem-Bridge.iss'])
    copy(join('installers',installer),join(root,'installers',installer));
  if(os!=='windows') chmodSync(join(root,'bin',executable),0o755);
  copy('native/LICENSE',join(root,'LICENSE'));
  copy('native/README.md',join(root,'README.md'));
  copy(notes,join(root,'RELEASE-NOTES.md'));
  copy('native/install-windows.ps1',join(root,'install-windows.ps1'));
  copy('native/install-unix.sh',join(root,'install-unix.sh'));
  copy('native/manifests/chromium.json',join(root,'manifests','chromium.json'));
  copy('native/manifests/firefox.json',join(root,'manifests','firefox.json'));
  // nlohmann_json is MIT, ensure its license accompanies the statically bundled header.
  copy(join(build,'_deps','json-src','LICENSE.MIT'),
    join(root,'third-party','nlohmann-json-LICENSE.MIT'));
  writeFileSync(join(root,'NOTICE-BETA.txt'),notice,'utf8');
  pack(name,name+(os==='windows'?'.zip':'.tar.gz'),os==='windows');
} else {
  for(const family of ['chromium','firefox']) {
    const name='lagosystem-bridge-'+tag+'-'+family;
    const root=join(staging,name);
    for(const file of ['manifest.json','background.js','content.js','popup.html','popup.js','LICENSE','icons/icon.png'])
      copy(join('extensions',family,file),join(root,file));
    copy(notes,join(root,'RELEASE-NOTES.md'));
    writeFileSync(join(root,'NOTICE-BETA.txt'),
      notice+'Unsigned development WebExtension — not a browser-store release.\n','utf8');
    pack(name,name+'.zip',true);
  }
}
