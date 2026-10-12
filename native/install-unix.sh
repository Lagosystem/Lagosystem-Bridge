#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Current user only; neither sudo nor system-wide permissions.
set -euo pipefail
if [ "$(id -u)" -eq 0 ]; then echo "Do not run as root." >&2; exit 3; fi
if [ "$#" -lt 1 ]; then echo "Usage: install-unix.sh /absolute/path/host [chrome-extension-id]" >&2; exit 2; fi
exe="$1"
if [ ! -f "$exe" ] || [ ! -x "$exe" ]; then echo "Host executable absent/not executable." >&2; exit 2; fi
case "$exe" in /*) ;; *) echo "Host must be an absolute path." >&2; exit 2 ;; esac
# This installer uses printf JSON; reject dangerous path characters rather than auto-escaping.
if [[ "$exe" == *\"* || "$exe" == *\\* || "$exe" == *$'\n'* || "$exe" == *$'\r'* ]]; then
  echo "Unsafe executable path for JSON manifest." >&2; exit 2
fi
umask 077
name="com.lagosystem.bridge.json"
os="$(uname -s)"
if [ "$os" = "Darwin" ]; then
  firefox="$HOME/Library/Application Support/Mozilla/NativeMessagingHosts"
  chrome="$HOME/Library/Application Support/Google/Chrome/NativeMessagingHosts"
  chromium="$HOME/Library/Application Support/Chromium/NativeMessagingHosts"
else
  firefox="$HOME/.mozilla/native-messaging-hosts"
  chrome="$HOME/.config/google-chrome/NativeMessagingHosts"
  chromium="$HOME/.config/chromium/NativeMessagingHosts"
fi
mkdir -p "$firefox"
printf '{"name":"com.lagosystem.bridge","description":"Local Device Bridge GPL","path":"%s","type":"stdio","allowed_extensions":["bridge@lagosystem.com.br"]}\n' "$exe" > "$firefox/$name"
echo "Registered Firefox for current user."
if [ "$#" -ge 2 ]; then
  id="$2"
  if [[ ! "$id" =~ ^[a-p]{32}$ ]]; then echo "Invalid Chromium extension ID." >&2; exit 2; fi
  mkdir -p "$chrome" "$chromium"
  for directory in "$chrome" "$chromium"; do
    printf '{"name":"com.lagosystem.bridge","description":"Local Device Bridge GPL","path":"%s","type":"stdio","allowed_origins":["chrome-extension://%s/"]}\n' "$exe" "$id" > "$directory/$name"
  done
  echo "Registered Chrome and Chromium for current user."
fi
echo "No elevation or system-wide files were used."
