$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [System.IO.Path]::GetFullPath((Join-Path $scriptDir ".."))
$vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$bundledCmake = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$buildDir = Join-Path $root "build"

if(!(Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if($cmakeCmd) {
  $cmake = $cmakeCmd.Source
} elseif(Test-Path $bundledCmake) {
  $cmake = $bundledCmake
} else {
  throw "cmake not found on PATH or at $bundledCmake"
}

$orbbecSdkDir = $env:ORBBEC_SDK_DIR
if([string]::IsNullOrWhiteSpace($orbbecSdkDir)) {
  $orbbecSdkDir = "C:\Program Files\OrbbecSDK 2.7.6"
}
if(!(Test-Path (Join-Path $orbbecSdkDir "include\\libobsensor\\ObSensor.hpp"))) {
  throw "Orbbec SDK headers not found. ORBBEC_SDK_DIR=$orbbecSdkDir"
}

$running = Get-Process -Name "stm_femto_bolt_viewer" -ErrorAction SilentlyContinue
if($running) {
  Write-Host "Stopping running viewer process before build..." -ForegroundColor Yellow
  $running | Stop-Process -Force
  Start-Sleep -Milliseconds 300
}

if(Test-Path $buildDir) {
  Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Path $buildDir | Out-Null

$vcpkgRoot = $env:VCPKG_ROOT
if([string]::IsNullOrWhiteSpace($vcpkgRoot)) {
  $localVcpkgRoot = Join-Path $root ".devenv\sdks\vcpkg"
  if(Test-Path $localVcpkgRoot) {
    $vcpkgRoot = $localVcpkgRoot
  }
}

$toolchainArg = ""
$tripletArg = ""
if(-not [string]::IsNullOrWhiteSpace($vcpkgRoot)) {
  $vcpkgToolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"
  if(!(Test-Path $vcpkgToolchain)) {
    throw "vcpkg toolchain not found: $vcpkgToolchain"
  }
  $triplet = $env:VCPKG_TARGET_TRIPLET
  if([string]::IsNullOrWhiteSpace($triplet)) {
    $triplet = "x64-windows"
  }
  $toolchainArg = " -DCMAKE_TOOLCHAIN_FILE=`"$vcpkgToolchain`" -DVCPKG_MANIFEST_MODE=ON"
  $tripletArg = " -DVCPKG_TARGET_TRIPLET=`"$triplet`""
  Write-Host "Using vcpkg: VCPKG_ROOT=$vcpkgRoot, triplet=$triplet" -ForegroundColor Cyan
} else {
  Write-Host "VCPKG_ROOT is not set; proceeding without vcpkg toolchain." -ForegroundColor Yellow
}

$configureCmd = "`"$cmake`" -S `"$root`" -B `"$buildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DORBBEC_SDK_DIR=`"$orbbecSdkDir`" -DCMAKE_PREFIX_PATH=`"$orbbecSdkDir;$orbbecSdkDir\lib`"$toolchainArg$tripletArg"
$cmd = "call `"$vcvars`" && $configureCmd && `"$cmake`" --build `"$buildDir`" --config Release"
cmd /c "$cmd"
if($LASTEXITCODE -ne 0) {
  throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Build completed." -ForegroundColor Green
Write-Host "Run: $buildDir\\stm_femto_bolt_viewer.exe"
