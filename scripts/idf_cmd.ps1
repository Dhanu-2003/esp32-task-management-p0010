# Helper: run ESP-IDF through the Nova wrapper with this laptop's working env.
# The stock wrapper (C:/nova/agent/idf.bat) needs two fixes on this machine:
#   1. python.exe must be on PATH (export.bat checks it).
#   2. IDF_TOOLS_PATH must not contain a space: the xtensa-esp32-elf-gcc shim
#      panics ("Failed to get path name. Error code: 5") when its path contains
#      one (e.g. C:\Users\Dhanu S\...). C:\nova\.home\espressif is a junction
#      to the real tools dir, same files, no spaces.
# Usage (from firmware/):  powershell ..\scripts\idf_cmd.ps1 build
$env:PATH = "C:\Users\Dhanu S\AppData\Local\Programs\Python\Python311;C:\Program Files\Git\cmd;" + $env:PATH
$env:IDF_TOOLS_PATH = "C:\nova\.home\espressif"
& C:/nova/agent/idf.bat @args
exit $LASTEXITCODE
