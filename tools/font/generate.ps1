param([Parameter(Mandatory=$true)][string]$Font)
$ErrorActionPreference='Stop'
$cabRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$cabSource=Join-Path $cabRoot 'APP\UI\bsp_cabinet_ui.c'
$cabText=[IO.File]::ReadAllText($cabSource)
$cabChars=($cabText.ToCharArray() | Where-Object { [int]$_ -gt 127 } | Sort-Object -Unique) -join ''
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'symbols.txt'),$cabChars,[Text.UTF8Encoding]::new($false))
$cabConv=Join-Path $PSScriptRoot 'node_modules\lv_font_conv\lv_font_conv.js'
& node $cabConv --font $Font --size 24 --bpp 4 --format lvgl --no-compress --range 0x20-0x7E --symbols $cabChars --lv-font-name cabinet_font_24 --lv-fallback lv_font_montserrat_14 --output (Join-Path $cabRoot 'APP\UI\cabinet_font_24.c')
if($LASTEXITCODE -ne 0){throw 'Font conversion failed'}
