# SPDX-License-Identifier: GPL-3.0-or-later
# Installs Native Messaging manifests ONLY for current Windows user. Does not run hardware actions.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$ChromeExtensionId,
    [string]$EdgeExtensionId,
    [string]$BraveExtensionId
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this installer as a standard user, never an elevated administrator.'
}
$exe = [IO.Path]::GetFullPath($Executable)
if (-not [IO.File]::Exists($exe)) { throw 'Host executable does not exist.' }
$directory = Join-Path $env:LOCALAPPDATA 'Lagosystem.Bridge\NativeMessaging'
[IO.Directory]::CreateDirectory($directory) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$name = 'com.lagosystem.bridge'

function Set-Manifest {
    param([string]$Browser, [string]$Id, [string]$Registry)
    if ($Id -notmatch '^[a-p]{32}$') { throw "Invalid extension ID for $Browser." }
    $manifestPath = Join-Path $directory ($Browser + '.json')
    $manifest = @{
        name = $name
        description = 'Local Device Bridge GPL native host'
        type = 'stdio'
        path = $exe
        allowed_origins = @("chrome-extension://$Id/")
    }
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), $utf8)
    $key = "HKCU:\Software\$Registry\NativeMessagingHosts\$name"
    New-Item -Path $key -Force | Out-Null
    Set-Item -Path $key -Value $manifestPath
    Write-Host "Registered $Browser for this user."
}

if ($ChromeExtensionId) { Set-Manifest -Browser 'chrome' -Id $ChromeExtensionId -Registry 'Google\Chrome' }
if ($EdgeExtensionId) { Set-Manifest -Browser 'edge' -Id $EdgeExtensionId -Registry 'Microsoft\Edge' }
if ($BraveExtensionId) { Set-Manifest -Browser 'brave' -Id $BraveExtensionId -Registry 'BraveSoftware\Brave-Browser' }

$firefoxPath = Join-Path $directory 'firefox.json'
$firefox = @{
    name = $name; description = 'Local Device Bridge GPL native host'
    type = 'stdio'; path = $exe
    allowed_extensions = @('bridge@lagosystem.com.br')
}
[IO.File]::WriteAllText($firefoxPath, ($firefox | ConvertTo-Json -Depth 5), $utf8)
$firefoxKey = "HKCU:\Software\Mozilla\NativeMessagingHosts\$name"
New-Item -Path $firefoxKey -Force | Out-Null
Set-Item -Path $firefoxKey -Value $firefoxPath
Write-Host 'Registered Firefox for this user.'
Write-Host 'No administrator privilege or system-wide installation was used.'
