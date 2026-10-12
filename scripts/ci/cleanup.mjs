// SPDX-License-Identifier: GPL-3.0-or-later
// GitHub-hosted runner is ephemeral. Proactively erase all generated CMake state.
import { rmSync } from 'node:fs';
import { dirname, resolve, relative, isAbsolute } from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const paths = [
  '.ci-build', 'build', 'native/build',
  'native/CMakeCache.txt', 'native/CMakeFiles', 'native/_deps',
  'native/cmake_install.cmake', 'native/CTestTestfile.cmake', 'native/Testing',
  'CMakeCache.txt', 'CMakeFiles', 'cmake_install.cmake', 'Testing'
];
for (const path of paths) {
  const target = resolve(repo, path);
  const rel = relative(repo, target);
  if (!rel || rel === '..' || rel.startsWith('../') || rel.startsWith('..\\') || isAbsolute(rel)) {
    throw new Error('Unsafe cleanup path: ' + path);
  }
  rmSync(target, { recursive: true, force: true, maxRetries: 3, retryDelay: 200 });
  console.log('Cleaned CMake temp: ' + path);
}
