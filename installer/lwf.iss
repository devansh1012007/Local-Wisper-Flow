; Inno Setup script for Echo flow
; Per-user install, no admin required.
;
; Two variants:
;   - online (default): iscc installer/lwf.iss - the model weights are NOT
;     bundled; the first-run wizard / Models page downloads them with an
;     explicit click, hash-verified against models.json.
;   - full/offline: iscc /DAppVersion=x.y.z /DFullBundle installer/lwf.iss
;     additionally ships the default speech model and the local cleanup
;     LLM as optional components. The weights must be staged in
;     dist/models/ first (the release workflow downloads them from the
;     pinned URLs in models.json and verifies the sha256 values; see
;     docs/RELEASE.md). Redistribution is licence-clean: whisper weights
;     MIT, Qwen2.5 GGUF Apache-2.0 (see NOTICE.md).
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
#ifdef FullBundle
OutputBaseFilename=EchoFlowSetup-{#AppVersion}-full
#else
OutputBaseFilename=EchoFlowSetup-{#AppVersion}
#endif
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern
SetupIconFile=..\assets\app.ico

#ifdef FullBundle
[Types]
Name: "full"; Description: "Full installation (includes the local models)"
Name: "app"; Description: "Application only (models download on first run)"

[Components]
Name: "stt"; Description: "Speech model: Whisper Base, English (~150 MB)"; Types: full
Name: "llm"; Description: "Local cleanup LLM: Qwen2.5 1.5B Instruct Q4_K_M (~1.1 GB)"; Types: full
#endif

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
#ifdef FullBundle
; Model weights install straight into the app's data dir so the existing
; ResolveModelPath probing (%APPDATA%\EchoFlow\models/<name>) finds them
; without a config change. They land next to wizard-downloaded models.
; uninsneveruninstall: the uninstaller's own keep/remove prompts decide
; their fate - Inno must not silently delete them.
Source: "..\dist\models\ggml-base.en.bin"; DestDir: "{code:GetModelsDir}"; Components: stt; Flags: ignoreversion uninsneveruninstall
Source: "..\dist\models\qwen2.5-1.5b-instruct-q4_k_m.gguf"; DestDir: "{code:GetModelsDir}"; Components: llm; Flags: ignoreversion uninsneveruninstall
#endif

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
// Full bundle only. The app resolves "models/<name>" against
// %APPDATA%\<data-dir-name>, so the weights must land in the CURRENT data
// dir: EchoFlow when it exists (or will be created by the app), the legacy
// LocalWisperFlow when the rename migration will pick it up on the next
// start (src/os/DataDir.cpp), EchoFlow otherwise.
#ifdef FullBundle
function GetSeedDataDir: string;
begin
  Result := ExpandConstant('{userappdata}') + '\EchoFlow';
  if not DirExists(Result) and
     DirExists(ExpandConstant('{userappdata}') + '\LocalWisperFlow') then
    Result := ExpandConstant('{userappdata}') + '\LocalWisperFlow';
end;

function GetModelsDir(Param: string): string;
begin
  Result := GetSeedDataDir + '\models';
  if not DirExists(Result) then
    ForceDirectories(Result);
end;

// Fresh installs: seed a minimal config.json so the bundled weights are
// found on the very first start (bare schema keys keep their defaults -
// SettingsStore is additive). An existing config is the user's; the Models
// page can activate the bundled models there. With the speech component
// selected the first-run wizard is skipped (the model is already present).
procedure SeedFullBundleConfig;
var
  ConfigPath: string;
  Content: string;
begin
  ConfigPath := GetSeedDataDir + '\config.json';
  if FileExists(ConfigPath) then
    exit;
  Content := '{';
  if IsComponentSelected('stt') then
    Content := Content + #13#10 + '    "whisper_model_path": "models/ggml-base.en.bin"';
  if IsComponentSelected('llm') then
  begin
    if IsComponentSelected('stt') then
      Content := Content + ',';
    Content := Content + #13#10 +
      '    "llm_model_path": "models/qwen2.5-1.5b-instruct-q4_k_m.gguf"';
  end;
  if IsComponentSelected('stt') then
    Content := Content + ',' + #13#10 + '    "first_run": false';
  Content := Content + #13#10 + '}';
  if Content <> '{}' then
    SaveStringToFile(ConfigPath, Content, False);
end;
#endif

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
#ifdef FullBundle
  if CurStep = ssPostInstall then
    SeedFullBundleConfig;
#endif
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
