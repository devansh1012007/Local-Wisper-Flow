. .\scripts\capture_baseline.ps1
$env:LWF_HOME = (Resolve-Path build\t9-lwf-home).Path
Start-Process -FilePath (Resolve-Path build\local-wisper-flow.exe).Path -WorkingDirectory (Get-Location).Path
Start-Sleep -Seconds 10
$w = Wait-ForTopWindow -Title 'Local Wisper Flow' -TimeoutSeconds 30 -RequireVisible
if ($null -eq $w) { throw 'welcome window never appeared' }
Set-WindowClientSize -Hwnd $w.Hwnd -ClientWidth 960 -ClientHeight 640 | Out-Null
Start-Sleep -Seconds 2
Capture-WindowToFile -Hwnd $w.Hwnd -Path 'docs\agent-ops\20261109-redesign-v2-t9\t9_welcome_960x640.png' -Method Screen | Out-Null
Set-WindowClientSize -Hwnd $w.Hwnd -ClientWidth 760 -ClientHeight 520 | Out-Null
Start-Sleep -Seconds 2
Capture-WindowToFile -Hwnd $w.Hwnd -Path 'docs\agent-ops\20261109-redesign-v2-t9\t9_welcome_760x520.png' -Method Screen | Out-Null
Write-Output 'welcome captures done'
