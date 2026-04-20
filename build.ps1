$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmake = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$buildDir = Join-Path $root "build"

if(!(Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
if(!(Test-Path $cmake)) { throw "cmake not found: $cmake" }

$running = Get-Process -Name "stm_femto_bolt_viewer_ver1","tsm_femto_bolt_usb_viewer_ver1","femtobolt_rebuild_v2" -ErrorAction SilentlyContinue
if($running) {
  Write-Host "Stopping running viewer process before build..." -ForegroundColor Yellow
  $running | Stop-Process -Force
  Start-Sleep -Milliseconds 300
}

$cmd = "call `"$vcvars`" && `"$cmake`" -S `"$root`" -B `"$buildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release && `"$cmake`" --build `"$buildDir`" --config Release"
cmd /c "$cmd"

Write-Host ""
Write-Host "Build completed." -ForegroundColor Green
Write-Host "Run: $buildDir\\stm_femto_bolt_viewer_ver1.exe"
