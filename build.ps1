$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmake = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$buildDir = Join-Path $root "build"

if(!(Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
if(!(Test-Path $cmake)) { throw "cmake not found: $cmake" }

$orbbecSdkDir = $env:ORBBEC_SDK_DIR
if([string]::IsNullOrWhiteSpace($orbbecSdkDir)) {
  $orbbecSdkDir = Join-Path $root "..\\legacy_20260420_pre_rebuild\\_vendor\\OrbbecSDK"
}
if(!(Test-Path (Join-Path $orbbecSdkDir "include\\libobsensor\\ObSensor.hpp"))) {
  throw "Orbbec SDK headers not found. ORBBEC_SDK_DIR=$orbbecSdkDir"
}

$running = Get-Process -Name "stm_femto_bolt_viewer_ver1","tsm_femto_bolt_usb_viewer_ver1","femtobolt_rebuild_v2" -ErrorAction SilentlyContinue
if($running) {
  Write-Host "Stopping running viewer process before build..." -ForegroundColor Yellow
  $running | Stop-Process -Force
  Start-Sleep -Milliseconds 300
}

if(Test-Path $buildDir) {
  Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Path $buildDir | Out-Null

$cmd = "call `"$vcvars`" && `"$cmake`" -S `"$root`" -B `"$buildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DORBBEC_SDK_DIR=`"$orbbecSdkDir`" && `"$cmake`" --build `"$buildDir`" --config Release"
cmd /c "$cmd"
if($LASTEXITCODE -ne 0) {
  throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Build completed." -ForegroundColor Green
Write-Host "Run: $buildDir\\stm_femto_bolt_viewer_ver1.exe"
