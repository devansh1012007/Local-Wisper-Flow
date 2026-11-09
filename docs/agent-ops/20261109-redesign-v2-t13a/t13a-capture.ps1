. .\scripts\capture_baseline.ps1
$env:LWF_HOME = (Resolve-Path build\t13a-lwf-home).Path
Start-Process -FilePath (Resolve-Path build\local-wisper-flow.exe).Path -WorkingDirectory (Get-Location).Path
Start-Sleep -Seconds 10
$tray = Wait-ForTopWindow -Title 'LwfTrayWindow' -TimeoutSeconds 30
if ($null -ne $tray) { OpenSettingsViaTray -Hwnd $tray.Hwnd }
Start-Sleep -Seconds 4
$w = Wait-ForTopWindow -Title 'Local Wisper Flow' -TimeoutSeconds 30
if ($null -eq $w) { throw 'full window never appeared' }
Set-WindowClientSize -Hwnd $w.Hwnd -ClientWidth 960 -ClientHeight 640
Start-Sleep -Seconds 2
Capture-WindowToFile -Hwnd $w.Hwnd -Path 'docs\agent-ops\20261109-redesign-v2-t13a\t13a_general_960x640.png' -Method Screen
Set-WindowClientSize -Hwnd $w.Hwnd -ClientWidth 760 -ClientHeight 520
Start-Sleep -Seconds 2
Capture-WindowToFile -Hwnd $w.Hwnd -Path 'docs\agent-ops\20261109-redesign-v2-t13a\t13a_general_760x520.png' -Method Screen
Write-Output 'captures done'
