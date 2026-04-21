param(
  [string]$Version,
  [string]$BuildDir = "build",
  [string]$OutputDir = "release",
  [string]$Suffix = ""
)

$ErrorActionPreference = "Stop"

if([string]::IsNullOrWhiteSpace($Version)) {
  if(Test-Path "VERSION") {
    $Version = (Get-Content "VERSION" -Raw).Trim()
  }
}
if([string]::IsNullOrWhiteSpace($Version)) {
  throw "Version is empty. Pass -Version or create VERSION file."
}

$buildPath = Resolve-Path $BuildDir
if(!(Test-Path $buildPath)) {
  throw "Build directory not found: $BuildDir"
}

$exe = Join-Path $buildPath "stm_femto_bolt_viewer.exe"
if(!(Test-Path $exe)) {
  throw "Executable not found: $exe"
}

$requiredDlls = @(
  "OrbbecSDK.dll"
)
$optionalDlls = @(
  "ob_usb.dll",
  "depthengine_2_0.dll",
  "live555.dll"
)

$sdkDir = $env:ORBBEC_SDK_DIR
if([string]::IsNullOrWhiteSpace($sdkDir)) {
  $sdkDir = "C:\Program Files\OrbbecSDK 2.7.6"
}
$sdkBinDir = Join-Path $sdkDir "bin"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$stageRoot = Join-Path $OutputDir "stage"
if(Test-Path $stageRoot) {
  Remove-Item -Recurse -Force $stageRoot
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

Copy-Item -LiteralPath $exe -Destination $stageRoot
Get-ChildItem -Path $buildPath -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination $stageRoot -Force
}
foreach($dll in $requiredDlls) {
  $src = Join-Path $buildPath $dll
  if(Test-Path $src) {
    Copy-Item -LiteralPath $src -Destination $stageRoot
  } elseif(Test-Path (Join-Path $sdkBinDir $dll)) {
    Copy-Item -LiteralPath (Join-Path $sdkBinDir $dll) -Destination $stageRoot
  } else {
    Write-Warning "Missing runtime DLL: $dll"
  }
}
foreach($dll in $optionalDlls) {
  $src = Join-Path $buildPath $dll
  if(Test-Path $src) {
    Copy-Item -LiteralPath $src -Destination $stageRoot
  } elseif(Test-Path (Join-Path $sdkBinDir $dll)) {
    Copy-Item -LiteralPath (Join-Path $sdkBinDir $dll) -Destination $stageRoot
  }
}

$buildExtDir = Join-Path $buildPath "extensions"
if(Test-Path $buildExtDir) {
  Copy-Item -Recurse -Force -LiteralPath $buildExtDir -Destination (Join-Path $stageRoot "extensions")
} elseif(Test-Path (Join-Path $sdkBinDir "extensions")) {
  Copy-Item -Recurse -Force -LiteralPath (Join-Path $sdkBinDir "extensions") -Destination (Join-Path $stageRoot "extensions")
}

$readmeTxt = @"
STM Femto Bolt Viewer

1) Femto Bolt を接続
2) stm_femto_bolt_viewer.exe を実行

操作:
- q / ESC: 終了
- 左ドラッグ: 回転
- 右ドラッグ: 平行移動
- ホイール: ズーム
- r: 視点リセット
- m: GPU MESH -> GPU POINT -> CPU POINT
"@
Set-Content -Path (Join-Path $stageRoot "README.txt") -Value $readmeTxt -Encoding UTF8

$suffixPart = ""
if(-not [string]::IsNullOrWhiteSpace($Suffix)) {
  $suffixPart = "_$Suffix"
}
$zipName = "STM-Femto-Bolt-Viewer_v$Version${suffixPart}_win64.zip"
$zipPath = Join-Path $OutputDir $zipName
if(Test-Path $zipPath) {
  Remove-Item -Force $zipPath
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -Force

Write-Output "PACKAGE_PATH=$zipPath"
