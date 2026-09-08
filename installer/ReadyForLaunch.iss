; Build through scripts/package.ps1 using Inno Setup 6.3 or newer.
#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef AppNumericVersion
  #error AppNumericVersion is required
#endif

[Setup]
AppId={{A9DA31D1-B1F4-4DFA-A7FB-5C3E8D220FA4}
AppName=ReadyForLaunch
AppVersion={#AppVersion}
AppVerName=ReadyForLaunch {#AppVersion} (Alpha)
AppPublisher=Adam Chesters
AppPublisherURL=https://github.com/AdamChesters
AppSupportURL=https://github.com/AdamChesters/ReadyForLaunch/issues
AppUpdatesURL=https://github.com/AdamChesters/ReadyForLaunch/releases
DefaultDirName={localappdata}\Programs\ReadyForLaunch
DefaultGroupName=ReadyForLaunch
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=ReadyForLaunch-{#AppVersion}-Setup
SetupIconFile={#PackageDir}\assets\ReadyForLaunch.ico
UninstallDisplayIcon={app}\ReadyForLaunch.exe
VersionInfoVersion={#AppNumericVersion}
VersionInfoDescription=ReadyForLaunch Alpha Setup
LicenseFile={#PackageDir}\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
AppMutex=Local\ReadyForLaunch_v1
CloseApplications=no
RestartApplications=no
; AppMutex requires the launcher to exit before replacement. Never close the user's app stack.
; https://jrsoftware.org/ishelp/topic_setup_appmutex.htm

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\ReadyForLaunch"; Filename: "{app}\ReadyForLaunch.exe"
Name: "{userdesktop}\ReadyForLaunch"; Filename: "{app}\ReadyForLaunch.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ReadyForLaunch.exe"; Description: "Launch ReadyForLaunch Alpha"; Flags: nowait postinstall skipifsilent

; Profiles in {localappdata}\ReadyForLaunch are intentionally not installed or removed.
