; Inno Setup script for FloCMS Studio on Windows (design note D12).
; CI builds it with:
;   ISCC.exe /DAppVersion=0.0.1 /DSourceDir=<deployed folder with Studio.exe> studio.iss
;
; Installs for the current user by default (no administrator rights needed);
; the user can choose all users in the first page.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\FloCMS-Studio"
#endif

[Setup]
; Never change AppId: Windows uses it to find the installed app for updates
; and uninstalling.
AppId={{16604886-7502-402D-879A-8B0940B19498}
AppName=FloCMS Studio
AppVersion={#AppVersion}
AppPublisher=HostKurd
AppPublisherURL=https://github.com/hostkurd/flocms-studio
AppSupportURL=https://github.com/hostkurd/flocms-studio/issues
DefaultDirName={autopf}\FloCMS Studio
DefaultGroupName=FloCMS Studio
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=FloCMS-Studio-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Studio.exe
UninstallDisplayName=FloCMS Studio
LicenseFile={#SourceDir}\LICENSE.txt

[Languages]
; Arabic and Kurdish installer texts come with the release (stage 9);
; Inno Setup ships neither language.
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Studio.exe with the FloStudio and Qt DLLs, Qt's plugins and QML files,
; the Visual C++ runtime DLLs, LICENSE.txt and THIRD-PARTY-NOTICES.txt.
; (The final release, stage 9, installs one static Studio.exe instead.)
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\FloCMS Studio"; Filename: "{app}\Studio.exe"
Name: "{autodesktop}\FloCMS Studio"; Filename: "{app}\Studio.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Studio.exe"; Description: "{cm:LaunchProgram,FloCMS Studio}"; Flags: nowait postinstall skipifsilent
