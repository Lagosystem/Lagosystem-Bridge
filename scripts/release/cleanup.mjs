// SPDX-License-Identifier: GPL-3.0-or-later
// Always remove ephemeral CMake builds and package staging after each job.
import {rmSync} from 'node:fs';
import {dirname,resolve,relative,isAbsolute} from 'node:path';
import {fileURLToPath} from 'node:url';
import '../ci/cleanup.mjs';

const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
for(const local of ['.release-build','.release-staging','.release-dist']) {
  const target=resolve(root,local);
  const rel=relative(root,target);
  if(!rel||rel==='..'||rel.startsWith('../')||rel.startsWith('..\\')||isAbsolute(rel))
    throw new Error('Unsafe release cleanup path: '+local);
  rmSync(target,{recursive:true,force:true,maxRetries:3,retryDelay:200});
  console.log('Deleted temporary release data: '+local);
}
