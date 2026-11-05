# Retries heavily for poor connections. Saves to repo root as the
# filenames that config.json expects when placed next to the binary.
$Root = Split-Path -Parent $PSScriptRoot
$Attempts = 30

function Fetch-WithRetry($Url, $FileName) {
    $Out = Join-Path $Root $FileName
    if (Test-Path $Out) {
        $len = (Get-Item $Out).Length
        # Accept partial downloads < 90% for resume; full = done
        Write-Host "[have] $FileName ($len bytes)"
        return $true
    }
    for ($i = 1; $i -le $Attempts; $i++) {
        $delay = [Math]::Min(60, 5 * $i)
        Write-Host "[try $i/$Attempts] $Url ($FileName)"
        $part = "$Out.part"
        $prevLen = 0
        if (Test-Path $part) { $prevLen = (Get-Item $part).Length }
        $eap = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        try {
            # -C - tries to resume partial .part file; -fL follows redirects
            & curl.exe -sS -L --connect-timeout 20 --max-time 3600 -C - `
                --retry 10 --retry-delay 10 --retry-all-errors -o $part $Url 2>&1 | Out-Null
        } catch { }
        $ErrorActionPreference = $eap
        if ($LASTEXITCODE -eq 0 -and (Test-Path $part)) {
            Move-Item -Force $part $Out
            $len = (Get-Item $Out).Length
            Write-Host "[ok] $FileName ($len bytes)"
            return $true
        }
        # If we got a partial file and it grew, keep it for -C - resume next loop
        Write-Host "[retry in ${delay}s] exit=$LASTEXITCODE part=$prevLen"
        Start-Sleep -Seconds $delay
    }
    Write-Host "[FAIL] $FileName after $Attempts attempts"
    return $false
}

# User choice: base.en (~148 MB) for Whisper
Fetch-WithRetry 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin' 'ggml-base.en.bin'
# User choice: configured Phi-4-mini-reasoning Q8 (~4.7 GB) — may not finish quickly; we keep retrying
New-Item -ItemType Directory -Force -Path (Join-Path $Root 'models/Phi-4-mini-reasoning-GGUF') | Out-Null
Fetch-WithRetry 'https://huggingface.co/bartowski/Phi-4-mini-reasoning-GGUF/resolve/main/Phi-4-mini-reasoning-Q8_0.gguf' 'models/Phi-4-mini-reasoning-GGUF/Phi-4-mini-reasoning-Q8_0.gguf'
