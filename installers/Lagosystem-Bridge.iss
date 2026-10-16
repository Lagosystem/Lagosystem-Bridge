; SPDX-License-Identifier: GPL-3.0-or-later
; Inno Setup 6 / per-user web bootstrapper (requires Internet access).
; Native Messaging is stdio on demand; no Windows autostart service is registered.
#define ApplicationName "Lagosystem Bridge"
#define Version "0.2.0"

[Setup]
AppId={{53C41AB1-2AD3-4350-B077-BD2D5D45AE15}
AppName={#ApplicationName}
AppVersion={#Version}
AppPublisher=Lagosystem
AppPublisherURL=https://github.com/Lagosystem/Lagosystem-Bridge
DefaultDirName={localappdata}\Lagosystem.Bridge\Installer
DefaultGroupName=Lagosystem Bridge
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible arm64
OutputBaseFilename=Lagosystem-Bridge-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\lagosystem_native_ico.ico
UninstallDisplayIcon=..\assets\lagosystem_native_ico.ico

[Files]
Source: "install-windows.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\install-windows.ps1"" -SiteUrl ""{code:GetSiteUrl}"" -ChromiumExtensionIds ""{code:GetIds}"" -NonInteractive"; Flags: runhidden waituntilterminated; StatusMsg: "Baixando e verificando o Native Host e configurando o navegador..."

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\install-windows.ps1"" -Uninstall"; Flags: runhidden waituntilterminated

[Code]
var
  ConfigurationPage: TInputQueryWizardPage;

procedure InitializeWizard();
begin
  ConfigurationPage := CreateInputQueryPage(
    wpSelectDir,
    'Configuração da integração',
    'URL do sistema que você deseja utilizar',
    'Informe o endereço HTTPS completo, inclusive porta personalizada e caminho, se existirem. HTTP somente localhost/127.0.0.1. A autorização dos periféricos continua exigindo consentimento na extensão.');

  ConfigurationPage.Add('URL do sistema:', False);
  ConfigurationPage.Values[0] := 'https://example.com';

  ConfigurationPage.Add('IDs Chromium (32 letras a-p, separados por vírgula; opcional):', False);
  ConfigurationPage.Values[1] := '';
end;

function GetSiteUrl(Param: string): string;
begin
  Result := Trim(ConfigurationPage.Values[0]);
end;

function GetIds(Param: string): string;
begin
  Result := Trim(ConfigurationPage.Values[1]);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Site: string;
  Ids: string;
  Index: Integer;
begin
  Result := True;
  if CurPageID <> ConfigurationPage.ID then Exit;
  Site := GetSiteUrl('');
  Ids := GetIds('');
  if (Pos('"', Site) > 0) or (Pos(#13, Site) > 0) or (Pos(#10, Site) > 0) or
     ((Pos('https://', LowerCase(Site)) <> 1) and
       (Pos('http://localhost', LowerCase(Site)) <> 1) and
       (Pos('http://127.0.0.1', LowerCase(Site)) <> 1)) then
  begin
    MsgBox('Informe uma URL HTTPS válida (HTTP somente para localhost).', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  for Index := 1 to Length(Ids) do
  begin
    if not ((Ids[Index] in ['a'..'p']) or (Ids[Index] = ',')) then
    begin
      MsgBox('IDs Chromium devem conter apenas letras a-p e vírgulas.', mbError, MB_OK);
      Result := False;
      Exit;
    end;
  end;
end;
