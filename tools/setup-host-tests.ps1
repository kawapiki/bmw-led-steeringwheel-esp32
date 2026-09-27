param([string]$Python = "python", [string]$Output = ".test-build/host-clean")
$ErrorActionPreference = "Stop"
& $Python -m pip install --target "$Output/deps" ziglang==0.14.1 cryptography==48.0.1
if ($LASTEXITCODE -ne 0) { throw "Host dependency setup failed" }
$env:HOST_CC = (Resolve-Path "$Output/deps/ziglang/zig.exe").Path
$env:HOST_TEST_OUT = [IO.Path]::GetFullPath($Output)
$env:PYTHONPATH = (Resolve-Path "$Output/deps").Path
& $Python tests/host/test_core.py
if ($LASTEXITCODE -ne 0) { throw "Core tests failed" }
& $Python tests/host/test_release.py
if ($LASTEXITCODE -ne 0) { throw "Release tests failed" }
