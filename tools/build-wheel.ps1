param(
    [string]$IdfPath = 'C:\esp\v6.1\esp-idf',
    [string]$IdfToolsPath = 'C:\Espressif'
)
$ErrorActionPreference = 'Stop'
$env:IDF_TOOLS_PATH = $IdfToolsPath
$env:PYTHONUTF8 = '1'
$env:PATH = (Join-Path $IdfToolsPath 'python_env\idf6.1_py3.14_env\Scripts') + ';' + $env:PATH
. (Join-Path $IdfPath 'export.ps1')
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF activation failed' }
$projectRoot = Join-Path $PSScriptRoot '..\firmware\wheel'
& python (Join-Path $IdfPath 'tools\idf.py') -C $projectRoot build
if ($LASTEXITCODE -ne 0) { throw 'Wheel build failed' }
