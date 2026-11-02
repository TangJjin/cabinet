param([string]$Python='python')
$ErrorActionPreference='Stop'
$cabRoot=Split-Path -Parent $PSScriptRoot
Push-Location $cabRoot
try {
    & cmake -S tests/ui -B build/ui -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE -ne 0){throw 'CMake configure failed'}
    & cmake --build build/ui -j 6
    if($LASTEXITCODE -ne 0){throw 'UI preview build failed'}
    Push-Location build/ui
    try { & ./cabinet_ui_preview.exe; if($LASTEXITCODE -ne 0){throw 'UI callback/render tests failed'} } finally { Pop-Location }
    & $Python tools/render_preview.py
    if($LASTEXITCODE -ne 0){throw 'PNG export failed (requires Pillow)'}
} finally { Pop-Location }
