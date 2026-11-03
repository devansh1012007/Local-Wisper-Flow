; Inno Setup script for Local Wisper Flow
; Per-user install, no admin required. Model weights are NOT bundled;
; the first-run wizard downloads them with an explicit click.
; The release process drops silero_vad.onnx (MIT license; confirm the exact
; model file's license header before redistribution) into third_party/ and
; the onnxruntime DLLs into third_party/onnxruntime/lib before compiling.
;
; SmartScreen note: an unsigned exe triggers "Windows protected your PC".
; Users click "More info" -> "Run anyway". Code signing with an EV or OV
; certificate (signtool sign /fd SHA256 /tr <timestamp> /td SHA256) removes
; the warning; that step belongs to the release process, not the installer.

#define AppName "Local Wisper Flow"
#define AppVersion "0.1.0"
#define AppExe "local-wisper-flow.exe"

[Setup]
AppId={{A6006D2C-635B-4CA1-9391-7070FB09B864}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Local Wisper Flow contributors
DefaultDirName={localappdata}\Programs\LocalWisperFlow
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputBaseFilename=LocalWisperFlowSetup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; Flags: unchecked
Name: "autostart"; Description: "Start {#AppName} when Windows starts"; Flags: unchecked

[Files]
Source: "..\build\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\onnxruntime\lib\onnxruntime.dll"; DestDir: "{app}"; Flags: ignoreversion skipifdoesntexist
Source: "..\third_party\onnxruntime\lib\onnxruntime_providers_shared.dll"; DestDir: "{app}"; Flags: ignoreversion skipifdoesntexist
Source: "..\third_party\silero_vad.onnx"; DestDir: "{app}"; Flags: ignoreversion skipifdoesntexist
Source: "..\config.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\models.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\transforms.json"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "LocalWisperFlow"; ValueData: """{app}\{#AppExe}"""; Flags: uninsdeletevalue; \
    Tasks: autostart

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /IM {#AppExe} /F"; Flags: runhidden; RunOnceId: "KillApp"

[Code]
function ShouldRemoveData: Boolean;
var
  Answer: Integer;
begin
  Answer := MsgBox(
    'Do you also want to remove your data (settings, dictionary, history)?' + #13#10 +
    'Choose No to keep %APPDATA%\LocalWisperFlow for a future reinstall.',
    mbConfirmation, MB_YESNO) = IDYES;
  Result := Answer;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    DataDir := ExpandConstant('{userappdata}') + '\LocalWisperFlow';
    if DirExists(DataDir) and ShouldRemoveData then
      DelTree(DataDir, True, True, True);
  end;
end;
