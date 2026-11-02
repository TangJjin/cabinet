param([string]$Gcc='gcc')
$ErrorActionPreference='Stop'
$cabRoot=Split-Path -Parent $PSScriptRoot
Push-Location $cabRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & $Gcc -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -ICommon/Inc tests/test_core.c Common/Src/cabinet_core.c Common/Src/cabinet_modbus.c -o build/test_core.exe
    if($LASTEXITCODE -ne 0){throw 'Core test build failed'}
    & build/test_core.exe
    if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
    & $Gcc -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -ICommon/Inc tests/test_boot.c Common/Src/cabinet_core.c Common/Src/cabinet_boot.c -o build/test_boot.exe
    if($LASTEXITCODE -ne 0){throw 'Boot test build failed'}
    & build/test_boot.exe
    if($LASTEXITCODE -ne 0){throw 'Boot tests failed'}
    & $Gcc -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -Itests/hal -IAPP/QSPI -ICommon/Inc tests/test_qspi.c APP/QSPI/bsp_qspi.c Common/Src/cabinet_core.c Common/Src/cabinet_boot.c -o build/test_qspi.exe
    if($LASTEXITCODE -ne 0){throw 'QSPI BSP test build failed'}
    & build/test_qspi.exe
    if($LASTEXITCODE -ne 0){throw 'QSPI BSP tests failed'}
} finally { Pop-Location }
