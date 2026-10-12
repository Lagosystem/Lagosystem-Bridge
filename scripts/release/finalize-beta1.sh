#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Finalize ONLY the draft with exact assets/checksums from the tested SHA.
set -euo pipefail

: "${GH_TOKEN:?GH_TOKEN is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"
: "${RELEASE_TAG:?RELEASE_TAG is required}"
: "${EXPECTED_TARGET_SHA:?EXPECTED_TARGET_SHA is required}"

if [[ "$RELEASE_TAG" != "v0.2.0-beta.1" ]]; then
  echo "::error::This script is locked to v0.2.0-beta.1"
  exit 1
fi
if [[ ! "$EXPECTED_TARGET_SHA" =~ ^[[:xdigit:]]{40}$ ]]; then
  echo "::error::Expected a complete 40-character target commit SHA"
  exit 1
fi

# Draft release tags may not be visible via refs or /releases/tags/{tag}.
# The authenticated releases list includes the unpublished draft.
release_json="$(
  gh api "repos/${GITHUB_REPOSITORY}/releases?per_page=100" |
    jq -ce --arg tag "$RELEASE_TAG" \
      '[.[] | select(.tag_name == $tag)] | if length == 1 then .[0] else error("Expected exactly one beta draft") end'
)"
is_draft="$(jq -r '.draft' <<<"$release_json")"
is_prerelease="$(jq -r '.prerelease' <<<"$release_json")"
target_sha="$(jq -r '.target_commitish' <<<"$release_json")"
[[ "$is_draft" == true ]] || { echo "::error::Release is not an unpublished draft"; exit 1; }
[[ "$is_prerelease" == true ]] || { echo "::error::Release is not marked prerelease"; exit 1; }
[[ "$target_sha" == "$EXPECTED_TARGET_SHA" ]] || {
  echo "::error::Draft targets $target_sha, expected $EXPECTED_TARGET_SHA"; exit 1;
}

expected=(
  "lagosystem-bridge-v0.2.0-beta.1-windows-x64.zip"
  "lagosystem-bridge-v0.2.0-beta.1-linux-x64.tar.gz"
  "lagosystem-bridge-v0.2.0-beta.1-macos-arm64.tar.gz"
  "lagosystem-bridge-v0.2.0-beta.1-chromium.zip"
  "lagosystem-bridge-v0.2.0-beta.1-firefox.zip"
)
count="$(jq '.assets | length' <<<"$release_json")"
[[ "$count" == 10 ]] || { echo "::error::Expected exactly 10 assets, found $count"; exit 1; }
for archive in "${expected[@]}"; do
  for filename in "$archive" "$archive.sha256"; do
    present="$(jq --arg name "$filename" '[.assets[] | select(.name == $name and .state == "uploaded" and .size > 0)] | length' <<<"$release_json")"
    [[ "$present" == 1 ]] || { echo "::error::Missing/invalid asset $filename"; exit 1; }
  done
done

# Beta1 assets total about 1.2 MB. No Actions cache or artifacts.
scratch="$(mktemp -d "${RUNNER_TEMP:-/tmp}/lagosystem-bridge-beta1.XXXXXXXX")"
trap 'rm -rf -- "$scratch"' EXIT
gh release download "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" --dir "$scratch"
download_count="$(find "$scratch" -maxdepth 1 -type f | wc -l | tr -d '[:space:]')"
[[ "$download_count" == 10 ]] || { echo "::error::Expected 10 downloaded assets, got $download_count"; exit 1; }
(
  cd "$scratch"
  for archive in "${expected[@]}"; do
    sha256sum --check --strict "$archive.sha256"
  done
)
# The ONLY action that makes this beta visible publicly.
gh release edit "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" \
  --draft=false --prerelease --latest=false
echo "Published $RELEASE_TAG after checking 5 archives and 5 SHA-256 checksums."
