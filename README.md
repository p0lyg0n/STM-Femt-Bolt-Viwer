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
| カメラ | Orbbec Femto Bolt × 1〜4台 |
| USB | USB 3.0（複数台時はカメラごとに別コントローラー推奨） |
| GPU | OpenGL 2.1 以上（NVIDIA / AMD / Intel いずれも可） |
| RAM | 8 GB 以上 |
| CPU | 4コア以上推奨 |
| OS | Windows 10 / 11 64-bit |

カメラ台数に応じて自動でレイアウトが変わります（1〜2台: 縦分割、3〜4台: 2×2 グリッド）。

### 操作

#### キーボード / マウス

- `q` / `ESC`: 終了
- 左ドラッグ: 視点回転（yaw / pitch）
- 右ドラッグ: 平行移動（pan）
- マウスホイール: ズーム
- `r`: 視点を初期化
- `m`: 表示モードを循環切替 `GPU MESH` → `GPU POINT` → `CPU POINT`

#### サイドバー（左側 UI）

- **言語切替**: 上部の `日本語 / English / 한국어` で UI 言語を切替
- **VIEW**
  - 表示モードの循環切替（GPU MESH / GPU POINT / CPU POINT）
  - 視点リセット
- **STREAM**
  - Color 解像度プリセット: `640×480 → 1280×720 → 1920×1080`
  - Depth 解像度プリセット: `320×288 → 640×576 → 512×512 → 1024×1024`
  - FPS プリセット: `5 → 15 → 30`
  - いずれも全カメラに同時適用
- **USB TOPOLOGY**: 各カメラがどの USB コントローラーに接続されているか。赤字は同一コントローラーを複数台で共有（帯域不足の原因）
- **GPU**: 描画に使われている OpenGL 実装（Renderer / Vendor / Version）
- **RECOVERY**
  - `USBをリセット`: カメラが認識されない時に USB ホストを一時的に無効→有効にする
  - `アプリを再起動`: USB リセット後に押してセッションを作り直す

#### ホバーヘルプ

サイドバーのボタンや右側のデバイス情報（Device / USB / IMU / FPS / TEMP / RES / PTS）、3つのペイン（RGB / DEPTH / POINT）の上にマウスを置くと、日本語・英語・韓国語で用途と意味を表示します。

### USB 抜き差しの挙動

- **同じ USB ポートに挿し直し**: 数秒以内に自動で LIVE へ復帰します
- **別の USB ポートに挿し直し**: 自動復帰しません (SDK / Windows USB ドライバの制約)。ペインに `戻らない場合は [アプリを再起動] で復旧してください` と表示されるので、サイドバーの **RECOVERY** セクションにある **USBをリセット** → **アプリを再起動** を順に押してください
- **カメラが反応しなくなった**: 同じく **USBをリセット** → **アプリを再起動** で復旧

---

## 主な機能

- 1ウィンドウ内で `RGB` / `DEPTH` / `XYZRGB` を同時表示
- `XYZRGB` の表示モード切替（`GPU MESH` / `GPU POINT` / `CPU POINT`）
- 点群ビューでマウス操作（回転・パン・ズーム）
- 床グリッドと XYZ 軸のガイド表示（GPU/CPU 両モード）
- 各ペインに解像度・FPS・描画方式を表示
- **複数カメラ対応（最大 4 台、自動レイアウト）**
- **UI 3 言語切替（日本語 / English / 한국어）+ ホバーヘルプ**
- **設定の永続化**: 言語・表示モード・ストリーム解像度 / FPS を `settings.ini` に保存し次回起動で復元
- **USB トポロジー表示**: どのカメラがどのコントローラーにいるか、共有していないかを一目で確認
- **IMU / 温度モニタ**: 各カメラの加速度センサー値と内部温度 (CPU / IR / LDM) を表示
- **USB ホスト復旧**: 一時的にコントローラーを無効→有効にしてカメラ未検出状態から立ち直る
- **自動再接続**: カメラが一時的に切れても UI は止まらず、再接続を待つ
- **OpenGL 初期化失敗時の CPU フォールバック**（アプリ継続）

### Depth 解像度の意味

Femto Bolt の Depth センサーは Azure Kinect 互換で 4 つのモードを持ちます。用途で選びます。

| 解像度 | モード | 画角 | 最大 FPS | 実用レンジ | 向いている用途 |
| --- | --- | --- | --- | --- | --- |
| 320×288 | NFOV 2×2 Binned | 狭 75°×65° | 30 | 0.5 – 5.46m | 遠距離・省帯域 |
| 640×576 | NFOV Unbinned | 狭 75°×65° | 30 | 0.5 – 3.86m | **標準**。最も用途が広い |
| 512×512 | WFOV 2×2 Binned | 広 120°×120° | 30 | 0.25 – 2.88m | 広角・至近を 30fps で |
| 1024×1024 | WFOV Unbinned | 広 120°×120° | **15** | 0.25 – 2.21m | 広角・至近を高精細で |

> **1024×1024 は 15 FPS のみ**。選択すると自動で FPS を 15 に変更し、その旨をモーダルで通知します。

### 設定ファイル

起動時・終了時に実行ファイルと同じ場所の `settings.ini` を読み書きします。

```ini
lang=ja
point_mode=mesh
depth_w=640
depth_h=576
color_w=1280
color_h=720
fps=30
```

欠けたキーやファイルそのものが無い場合はデフォルト値で起動します。

### FPS / 状態ログ

コンソールに1秒ごとに FPS ログをカメラごとに出力します。

```text
[Device 0] color=30.0 depth=30.0 point=30.0 drawPts=2140 mode=GPU MESH
[Device 1] color=30.0 depth=30.0 point=30.0 drawPts=1832 mode=GPU MESH
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

### devenv + direnv での開発（推奨）

`devenv` と `direnv` を使うと、リポジトリに入るだけで開発コマンドを使える状態にできます。
`dev-build` は macOS / Windows / Linux で CMake configure/build を実行します。
`dev-run` は macOS / Windows / Linux で同じ入口を使えます（Linux では GUI 用の表示サーバが必要です）。
`dev-package` は引き続き Windows 向け入口です。

PowerShell で使う場合は、先に `direnv` フックを有効化してください（未設定だと `direnv allow` だけでは `dev-*` コマンドが読み込まれません）。

```powershell
Invoke-Expression "$(direnv hook pwsh)"
```

1. `devenv` と `direnv` をインストール
2. `.env.local` を作成して SDK パスを設定
3. `direnv allow` を実行
4. 必要な依存を取得（`dev-download-sdks`）
5. `dev-doctor` → `dev-build` を実行

```powershell
Copy-Item .env.local.example .env.local
direnv allow
dev-download-sdks
dev-doctor
dev-build
```

#### 実行

Windows:

```powershell
dev-run
```

macOS:

```bash
dev-run
```

配布用 zip は Windows 上で `dev-package` を使って作成できます。

```powershell
dev-package
```

`direnv` フックを使わない場合でも、次のように `devenv shell -- <command>` で実行できます。

```powershell
devenv shell -- dev-doctor
devenv shell -- dev-download-sdks
devenv shell -- dev-build
devenv shell -- dev-run
devenv shell -- dev-format-cpp
devenv shell -- dev-lint-cpp
```

ローカル SDK パスは `.env.local` で上書きできます（Windows 既定値: `C:\Program Files\OrbbecSDK 2.7.6`）。
`dev-download-sdks` は `vcpkg` を `.devenv/sdks/vcpkg` にセットアップし、`vcpkg.json` に基づいて依存（現在は `glfw3`）をインストールします。`ORBBEC_SDK_URL` が設定されている場合は Orbbec SDK も `.devenv/sdks/orbbec` にダウンロードします。
macOS / Linux では `dev-build` が `.devenv/sdks/orbbec/extracted` 以下の SDK を自動検出します。

> `scripts/dev-build.ps1`（Windows）と `scripts/dev-build.sh`（macOS / Linux）は `VCPKG_ROOT`（未設定時は `.devenv/sdks/vcpkg`）を自動検出し、vcpkg toolchain を使って configure します。

### Unit Test / Coverage

Orbbec SDK や OpenGL を使う本体とは別に、設定パーサ・i18n・画素変換・USB 表示名整形だけを `stm_core` として unit test できます。
テストフレームワークは `Catch2`、runner は `CTest` です。

SDK なしで unit test だけ回す場合:

```bash
cmake -S . -B build-unit -DSTM_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build-unit
ctest --test-dir build-unit --output-on-failure
```

LLVM coverage を出す場合（Clang 系コンパイラが必要）:

```bash
cmake -S . -B build-coverage \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DSTM_BUILD_APP=OFF \
  -DBUILD_TESTING=ON \
  -DSTM_ENABLE_COVERAGE=ON
cmake --build build-coverage --target coverage
```

HTML レポートは `build-coverage/coverage/html/index.html` に出力されます。

### C++ Lint / Format

`clang-format` と `clang-tidy` は `devenv` に含まれています。初期導入では **`tests/**` と新規追加した C/C++ ファイルだけ** を対象にします。

```bash
dev-format-cpp
dev-lint-cpp
```

`dev-lint-cpp` は次をまとめて実行します。

- 新規ファイルの Google 形式コメントチェック
- 対象ファイルへの `clang-format --dry-run --Werror`
- 対象 `.cpp` への `clang-tidy`

`clang-tidy` は初期導入では範囲を絞り、`clang-analyzer` / `bugprone` 中心の bug-finding 寄りな check だけを有効にしています。ノイズを見ながら段階的に広げる前提です。

新規 C++ ファイルには次を必須にしています。

- ファイル先頭の overview コメント
- 公開 API 宣言（公開関数、class/struct/enum など）の Google 形式コメント

### フォントの前提

UI は Windows 同梱フォントを実行時に読み込みます。

| フォント | 場所 | 用途 |
| --- | --- | --- |
| `meiryo.ttc` | `C:\Windows\Fonts\meiryo.ttc` | 日本語・英語の主フォント |
| `malgun.ttf` | `C:\Windows\Fonts\malgun.ttf` | 韓国語（ハングル）を merge mode で重ねる |
| `segoeui.ttf` | `C:\Windows\Fonts\segoeui.ttf` | meiryo が無いときのラテン文字フォールバック |

Windows 10 / 11 には `meiryo.ttc` と `malgun.ttf` の両方が標準で入っているため、特に追加の作業は不要です。

### ビルド（従来手順・後方互換）

Windows:

```powershell
.\scripts\dev-build.ps1
```

macOS / Linux:

```bash
./scripts/dev-build.sh
```

初回ビルド時、CMake が自動的に Dear ImGui v1.91.6 を `_imgui_cache/` へダウンロードします。2回目以降はネット接続不要です。

### ビルド後の実行

Windows:

```powershell
.\build\stm_femto_bolt_viewer.exe
```

macOS / Linux:

```bash
./build/stm_femto_bolt_viewer
```

### Orbbec SDK の場所

Orbbec SDK 2.7.6 を公式インストーラーで **`C:\Program Files\OrbbecSDK 2.7.6\`** にインストールしておけば、そのまま `scripts/dev-build.ps1` が動きます。
macOS では公式 zip を展開したディレクトリを `ORBBEC_SDK_DIR` に指定するか、`dev-download-sdks` で `.devenv/sdks/orbbec/extracted/` に配置してください。

別のパスに SDK を置いている場合は環境変数 `ORBBEC_SDK_DIR` で上書きしてください：

```powershell
$env:ORBBEC_SDK_DIR = "D:\path\to\OrbbecSDK"
.\scripts\dev-build.ps1
```

### ローカルで配布 zip を作る

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_release.ps1 `
  -Version (Get-Content .\VERSION -Raw).Trim() `
  -BuildDir .\build `
  -OutputDir .\release
```

出力ファイル名: `STM-Femto-Bolt-Viewer_v<VERSION>_<SHA8>_win64.zip`

### ソース構成

```text
src/
  main.cpp              起動/ループ/フォントロード/終了処理
  app_settings.{h,cpp}  settings.ini 読み書き (言語・モード・ストリーム設定)
  i18n.{h,cpp}          JA/EN/KO 3言語の文字列テーブルと切替
  camera_session.{h,cpp} Orbbec パイプライン/再接続/IMU/温度
  frame_processing.{h,cpp} 疑似カラー変換/メッシュ生成
  gl_utils.{h,cpp}      OpenGL 補助
  input.{h,cpp}         キー/マウス入力と視点更新
  render.{h,cpp}        サイドバー、デバイスパネル、点群描画
  types.h               共通構造体/定数
  usb_topology.{h,cpp}  USB コントローラー列挙 (Windows)
```

---

## リリース自動公開の仕組み

- **`main` ブランチに push** されるたびに GitHub Actions が自動ビルドし、`Auto Build (main)` プレリリースを上書き公開
- **`v*` タグ（例: `v1.0.0`）を push** した場合は、同名の正式リリースとして公開
- CI 実行環境は **self-hosted Windows runner**（Orbbec SDK のインストールが必要なため GitHub-hosted runner は使えません）
- ランナー側は `C:\Program Files\OrbbecSDK 2.7.6` に SDK がある前提
- **ランナーはサービス化していません。push 前に `C:\actions-runner\run.cmd` を手動起動する必要があります** → [.claude/skills/release-self-hosted-runner/SKILL.md](.claude/skills/release-self-hosted-runner/SKILL.md)
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
- 新しい文字列は必ず `src/i18n.h` の `enum class S` に ID を追加し、`src/i18n.cpp` の 3 言語テーブルを同時に埋める
