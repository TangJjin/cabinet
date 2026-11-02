param([string]$Keil='D:\Keil_v5\UV4\UV4.exe', [switch]$Rebuild)
$ErrorActionPreference='Stop'
$cabRoot=Split-Path -Parent $PSScriptRoot
$cabProject=Join-Path $cabRoot 'MDK-ARM\MIPIProject.uvprojx'
$cabBuild=Join-Path $cabRoot 'build'
New-Item -ItemType Directory -Path $cabBuild -Force | Out-Null
$cabFlag=if($Rebuild){'-r'}else{'-b'}
foreach($cabTarget in @('Cabinet_Bootloader','Cabinet_CM4_Gate','MIPIProject_CM4','MIPIProject_CM7')) {
    $cabLog=Join-Path $cabBuild ($cabTarget+'.log')
    $cabArgs=@($cabFlag,('"'+$cabProject+'"'),'-t',$cabTarget,'-o',('"'+$cabLog+'"'))
    $cabProcess=Start-Process -FilePath $Keil -ArgumentList $cabArgs -WindowStyle Hidden -Wait -PassThru
    if(-not(Test-Path -LiteralPath $cabLog)) { throw "Keil did not produce $cabLog" }
    $cabText=Get-Content -LiteralPath $cabLog -Raw
    Write-Output ($cabText -split '\r?\n' | Select-Object -Last 8)
    if($cabText -notmatch '0 Error\(s\)' -or $cabText -match 'Target not created') { throw "Build failed: $cabTarget; see $cabLog" }
    $cabFromElf=Join-Path (Split-Path (Split-Path $Keil -Parent) -Parent) 'ARM\ARMCLANG\bin\fromelf.exe'
    $cabImage=Join-Path $cabRoot ("MDK-ARM\$cabTarget\$cabTarget.axf")
    & $cabFromElf --bin --output (Join-Path $cabBuild ($cabTarget+'.bin')) $cabImage
    if($LASTEXITCODE -ne 0) { throw "fromelf failed: $cabTarget" }
    Copy-Item -LiteralPath (Join-Path $cabRoot "MDK-ARM\$cabTarget\$cabTarget.hex") -Destination $cabBuild -Force
}
Write-Output 'All four targets built. No firmware was downloaded to hardware.'
