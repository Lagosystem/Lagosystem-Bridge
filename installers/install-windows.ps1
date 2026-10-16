# SPDX-License-Identifier: GPL-3.0-or-later
# Per-user GitHub Release bootstrapper; never runs as administrator.
[CmdletBinding()]
param(
    [string]$SiteUrl,
    [string]$ChromiumExtensionIds,
    [switch]$Uninstall,
    [switch]$NonInteractive
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run in an unelevated session. Native Messaging registration is per-user.'
}
$root = Join-Path $env:LOCALAPPDATA 'Lagosystem.Bridge'
$manifestDirectory = Join-Path $root 'NativeMessaging'
$hostName = 'com.lagosystem.bridge'
$registryPaths = @{
    chrome = 'HKCU:\Software\Google\Chrome\NativeMessagingHosts'
    edge = 'HKCU:\Software\Microsoft\Edge\NativeMessagingHosts'
    brave = 'HKCU:\Software\BraveSoftware\Brave-Browser\NativeMessagingHosts'
    firefox = 'HKCU:\Software\Mozilla\NativeMessagingHosts'
}
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Lagosystem Bridge.url'

if ($Uninstall) {
    foreach ($browser in $registryPaths.Keys) {
        $key = Join-Path $registryPaths[$browser] $hostName
        $expected = Join-Path $manifestDirectory "$browser.json"
        if (Test-Path -LiteralPath $key) {
            $actual = (Get-Item -LiteralPath $key).GetValue('')
            if ($actual -and [string]::Equals($actual, $expected, [StringComparison]::OrdinalIgnoreCase)) {
                Remove-Item -LiteralPath $key -Recurse -Force
            }
        }
    }
    Remove-Item -LiteralPath $startMenu -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $manifestDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $root 'bin') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $root 'site-url.txt') -Force -ErrorAction SilentlyContinue
    Write-Host 'Lagosystem Bridge removed for the current user. Exports and pairings preserved.'
    return
}

if (-not $SiteUrl -and -not $NonInteractive) {
    $SiteUrl = Read-Host 'Website URL (HTTPS, localhost HTTP for development)'
}
$uri = $null
if (-not [uri]::TryCreate($SiteUrl, [UriKind]::Absolute, [ref]$uri) -or
    ($uri.Scheme -ne 'https' -and -not ($uri.Scheme -eq 'http' -and
        $uri.Host -in @('localhost', '127.0.0.1'))) -or
    $uri.UserInfo -or $uri.Fragment -or
    $uri.Host -match '[^a-zA-Z0-9.\-]' -or
    $uri.AbsoluteUri -match '\s' -or
    $uri.Port -lt 1 -or $uri.Port -gt 65535) {
    throw 'Invalid website URL. HTTPS is required except localhost and 127.0.0.1.'
}
if (-not $ChromiumExtensionIds -and -not $NonInteractive) {
    $ChromiumExtensionIds = Read-Host 'Chromium extension IDs (comma separated, optional)'
}
$origins = @()
foreach ($id in @($ChromiumExtensionIds -split ',')) {
    $id = $id.Trim()
    if (-not $id) { continue }
    if ($id -cnotmatch '^[a-p]{32}$') { throw "Invalid Chromium extension ID: $id" }
    $origins += "chrome-extension://$id/"
}
$origins = @($origins | Select-Object -Unique)

# 'latest' deliberately excludes beta releases. GitHub supplies the checksums
# beside the release assets; a signed checksum manifest is a future enhancement.
$headers = @{ 'User-Agent' = 'Lagosystem-Bridge-Installer' }
$release = Invoke-RestMethod -Headers $headers -Uri 'https://api.github.com/repos/Lagosystem/Lagosystem-Bridge/releases/latest'
$tag = [string]$release.tag_name
if ($tag -cnotmatch '^v\d+\.\d+\.\d+([.-][a-zA-Z0-9.-]+)?$') {
    throw 'No eligible stable release was found.'
}
$arch = if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq
    [System.Runtime.InteropServices.Architecture]::Arm64) { 'arm64' } else { 'x64' }
$name = "lagosystem-bridge-$tag-windows-$arch.zip"
$checksumName = "$name.sha256"
$binaryAsset = @($release.assets | Where-Object { $_.name -ceq $name }) | Select-Object -First 1
$hashAsset = @($release.assets | Where-Object { $_.name -ceq $checksumName }) | Select-Object -First 1
if (-not $binaryAsset -or -not $hashAsset) {
    throw "Release $tag is missing the executable or its SHA-256 file for $arch."
}
$temp = Join-Path ([IO.Path]::GetTempPath()) ("LagosystemBridge-" + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
try {
    $archive = Join-Path $temp $name
    $hashFile = Join-Path $temp $checksumName
    Invoke-WebRequest -Headers $headers -Uri $binaryAsset.browser_download_url -OutFile $archive
    Invoke-WebRequest -Headers $headers -Uri $hashAsset.browser_download_url -OutFile $hashFile
    $checksumContents = [IO.File]::ReadAllText($hashFile)
    $pattern = '(?im)^([a-f0-9]{64})\s+\*?' + [regex]::Escape($name) + '\s*$'
    $match = [regex]::Match($checksumContents, $pattern)
    if (-not $match.Success) { throw 'Invalid or missing release SHA-256 checksum.' }
    $expected = $match.Groups[1].Value.ToLowerInvariant()
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($expected -cne $actual) { throw 'SHA-256 mismatch. Aborted without installation.' }
    $extracted = Join-Path $temp 'unzipped'
    Expand-Archive -LiteralPath $archive -DestinationPath $extracted
    $source = Join-Path $extracted "lagosystem-bridge-$tag-windows-$arch\bin\lagosystem-bridge.exe"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'Host missing from verified archive.' }
    $bin = Join-Path $root 'bin'
    [IO.Directory]::CreateDirectory($bin) | Out-Null
    $target = Join-Path $bin 'lagosystem-bridge.exe'
    Copy-Item -LiteralPath $source -Destination $target -Force
    [IO.Directory]::CreateDirectory($manifestDirectory) | Out-Null
    $utf8 = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText((Join-Path $root 'site-url.txt'), $uri.AbsoluteUri, $utf8)

    function Add-Registration([string]$Browser, [string[]]$AllowedOrigins) {
        $manifest = @{
            name = $hostName
            description = 'Lagosystem Bridge native device connector'
            path = $target
            type = 'stdio'
        }
        if ($Browser -eq 'firefox') {
            $manifest.allowed_extensions = @('bridge@lagosystem.com.br')
        }
        else {
            if ($AllowedOrigins.Count -eq 0) { return }
            $manifest.allowed_origins = $AllowedOrigins
        }
        $path = Join-Path $manifestDirectory "$Browser.json"
        [IO.File]::WriteAllText($path, ($manifest | ConvertTo-Json -Depth 5), $utf8)
        $registry = Join-Path $registryPaths[$Browser] $hostName
        New-Item -Path $registry -Force | Out-Null
        Set-Item -Path $registry -Value $path
    }
    Add-Registration -Browser 'firefox' -AllowedOrigins @()
    foreach ($browser in @('chrome', 'edge', 'brave')) {
        Add-Registration -Browser $browser -AllowedOrigins $origins
    }
    # .url is a per-user Start Menu shortcut, not an elevated startup task.
    [IO.File]::WriteAllText($startMenu,
        "[InternetShortcut]" + [Environment]::NewLine + "URL=" + $uri.AbsoluteUri + [Environment]::NewLine, $utf8)
    Write-Host "Installed $tag for the current user. Website: $($uri.AbsoluteUri)"
    Write-Host 'Install browser extensions separately; approve the site in the extension popup.'
    Write-Host 'Native Messaging starts the host ON DEMAND; no autostart service is needed.'
}
finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}