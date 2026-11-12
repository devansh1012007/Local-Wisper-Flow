; Inno Setup script for Echo flow
; Per-user install, no admin required. Model weights are NOT bundled by this
; "online" installer; the first-run wizard downloads them with an explicit
; click. A separate full/offline variant (EchoFlowSetup-full.iss, built in CI)
; additionally ships the default speech model and the local cleanup LLM.
;
; The release process drops silero_vad.onnx (MIT license; confirm the exact
; model file's license header before redistribution) into third_party/ and
; the onnxruntime DLLs into third_party/onnxruntime/lib before compiling.
;
; CI passes the version explicitly: iscc /DAppVersion=x.y.z installer/lwf.iss
; (synced from CMake project VERSION - see docs/RELEASE.md). The default
; below keeps local double-click builds working.
;
; SmartScreen note: an unsigned exe triggers "Windows protected your PC".
; Users click "More info" -> "Run anyway". Code signing (see docs/RELEASE.md
; and the release workflow's conditional signtool step) removes the warning.
;
; Rename history (followups-6): the app was previously installed as
; "Local Wisper Flow" with a different AppId and install directory. That
; install keeps its own uninstaller; this script installs beside it under
; the EchoFlow directory and the app migrates %APPDATA%\LocalWisperFlow to
; %APPDATA%\EchoFlow on first start (src/os/DataDir.cpp). The legacy
; autostart Run value is carried over by the app and removed on install
; (CurStepChanged below).

#ifndef AppVersion
#define AppVersion "0.1.0"
#endif

#define AppName "Echo flow"
#define AppExe "EchoFlow.exe"

[Setup]
AppId={{D47CF84B-737B-43E1-B3DC-F3278C5B429A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Echo flow contributors
DefaultDirName={localappdata}\Programs\EchoFlow
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputBaseFilename=EchoFlowSetup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern
SetupIconFile=..\assets\app.ico

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
    ValueName: "EchoFlow"; ValueData: """{app}\{#AppExe}"""; Flags: uninsdeletevalue; \
    Tasks: autostart

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /IM {#AppExe} /F"; Flags: runhidden; RunOnceId: "KillApp"

[Code]
// The legacy install autostarted via the LocalWisperFlow Run value. Remove
// it on install (the app migrates the path to the EchoFlow value on first
// start; deleting both here avoids two autostart entries running side by
// side until that happens).
procedure CurStepChanged(CurStep: TSetupStep);
var
  RunKey: string;
begin
  if CurStep = ssInstall then
  begin
    RunKey := 'Software\Microsoft\Windows\CurrentVersion\Run';
    if RegValueExists(HKEY_CURRENT_USER, RunKey, 'LocalWisperFlow') then
      RegDeleteValue(HKEY_CURRENT_USER, RunKey, 'LocalWisperFlow');
  end;
end;

function ShouldRemoveData: Boolean;
var
  Answer: Integer;
begin
  Answer := MsgBox(
    'Do you also want to remove your data (settings, dictionary, history)?' + #13#10 +
    'Choose No to keep %APPDATA%\EchoFlow for a future reinstall.',
    mbConfirmation, MB_YESNO) = IDYES;
  Result := Answer;
end;

function ShouldRemoveModels: Boolean;
var
  Answer: Integer;
begin
  Answer := MsgBox(
    'Do you also want to remove the downloaded speech and language models?' + #13#10 +
    'They can be large (about 1 GB) and are downloaded again on demand.',
    mbConfirmation, MB_YESNO) = IDYES;
  Result := Answer;
end;

// Deletes everything directly inside DataDir except the models subdirectory.
procedure RemoveDirExceptModels(DataDir: string);
var
  FindRec: TFindRec;
  Child: string;
begin
  if FindFirst(DataDir + '\*', FindRec) then
  begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          Child := DataDir + '\' + FindRec.Name;
          if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
          begin
            if CompareText(FindRec.Name, 'models') <> 0 then
              DelTree(Child, True, True, True);
          end
          else
            DeleteFile(Child);
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    DataDir := ExpandConstant('{userappdata}') + '\EchoFlow';
    if DirExists(DataDir) and ShouldRemoveData then
    begin
      if ShouldRemoveModels then
        DelTree(DataDir, True, True, True)
      else
        // Keep the downloaded models; remove every other file and
        // subdirectory (config, logs, history, ...).
        RemoveDirExceptModels(DataDir);
    end;
    // Legacy data dir from a pre-rename install (unmigrated or abandoned).
    if DirExists(ExpandConstant('{userappdata}') + '\LocalWisperFlow') and
       ShouldRemoveData then
      DelTree(ExpandConstant('{userappdata}') + '\LocalWisperFlow', True, True, True);
  end;
end;
