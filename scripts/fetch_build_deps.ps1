# Prefetches the FetchContent sources as tarballs and extracts them into
# _deps-src/, so CMake can be configured fully offline via
# FETCHCONTENT_SOURCE_DIR_* overrides. Resumable + heavily retried for
# poor/flaky connections. Run from the repo root:
#
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_build_deps.ps1

$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot
$Cache = Join-Path $Root '_deps-cache'
$Src = Join-Path $Root '_deps-src'
New-Item -ItemType Directory -Force -Path $Cache, $Src | Out-Null

$Items = @(
    @{ Name = 'miniaudio'; Tag = '0.11.25';   Url = 'https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz' },
    @{ Name = 'whisper';   Tag = 'v1.9.3';    Url = 'https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.3.tar.gz' },
    @{ Name = 'json';      Tag = 'v3.12.0';   Url = 'https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz' },
    @{ Name = 'doctest';   Tag = 'v2.4.12';   Url = 'https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz' },
    @{ Name = 'glfw';      Tag = '3.4';       Url = 'https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz' },
    @{ Name = 'imgui';     Tag = 'v1.92.9b';  Url = 'https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz' }
)

$Attempts = 40
$Fail = $false

function Fetch-WithRetry($url, $out) {
    if ((Test-Path $out) -and ((Get-Item $out).Length -gt 0)) {
        Write-Host "[have] $(Split-Path -Leaf $out)"
        return $true
    }
    for ($i = 1; $i -le $Attempts; $i++) {
        Write-Host "[try $i/$Attempts] $url"
        $part = "$out.part"
        $eap = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & curl.exe -sS -fL --connect-timeout 20 --max-time 1800 --retry 5 --retry-delay 5 `
                --retry-all-errors -C - -o $part $url 2>&1 | Out-Null
        } catch { }
        $ErrorActionPreference = $eap
        if ($LASTEXITCODE -eq 0 -and (Test-Path $part) -and ((Get-Item $part).Length -gt 0)) {
            Move-Item -Force $part $out
            Write-Host "[ok] $(Split-Path -Leaf $out) ($((Get-Item $out).Length) bytes)"
            return $true
        }
        $delay = [Math]::Min(60, 5 * $i)
        Write-Host "[retry in ${delay}s] exit=$LASTEXITCODE"
        Start-Sleep -Seconds $delay
    }
    Write-Host "[FAIL] $url"
    return $false
}

foreach ($it in $Items) {
    $tarball = Join-Path $Cache "$($it.Name)-$($it.Tag).tar.gz"
    if (-not (Fetch-WithRetry $it.Url $tarball)) { $Fail = $true; continue }

    $dest = Join-Path $Src $it.Name
    if (-not (Test-Path $dest)) {
        $stage = Join-Path $Src "$($it.Name)-stage"
        if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
        New-Item -ItemType Directory -Force -Path $stage | Out-Null
        & tar.exe -xzf $tarball -C $stage
        if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] extract $tarball"; $Fail = $true; continue }
        $inner = Get-ChildItem $stage | Select-Object -First 1
        Move-Item $inner.FullName $dest
        Remove-Item -Recurse -Force $stage
        Write-Host "[extracted] $($it.Name)"
    } else {
        Write-Host "[have] $($it.Name) source"
    }
}

if ($Fail) {
    Write-Host 'DEPENDENCY FETCH INCOMPLETE - re-run this script; completed items are kept'
    exit 1
}

$Patch = Join-Path $Root 'patches\whisper-v1.9.3-ggml-alias.patch'
$WhisperDir = Join-Path $Src 'whisper'
$MarkerFile = Join-Path $WhisperDir 'ggml\src\CMakeLists.txt'
if (Select-String -Path $MarkerFile -Pattern 'NOT TARGET ggml::ggml' -Quiet) {
    Write-Host '[patch] already applied'
} else {
    Push-Location $WhisperDir
    try {
        & C:\msys64\usr\bin\patch.exe -p1 --batch --forward -i $Patch
        if ($LASTEXITCODE -ne 0) { Write-Host '[FAIL] whisper patch'; exit 1 }
    } finally {
        Pop-Location
    }
    if (Select-String -Path $MarkerFile -Pattern 'NOT TARGET ggml::ggml' -Quiet) {
        Write-Host '[patch] applied'
    } else {
        Write-Host '[FAIL] whisper patch marker missing after apply'; exit 1
    }
}

Write-Host 'ALL BUILD DEPS READY'
