param([string]$IdfPath='C:\esp\v6.1\esp-idf',[string]$IdfToolsPath='C:\Espressif')
$ErrorActionPreference='Stop'
$env:IDF_TOOLS_PATH=$IdfToolsPath
$env:PYTHONUTF8='1'
$env:PATH=(Join-Path $IdfToolsPath 'python_env\idf6.1_py3.14_env\Scripts')+';'+$env:PATH
. (Join-Path $IdfPath 'export.ps1')
& python (Join-Path $IdfPath 'tools\idf.py') -C (Join-Path $PSScriptRoot '..\firmware\gateway') build
if($LASTEXITCODE -ne 0){throw 'Gateway build failed'}
