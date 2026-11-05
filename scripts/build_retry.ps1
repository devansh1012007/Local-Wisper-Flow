# Retry-wrapped build for LocalWisperFlow.
# Phase 1: builds llama_ext + all targets.
# Phase 2: re-runs cmake configure so the full static-link closure from
# build/llama-install is picked up.
#
# usage: powershell -ExecutionPolicy Bypass -File scripts\build_retry.ps1

$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build'
$Attempts = 30
$DelayBase = 10   # seconds, grows per attempt (max 120)

function Run-WithRetry {
    param([string]$Label, [string[]]$Cmd)
    $CmdStr = $Cmd -join ' '
    for ($i = 1; $i -le $Attempts; $i++) {
        $delay = [Math]::Min(120, $DelayBase * $i)
        Write-Host "[$Label] attempt $i/$Attempts: $CmdStr"
        try {
            & $Cmd 2>&1
            $rc = $LASTEXITCODE
        } catch {
            Write-Host "[$Label] exception: $_"
            $rc = 1
        }
        if ($rc -eq 0) {
            Write-Host "[$Label] OK"
            return $true
        }
        Write-Host "[$Label] FAILED exit=$rc — retrying in ${delay}s ..."
        Start-Sleep -Seconds $delay
    }
    Write-Host "[$Label] FAILED after $Attempts attempts"
    return $false
}

# ---------- Phase 1 build ----------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($null -eq $cmake) { $cmake = 'cmake' } else { $cmake = $cmake.Source }

$ok = Run-WithRetry -Label "phase1-build" -Cmd @(
    $cmake, '--build', $Build, '--config', 'Release', '--parallel', '4'
)
if (-not $ok) { Write-Host 'BUILD PHASE 1 FAILED'; exit 1 }

# ---------- Phase 2 re-configure ----------
$ok = Run-WithRetry -Label "phase2-reconfigure" -Cmd @(
    $cmake, '-S', $Root, '-B', $Build,
    '-G', 'MinGW Makefiles',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_CXX_COMPILER=g++', '-DCMAKE_C_COMPILER=gcc',
    '-DLLAMA_URL=file://C:/Users/devan/Documents/Local-Wisper-Flow/_deps-cache/llama-b10603.tar.gz',
    '-DLLAMA_JOBS=4',
    '-DFETCHCONTENT_SOURCE_DIR_MINIAUDIO=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/miniaudio',
    '-DFETCHCONTENT_SOURCE_DIR_WHISPER=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/whisper',
    '-DFETCHCONTENT_SOURCE_DIR_JSON=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/json',
    '-DFETCHCONTENT_SOURCE_DIR_DOCTEST=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/doctest',
    '-DFETCHCONTENT_SOURCE_DIR_GLFW=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/glfw',
    '-DFETCHCONTENT_SOURCE_DIR_IMGUI=C:/Users/devan/Documents/Local-Wisper-Flow/_deps-src/imgui',
    '-DLWF_BUILD_GRPC=OFF',
    '-DONNXRUNTIME_DIR=C:/Users/devan/Documents/Local-Wisper-Flow/third_party/onnxruntime'
)
if (-not $ok) { Write-Host 'BUILD PHASE 2 RECONFIGURE FAILED'; exit 1 }

# ---------- Phase 2 build ----------
$ok = Run-WithRetry -Label "phase2-build" -Cmd @(
    $cmake, '--build', $Build, '--config', 'Release', '--parallel', '4'
)
if (-not $ok) { Write-Host 'BUILD PHASE 2 FAILED'; exit 1 }

Write-Host 'ALL BUILD PHASES COMPLETE'
