# Package Echo flow for sharing (followups-4: "share the exe, must work
# on other PCs").
#
# What it does:
#   1. Builds the local-wisper-flow target (GUI subsystem, statically
#      linked GCC runtime - see CMakeLists.txt).
#   2. AUDITS the exe's import table: the package FAILS if the exe still
#      depends on a non-system DLL (libstdc++-6.dll, libgcc_s_seh-1.dll,
#      libwinpthread-1.dll, glfw3.dll, whisper.dll, ...) because those do
#      not exist on a clean machine. A missing audit is exactly the
#      "works here, not there" failure the report is about.
#   3. Stages a portable folder: the exe, the models.json download catalog
#      (read from the exe dir at startup) and a short README.
#   4. Zips it as EchoFlow-<version>-win64.zip under dist/.
#
# usage (MSYS2 MinGW shell or PowerShell, from the repo root):
#   powershell -ExecutionPolicy Bypass -File scripts/package_windows.ps1
# options:
#   -BuildDir build   -OutDir dist
#
# No code signing: Windows SmartScreen may warn recipients about an
# unsigned exe ("More info" -> "Run anyway"); that is expected for a
# personally shared binary and cannot be fixed without a certificate.

param(
    [string]$BuildDir = "build",
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root $BuildDir
$Out = Join-Path $Root $OutDir

# ---------- build ----------
Write-Host "[package] building local-wisper-flow ..."
& cmake --build $Build --target local-wisper-flow --parallel 4
if ($LASTEXITCODE -ne 0) {
    throw "build failed (exit $LASTEXITCODE). Run scripts/build_retry.ps1 first if the two-phase llama configure is not done yet."
}

$Exe = Join-Path $Build "local-wisper-flow.exe"
if (-not (Test-Path $Exe)) {
    throw "built exe not found at $Exe"
}

# ---------- version ----------
$Version = "0.0.0"
$AppVersionHeader = Join-Path $Root "src/core/AppVersion.h"
$CMakeLists = Get-Content (Join-Path $Root "CMakeLists.txt") -Raw
if ($CMakeLists -match 'project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    $Version = $Matches[1]
}

# ---------- DLL audit ----------
# objdump comes with the MinGW toolchain (MSYS2: it is on PATH).
$Objdump = Get-Command objdump -ErrorAction SilentlyContinue
if ($null -eq $Objdump) {
    $Objdump = Get-Command x86_64-w64-mingw32-objdump -ErrorAction SilentlyContinue
}
if ($null -eq $Objdump) {
    throw "objdump not found - run from the MSYS2 MinGW shell where the toolchain lives"
}

Write-Host "[package] auditing import table of local-wisper-flow.exe ..."
$dump = & $Objdump.Source -p $Exe
$dlls = @($dump | Select-String 'DLL Name:' | ForEach-Object {
    if ($_.Line -match 'DLL Name:\s*(\S+)') { $Matches[1] } })

# Windows system DLLs (present on every Win10/11 box). Everything else -
# MinGW runtime, third-party shared libs - must not appear: the audit
# exists so a dep regression fails HERE instead of on someone else's PC.
$SystemDlls = @(
    'kernel32', 'user32', 'gdi32', 'shell32', 'advapi32', 'ole32',
    'oleaut32', 'ws2_32', 'crypt32', 'bcrypt', 'winmm', 'imm32',
    'version', 'comctl32', 'comdlg32', 'shlwapi', 'dwmapi', 'wtsapi32',
    'winhttp', 'opengl32', 'glu32', 'msvcrt', 'ucrtbase', 'ntdll',
    'rpcrt4', 'secur32', 'iphlpapi', 'setupapi', 'userenv', 'wintrust',
    'msimg32', 'dbghelp', 'kernelbase', 'mswsock', 'dnsapi'
)
$foreign = @($dlls | Where-Object {
    $name = $_ -replace '\.dll$', ''
    ($SystemDlls -notcontains $name.ToLowerInvariant()) })

Write-Host "[package] imports: $($dlls -join ', ')"
if ($foreign.Count -gt 0) {
    throw ("exe still depends on non-system DLL(s): $($foreign -join ', '). " +
           "The exe would NOT run on a machine without these. " +
           "Check the -static link options on local-wisper-flow in CMakeLists.txt.")
}

# ---------- stage ----------
$Stage = Join-Path $Out "EchoFlow"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

Copy-Item $Exe (Join-Path $Stage "EchoFlow.exe")
Write-Host "[package] staged EchoFlow.exe"

# models.json: the first-run model download catalog, read from the EXE dir
# (Onboarding::ParseModelsJson(GetExeDir() + "\models.json")). Without it the
# Welcome page cannot offer the model download.
$ModelsJson = Join-Path $Root "models.json"
if (Test-Path $ModelsJson) {
    Copy-Item $ModelsJson $Stage
    Write-Host "[package] staged models.json (first-run model download catalog)"
} else {
    Write-Warning "models.json not found in the repo root - first-run model download will be unavailable"
}

# README for the recipient.
$Readme = @"
Echo flow $Version - local dictation for Windows

Run EchoFlow.exe. On first start it opens a short welcome page and can
download the speech model (about 150 MB) - an internet connection is
needed once. After that everything runs offline on this machine.

What is stored, and where:
  %APPDATA%\LocalWisperFlow\
      config.json     settings
      logs\           log files
      history.jsonl   dictation history (if enabled)
      models\         downloaded speech model

Hold the push-to-talk hotkey and speak; release to type. Right-click the
tray icon for settings and to quit. If Windows SmartScreen warns about an
unknown publisher, choose "More info" then "Run anyway" - the app is not
code-signed.

Shared as a zip; no installation and no other files are required.
"@
$Readme | Out-File -FilePath (Join-Path $Stage "README.txt") -Encoding utf8
Write-Host "[package] staged README.txt"

# ---------- zip ----------
$Zip = Join-Path $Out ("EchoFlow-" + $Version + "-win64.zip")
if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip
$Size = "{0:N1}" -f ((Get-Item $Zip).Length / 1MB)
Write-Host "[package] OK: $Zip ($Size MB)"
Write-Host "[package] share the zip (not the bare exe) so models.json and README.txt travel with it."
