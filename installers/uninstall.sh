#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Current-user removal only; pairings and exported customer files are preserved.
set -euo pipefail
if [ "$(id -u)" -eq 0 ]; then echo "Do not run as root." >&2; exit 3; fi
case "$(uname -s)" in
 Linux)
   data="${XDG_DATA_HOME:-$HOME/.local/share}/lagosystem-bridge"
   config="${XDG_CONFIG_HOME:-$HOME/.config}/lagosystem-bridge"
   firefox="$HOME/.mozilla/native-messaging-hosts"
   browsers=("$HOME/.config/google-chrome/NativeMessagingHosts" "$HOME/.config/chromium/NativeMessagingHosts" "$HOME/.config/microsoft-edge/NativeMessagingHosts" "$HOME/.config/BraveSoftware/Brave-Browser/NativeMessagingHosts" "$HOME/.config/vivaldi/NativeMessagingHosts")
   rm -f -- "${XDG_DATA_HOME:-$HOME/.local/share}/applications/lagosystem-bridge.desktop"
   ;;
 Darwin)
   data="$HOME/Library/Application Support/Lagosystem Bridge"
   config="$data"
   firefox="$HOME/Library/Application Support/Mozilla/NativeMessagingHosts"
   browsers=("$HOME/Library/Application Support/Google/Chrome/NativeMessagingHosts" "$HOME/Library/Application Support/Chromium/NativeMessagingHosts" "$HOME/Library/Application Support/Microsoft Edge/NativeMessagingHosts" "$HOME/Library/Application Support/BraveSoftware/Brave-Browser/NativeMessagingHosts")
   rm -rf -- "$HOME/Applications/Lagosystem Bridge.app"
   ;;
 *) echo "Unsupported OS." >&2; exit 2 ;;
esac
name='com.lagosystem.bridge.json'
for directory in "$firefox" "${browsers[@]}"; do
  file="$directory/$name"
  # Only remove manifests belonging to this application; do not remove other native hosts.
  if [ -f "$file" ] && grep -q '"name":"com.lagosystem.bridge"' "$file"; then
    rm -f -- "$file"
  fi
done
rm -rf -- "$data"
if [ "$config" != "$data" ]; then rm -f -- "$config/site-url"; rmdir -- "$config" 2>/dev/null || true; fi
echo "Lagosystem Bridge uninstalled for this user. Hardware pairings and exports were preserved."
