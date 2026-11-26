; Installeur Windows - MBK De-noiser PRO (VST3)
; Compilation : iscc /DAppVersion=2.0.0 installer\MBK_DenoiserPro.iss

#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#define AppName      "MBK De-noiser PRO"
#define AppPublisher "MBK"
#define AppURL       "https://github.com/josh-mbk/MBK-De-noiser-PRO"
#ifndef VstSource
  #define VstSource "..\build\MBKDenoiserPro_artefacts\Release\VST3\MBK De-noiser PRO.vst3"
#endif

[Setup]
AppId={{7C2E9B14-5D3A-4F68-8A1B-2E6D90C4A7F3}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
AppCopyright=Copyright (C) MBK
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile=license.txt
OutputDir=..\dist
OutputBaseFilename=MBK-De-noiser-PRO-{#AppVersion}-Windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard_small.bmp
SetupIconFile=icon.ico
UninstallDisplayIcon={uninstallexe}
UninstallDisplayName={#AppName} (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion=2.0.0.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} VST3 Setup

[Languages]
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#VstSource}\*"; DestDir: "{commoncf64}\VST3\MBK De-noiser PRO.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\MBK De-noiser PRO.vst3"
