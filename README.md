# STM Femto Bolt Viewer

Sports Time Machine で Orbbec Femto Bolt を使うための、C++製の簡易ビューアーです。  
RGB / Depth / XYZRGB（PointCloud）を1画面で確認し、「現場で安定して動くこと」を最優先にしています。

A C++ real-time viewer for Orbbec Femto Bolt cameras, using OpenGL 2.1 and Dear ImGui.  
Displays RGB, Depth, and XYZRGB point cloud simultaneously in one window.

---

## 現場で使うだけの方へ / Quick Start (No Build Required)

**ビルド不要。zip をダウンロードして実行するだけです。**

1. [Releases ページ](https://github.com/p0lyg0n/STM-Femt-Bolt-Viwer/releases) を開く
2. 最新の zip をダウンロード
   - `Auto Build (main)` … 開発中の最新版（プレリリース）
   - `v1.0.0` などのタグ付き … 正式版
3. 解凍して `stm_femto_bolt_viewer.exe` を実行

zip には実行ファイルと Orbbec SDK ランタイム DLL がすべて同梱されています。**別途 Orbbec SDK のインストールは不要**です。

### ハードウェア要件

| 項目 | 要件 |
| --- | --- |
| カメラ | Orbbec Femto Bolt × 1〜2台 |
| USB | USB 3.0（複数台時はカメラごとに別コントローラー推奨） |
| GPU | OpenGL 2.1 以上（NVIDIA / AMD / Intel いずれも可） |
| RAM | 8 GB 以上 |
| CPU | 4コア以上推奨 |
| OS | Windows 10 / 11 64-bit |

### 操作

- `q` / `ESC`: 終了
- 左ドラッグ: 視点回転（yaw / pitch）
- 右ドラッグ: 平行移動（pan）
- マウスホイール: ズーム
- `r`: 視点を初期化
- `m`: 表示モードを循環切替 `GPU MESH` → `GPU POINT` → `CPU POINT`

---

## 主な機能

- 1ウィンドウ内で `RGB` / `DEPTH` / `XYZRGB` を同時表示
- `XYZRGB` の表示モード切替（`GPU MESH` / `GPU POINT` / `CPU POINT`）
- 点群ビューでマウス操作（回転・パン・ズーム）
- 床グリッドと XYZ 軸のガイド表示（GPU/CPU 両モード）
- 各ペインに解像度・FPS・描画方式を表示
- OpenGL 初期化失敗時の CPU フォールバック（アプリ継続）

### FPS 確認

コンソールに1秒ごとに FPS ログを出力します。

```text
[FPS] color=30.0 depth=30.0 point=30.0 drawPts=2140
```

---

## 開発者向け / For Developers

自分でソースからビルドする方はこちら。現場で使うだけの方は上のセクションで十分です。

### ソフトウェア要件

| 項目 | バージョン |
| --- | --- |
| Orbbec SDK | 2.7.6 |
| Visual Studio 2022 Build Tools | 最新 |
| CMake | 3.20 以上 |
| インターネット接続 | 初回ビルド時に Dear ImGui v1.91.6 を GitHub からダウンロード |

### ビルド

```powershell
.\build.ps1
```

初回ビルド時、CMake が自動的に Dear ImGui v1.91.6 を `_imgui_cache/` へダウンロードします。2回目以降はネット接続不要です。

### ビルド後の実行

```powershell
.\build\stm_femto_bolt_viewer.exe
```

### Orbbec SDK の場所

`build.ps1` は以下の順で SDK を探します：

1. 環境変数 `ORBBEC_SDK_DIR` が設定されていればそのパス
2. 未設定時のデフォルト: `..\legacy_20260420_pre_rebuild\_vendor\OrbbecSDK`

開発者が手元でビルドするときは、環境変数で SDK インストール先を指定するのが確実です：

```powershell
$env:ORBBEC_SDK_DIR = "C:\Program Files\OrbbecSDK 2.7.6"
.\build.ps1
```

### ローカルで配布 zip を作る

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_release.ps1 `
  -Version (Get-Content .\VERSION -Raw).Trim() `
  -BuildDir .\build `
  -OutputDir .\release
```

出力ファイル名: `STM-Femto-Bolt-Viewer_v<VERSION>_<SHA8>_win64.zip`

---

## リリース自動公開の仕組み

- **`main` ブランチに push** されるたびに GitHub Actions が自動ビルドし、`Auto Build (main)` プレリリースを上書き公開
- **`v*` タグ（例: `v1.0.0`）を push** した場合は、同名の正式リリースとして公開
- CI 実行環境は **self-hosted Windows runner**（Orbbec SDK のインストールが必要なため GitHub-hosted runner は使えません）
- ランナー側は `C:\Program Files\OrbbecSDK 2.7.6` に SDK がある前提
- 運用詳細: [docs/RELEASE_RULE_JA.md](docs/RELEASE_RULE_JA.md)

---

## 開発方針

### 表示安定ルール

- 表示の安定を最優先とする
- FPS 低下やカクつきを招く処理は、できるだけメイン描画ループから外す
- USB 列挙、切断検知、トポロジー確認などの重い処理は別スレッド化が基本
- UI / 描画を止める可能性がある同期処理は避け、必要ならキャッシュや遅延更新を使う
- 迷ったら「止まらない実装」を優先する

### UI 言語ルール

- UI は **日本語を基準** に作り続ける
- 文言追加・整理では **日本語・韓国語・英語** の3言語展開を前提に設計する
- 画面文言、ログ、ツールチップは3言語に展開しやすい形を維持する
