# Stage the model weights for the full/offline installer variant.
#
# Downloads the files listed in models.json (the single source of truth for
# URLs and sha256 - same values the in-app downloader enforces) into
# dist/models/ and VERIFIES each hash before staging. A mismatch fails the
# release: the installer must never ship weights that the app would later
# refuse.
#
# usage (from the repo root, on the release runner or locally):
#   powershell -ExecutionPolicy Bypass -File scripts/prepare_full_bundle.ps1
#
# Only the entries the full bundle actually ships are staged: the first
# whisper entry (English base) and the first llm entry.

param(
    [string]$OutDir = "dist/models"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$CatalogPath = Join-Path $Root "models.json"

$Catalog = Get-Content $CatalogPath -Raw | ConvertFrom-Json
$Wanted = @()
if ($Catalog.whisper.Count -gt 0) { $Wanted += $Catalog.whisper[0] }
if ($Catalog.llm.Count -gt 0) { $Wanted += $Catalog.llm[0] }
if ($Wanted.Count -eq 0) {
    throw "models.json has no whisper/llm entries - nothing to stage"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

foreach ($Model in $Wanted) {
    if (-not $Model.url) { throw "model '$($Model.name)' has no url - refusing to stage" }
    if (-not $Model.sha256) { throw "model '$($Model.name)' has no sha256 - refusing to stage" }
    $Dest = Join-Path $OutDir $Model.name
    if ((Test-Path $Dest) -and (Get-FileHash -Algorithm SHA256 $Dest).Hash.ToLowerInvariant() -eq $Model.sha256) {
        Write-Host "[bundle] $($_) already staged and verified: $($Model.name)"
        continue
    }
    Write-Host "[bundle] downloading $($Model.name) ..."
    Invoke-WebRequest -Uri $Model.url -OutFile $Dest -MaximumRedirection 10
    $Actual = (Get-FileHash -Algorithm SHA256 $Dest).Hash.ToLowerInvariant()
    if ($Actual -ne $Model.sha256) {
        Remove-Item -Force $Dest
        throw "sha256 mismatch for $($Model.name): expected $($Model.sha256), got $Actual"
    }
    $Size = "{0:N1}" -f ((Get-Item $Dest).Length / 1MB)
    Write-Host "[bundle] verified: $($Model.name) ($Size MB)"
}
Write-Host "[bundle] staging complete in $OutDir"
