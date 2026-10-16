#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Per-user native host installation. No sudo, system services, or device access.
set -euo pipefail
if [ "$(id -u)" -eq 0 ]; then echo "Do not run this installer as root." >&2; exit 3; fi

usage() { echo "Usage: $0 [--url https://example.com:8443/app] [--chromium-ids id1,id2] [--yes]"; }
site_url='' chromium_ids='' noninteractive=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --url) site_url="${2:?Missing URL}"; shift 2 ;;
    --chromium-ids) chromium_ids="${2:?Missing IDs}"; shift 2 ;;
    --yes) noninteractive=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
done

os="$(uname -s)"
case "$os" in Linux) platform=linux ;; Darwin) platform=macos ;; *) echo "Unsupported OS: $os" >&2; exit 2 ;; esac
case "$(uname -m)" in x86_64|amd64) arch=x64 ;; arm64|aarch64) arch=arm64 ;; *) echo "Unsupported CPU architecture." >&2; exit 2 ;; esac
for executable in curl tar; do command -v "$executable" >/dev/null || { echo "Missing: $executable" >&2; exit 2; }; done
if ! command -v sha256sum >/dev/null && ! command -v shasum >/dev/null; then echo "Missing sha256sum/shasum." >&2; exit 2; fi
if [ -z "$site_url" ] && [ "$noninteractive" = false ]; then
  read -r -p "Website URL (HTTPS, or localhost HTTP for development): " site_url
fi
# No arbitrary HTTP destinations or embedded credentials; preserve custom ports and paths.
if [[ "$site_url" =~ ^https://[A-Za-z0-9][A-Za-z0-9.-]*(:[0-9]{1,5})?(/[^[:space:]#@]*)?$ ]] ||
   [[ "$site_url" =~ ^http://(localhost|127\.0\.0\.1)(:[0-9]{1,5})?(/[^[:space:]#@]*)?$ ]]; then :; else
  echo "Invalid URL. HTTPS required except localhost / 127.0.0.1." >&2; exit 2
fi
authority="${site_url#*://}"; authority="${authority%%/*}"
if [[ "$authority" == *:* ]]; then
  port="${authority##*:}"
  if (( 10#$port < 1 || 10#$port > 65535 )); then echo "Port outside 1..65535." >&2; exit 2; fi
fi
if [ -z "$chromium_ids" ] && [ "$noninteractive" = false ]; then
  read -r -p "Chromium extension IDs (32 letters a-p, comma separated; blank for Firefox only): " chromium_ids
fi
origins=()
if [ -n "$chromium_ids" ]; then
  IFS=',' read -r -a identifiers <<< "$chromium_ids"
  for identifier in "${identifiers[@]}"; do
    if [[ ! "$identifier" =~ ^[a-p]{32}$ ]]; then echo "Invalid Chromium extension ID: $identifier" >&2; exit 2; fi
    origins+=("chrome-extension://$identifier/")
  done
fi

# GitHub's /releases/latest only includes stable, published releases.
repo='Lagosystem/Lagosystem-Bridge'
metadata="$(curl --proto '=https' --tlsv1.2 -fsSL --retry 3 "https://api.github.com/repos/$repo/releases/latest")"
tag="$(printf '%s\n' "$metadata" | sed -n 's/^[[:space:]]*"tag_name":[[:space:]]*"\([^"]*\)".*/\1/p' | head -n 1)"
if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]]; then
  echo "No eligible published stable release found." >&2; exit 4
fi
asset="lagosystem-bridge-$tag-$platform-$arch"
case "$platform" in linux) asset="$asset.tar.gz" ;; macos) asset="$asset.tar.gz" ;; esac
base="https://github.com/$repo/releases/download/$tag"
temporary="$(mktemp -d)"
trap 'rm -rf -- "$temporary"' EXIT
echo "Downloading $tag ($platform/$arch)..."
curl --proto '=https' --tlsv1.2 -fL --retry 3 "$base/$asset" -o "$temporary/$asset"
curl --proto '=https' --tlsv1.2 -fL --retry 3 "$base/$asset.sha256" -o "$temporary/$asset.sha256"
expected="$(awk -v filename="$asset" '$2 == filename && length($1) == 64 && $1 ~ /^[[:xdigit:]]+$/ { print tolower($1) }' "$temporary/$asset.sha256")"
if [ -z "$expected" ]; then echo "Missing or malformed SHA-256 release checksum." >&2; exit 4; fi
if command -v sha256sum >/dev/null; then actual="$(sha256sum "$temporary/$asset" | awk '{print $1}')"
else actual="$(shasum -a 256 "$temporary/$asset" | awk '{print $1}')"; fi
if [ "$expected" != "$actual" ]; then echo "SHA-256 mismatch; installation aborted." >&2; exit 4; fi

umask 077
if [ "$platform" = linux ]; then
  data="${XDG_DATA_HOME:-$HOME/.local/share}/lagosystem-bridge"
  config="${XDG_CONFIG_HOME:-$HOME/.config}/lagosystem-bridge"
  applications="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
  firefox="$HOME/.mozilla/native-messaging-hosts"
  browsers=("$HOME/.config/google-chrome/NativeMessagingHosts" "$HOME/.config/chromium/NativeMessagingHosts" "$HOME/.config/microsoft-edge/NativeMessagingHosts" "$HOME/.config/BraveSoftware/Brave-Browser/NativeMessagingHosts" "$HOME/.config/vivaldi/NativeMessagingHosts")
else
  data="$HOME/Library/Application Support/Lagosystem Bridge"
  config="$data"
  firefox="$HOME/Library/Application Support/Mozilla/NativeMessagingHosts"
  browsers=("$HOME/Library/Application Support/Google/Chrome/NativeMessagingHosts" "$HOME/Library/Application Support/Chromium/NativeMessagingHosts" "$HOME/Library/Application Support/Microsoft Edge/NativeMessagingHosts" "$HOME/Library/Application Support/BraveSoftware/Brave-Browser/NativeMessagingHosts")
fi
mkdir -p "$data/bin" "$config" "$firefox"
# Stream exactly one known tar member; never unpack untrusted paths into the home directory.
bundle="${asset%.tar.gz}"
target="$data/bin/lagosystem-bridge"
tar -xOzf "$temporary/$asset" "$bundle/bin/lagosystem-bridge" > "$temporary/lagosystem-bridge"
chmod 700 "$temporary/lagosystem-bridge"
mv -f "$temporary/lagosystem-bridge" "$target"
printf '%s\n' "$site_url" > "$config/site-url"
chmod 600 "$config/site-url"

# Absolute paths for Native Messaging manifests. Encode JSON strings instead of interpolating
# unescaped shell arguments into JSON.
json_string() {
  local input="$1"
  input="${input//\\/\\\\}"
  input="${input//\"/\\\"}"
  printf '"%s"' "$input"
}
manifest_name='com.lagosystem.bridge.json'
printf '{"name":"com.lagosystem.bridge","description":"Lagosystem Bridge","type":"stdio","path":%s,"allowed_extensions":["bridge@lagosystem.com.br"]}\n' "$(json_string "$target")" > "$firefox/$manifest_name"
if [ "${#origins[@]}" -gt 0 ]; then
  permitted=''
  for origin in "${origins[@]}"; do
    if [ -n "$permitted" ]; then permitted+=','; fi
    permitted+="$(json_string "$origin")"
  done
  for directory in "${browsers[@]}"; do
    mkdir -p "$directory"
    printf '{"name":"com.lagosystem.bridge","description":"Lagosystem Bridge","type":"stdio","path":%s,"allowed_origins":[%s]}\n' "$(json_string "$target")" "$permitted" > "$directory/$manifest_name"
  done
else
  echo "No Chromium IDs supplied: Firefox configured; Chromium registration skipped."
fi

launcher="$data/open-site.sh"
cat > "$launcher" <<'LAUNCH'
#!/usr/bin/env bash
set -euo pipefail
if [ "$(uname -s)" = Darwin ]; then
  file="$HOME/Library/Application Support/Lagosystem Bridge/site-url"
  IFS= read -r url < "$file"
  open "$url"
else
  file="${XDG_CONFIG_HOME:-$HOME/.config}/lagosystem-bridge/site-url"
  IFS= read -r url < "$file"
  xdg-open "$url"
fi
LAUNCH
chmod 700 "$launcher"
if [ "$platform" = linux ]; then
  mkdir -p "$applications"
  # Desktop Entry Exec quoted to support user home paths containing spaces.
  printf '[Desktop Entry]\nType=Application\nName=Lagosystem Bridge\nComment=Open your configured local device bridge website\nExec="%s"\nTerminal=false\nCategories=Utility;\n' "$launcher" > "$applications/lagosystem-bridge.desktop"
  chmod 644 "$applications/lagosystem-bridge.desktop"
else
  app="$HOME/Applications/Lagosystem Bridge.app"
  mkdir -p "$app/Contents/MacOS"
  cp "$launcher" "$app/Contents/MacOS/lagosystem-bridge-open"
  chmod 700 "$app/Contents/MacOS/lagosystem-bridge-open"
  cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>br.com.lagosystem.bridge</string>
<key>CFBundleName</key><string>Lagosystem Bridge</string>
<key>CFBundleExecutable</key><string>lagosystem-bridge-open</string>
<key>CFBundlePackageType</key><string>APPL</string>
</dict></plist>
PLIST
fi
echo "Installed $tag for current user. Configured URL: $site_url"
echo "Browser extensions still require installation and explicit site approval in their popups."
echo "Native Messaging launches the host ON DEMAND; do not autostart the stdio executable."
