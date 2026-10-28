# Crea (o actualiza) el acceso directo a UltimateTracker en el escritorio y en el menú Inicio.
# Uso: crear-acceso-directo.ps1 -Exe "ruta\a\UltimateTracker.exe"
param([Parameter(Mandatory = $true)][string]$Exe)
if (-not (Test-Path $Exe)) { Write-Host "No encuentro $Exe (compilá primero con recompilar.bat)." -ForegroundColor Red; exit 1 }
$ws = New-Object -ComObject WScript.Shell
$places = @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('Programs')) ''))
foreach ($dir in $places) {
    $lnk = $ws.CreateShortcut((Join-Path $dir 'UltimateTracker.lnk'))
    $lnk.TargetPath = $Exe
    $lnk.WorkingDirectory = Split-Path -Parent $Exe
    $lnk.IconLocation = "$Exe,0"
    $lnk.Description = 'Pomodoros y horas de estudio'
    $lnk.Save()
    Write-Host "Acceso directo: $(Join-Path $dir 'UltimateTracker.lnk')"
}
