#ifndef PrismVersion
  #define PrismVersion "1.0.0"
#endif
#ifndef PrismStage
  #define PrismStage "..\dist\PrismInstallerStage"
#endif
#ifndef PrismAppId
  #define PrismAppId "{{E81D3B23-36D1-4800-B47E-A0B727DD2314}"
#endif
#ifndef PrismName
  #define PrismName "Prism Browser"
#endif
#ifndef PrismFolder
  #define PrismFolder "Prism"
#endif
#ifndef PrismLaunchArguments
  #define PrismLaunchArguments ""
#endif
[Setup]
AppId={#PrismAppId}
AppName={#PrismName}
AppVersion={#PrismVersion}
DefaultDirName={localappdata}\Programs\{#PrismFolder}
DefaultGroupName={#PrismFolder}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\dist
OutputBaseFilename=Prism-Browser-{#PrismVersion}-Setup
Compression=lzma2/fast
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\Prism.exe
SetupIconFile=..\resources\prism.ico
LicenseFile=..\LICENSE
[Files]
Source: "{#PrismStage}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\{#PrismName}"; Filename: "{app}\Prism.exe"; Parameters: "{#PrismLaunchArguments}"
[Run]
Filename: "{app}\Prism.exe"; Parameters: "{#PrismLaunchArguments}"; Description: "Launch Prism Browser"; Flags: nowait postinstall skipifsilent
; Browsing data is deliberately excluded from uninstall deletion rules.
