# Instala lo necesario para compilar UltimateTracker en Windows, sin cuenta de Qt:
# Python (si falta, con winget, solo para tu usuario), aqtinstall, Qt 6.8 MinGW 64-bit
# con Qt Multimedia, y MinGW, CMake y Ninja en C:\Qt. No compila: para eso está recompilar.bat.
$ErrorActionPreference = 'Stop'
$QtDir = 'C:\Qt'

function Test-Python([string]$Exe) {
    if (-not $Exe -or $Exe -like '*WindowsApps*' -or $Exe -like '*msys*' -or $Exe -like '*cygwin*') { return $false }
    & $Exe -c "import pip" 2>$null
    return ($LASTEXITCODE -eq 0)
}
function Find-Python {
    $c = @()
    $py = Get-Command 'py' -ErrorAction SilentlyContinue
    if ($py) { $c += (& $py.Source -3 -c "import sys; print(sys.executable)" 2>$null | Out-String).Trim() }
    $c += (Get-Command 'python' -All -ErrorAction SilentlyContinue | ForEach-Object { $_.Source })
    $c += (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
    foreach ($x in $c) { if (Test-Python $x) { return $x } }
    return $null
}

$py = Find-Python
if (-not $py) {
    winget install -e --id Python.Python.3.12 --scope user --silent --accept-package-agreements --accept-source-agreements
    $py = Find-Python
    if (-not $py) { throw 'No encuentro Python después de instalarlo.' }
}
& $py -m pip install --user --upgrade --disable-pip-version-check aqtinstall
$ver = (& $py -m aqt list-qt windows desktop --spec 6.8 --latest-version | Out-String).Trim()
if (-not (Test-Path "$QtDir\$ver\mingw_64\bin\Qt6Multimedia.dll")) {
    & $py -m aqt install-qt windows desktop $ver win64_mingw -m qtmultimedia -O $QtDir
}
$tools = @(@('tools_mingw1310', 'qt.tools.win64_mingw1310', 'Tools\mingw1310_64\bin\g++.exe'),
           @('tools_cmake', 'qt.tools.cmake', 'Tools\CMake_64\bin\cmake.exe'),
           @('tools_ninja', 'qt.tools.ninja', 'Tools\Ninja\ninja.exe'))
foreach ($t in $tools) {
    if (-not (Test-Path (Join-Path $QtDir $t[2]))) { & $py -m aqt install-tool windows desktop $t[0] $t[1] -O $QtDir }
}
Write-Host "`nListo: Qt $ver en $QtDir. Ahora corré recompilar.bat." -ForegroundColor Green
