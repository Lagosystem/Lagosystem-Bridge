#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
if [ "$(uname -s)" != Darwin ]; then echo "Use uninstall.sh on Linux." >&2; exit 2; fi
exec bash "$(cd -- "$(dirname -- "$0")" && pwd)/uninstall.sh" "$@"
