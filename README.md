# STM Femto Bolt Viewer

Sports Time Machine で Orbbec Femto Bolt を使うための、C++製の簡易ビューアーです。  
RGB / Depth / XYZRGB（PointCloud）を1画面で確認し、まずは「現場で安定して動くこと」を最優先にしています。

## 主な機能
- 1ウィンドウ内で `RGB` / `DEPTH` / `XYZRGB` を同時表示
- `XYZRGB` の表示モード切替
  - `GPU MESH`
  - `GPU POINT`
  - `CPU POINT`
- 点群ビューでマウス操作（回転・パン・ズーム）
- 床グリッドと XYZ 軸のガイド表示（GPU/CPU 両モード）
- 各ペインに解像度・FPS・描画方式を表示
- OpenGL 初期化失敗時の CPU フォールバック（アプリ継続）

## 想定環境
- Windows 10 / 11
- Orbbec SDK（`legacy_20260420_pre_rebuild/_vendor/OrbbecSDK` を参照）
- Visual Studio Build Tools 2022（MSVC / NMake / CMake）
- GPU描画を使う場合は OpenGL 対応ドライバ

## ビルド
PowerShell で以下を実行します。

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## 実行
```powershell
.\build\stm_femto_bolt_viewer_ver1.exe
```

## 操作
- `q` または `ESC`: 終了
- 左ドラッグ（右ペイン上）: 視点回転（yaw / pitch）
- 右ドラッグ（右ペイン上）: 平行移動（pan）
- マウスホイール（右ペイン上）: ズーム
- `r`: 視点を初期化
- `m`: 表示モードを循環切替  
`GPU MESH -> GPU POINT -> CPU POINT`

## FPS確認
コンソールに1秒ごとにFPSログを出力します。

```text
[FPS] color=30.0 depth=30.0 point=30.0 drawPts=2140
```

## 現在の方針
- まずは **1台の Femto Bolt で XYZRGB を安定表示** することを優先
- その上で Sports Time Machine 連携に必要な機能を段階追加
- 複雑化より、再現性とトラブル切り分けのしやすさを重視

## 配布（バージョン付きZIP）
- バージョンは `VERSION` ファイルで管理します。
- 配布ZIP名の形式:
  - `STM-Femto-Bolt-Viewer-ver1_v<VERSION>_<SHA8>_win64.zip`
- ローカルで生成する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_release.ps1 -Version (Get-Content .\VERSION -Raw).Trim() -BuildDir .\build -OutputDir .\release
```

## GitHub自動公開ルール
- `main` へ push するたびに、GitHub Actions が自動ビルドして ZIP を `Auto Build (main)` リリースへ更新公開します。
- `v*` タグ（例: `v1.0.0`）を push した場合は、正式リリースとして公開します。
- 運用ルール詳細:
  - `docs/RELEASE_RULE_JA.md`

## CI実行環境について
- このプロジェクトは Orbbec SDK に依存するため、Actions は **self-hosted Windows runner** 前提です。
- ワークフロー既定値:
  - `ORBBEC_SDK_DIR = C:\Program Files\OrbbecSDK 2.7.6`
