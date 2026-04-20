# 自動リリース運用ルール

このプロジェクトでは、以下を標準運用とする。

## 1. バージョン管理
- `VERSION` ファイルを唯一のバージョン定義とする。
- 例: `1.0.0`

## 2. 配布物
- 配布形式は ZIP のみ。
- ファイル名は以下:
  - `STM-Femto-Bolt-Viewer-ver1_v<VERSION>_<SUFFIX>_win64.zip`

## 3. GitHub Actions による自動公開
- `main` への push ごとに自動ビルドし、`Auto Build (main)` リリースへ更新公開する。
- `v*` タグ push 時は、正式リリースとして同名タグで公開する。

## 4. 配布ZIPの中身
- `stm_femto_bolt_viewer_ver1.exe`
- 実行に必要な Orbbec ランタイム DLL
- `README.txt`

## 5. 例外
- GitHub-hosted runner では Orbbec SDK を扱えない場合がある。
- その場合は self-hosted Windows runner を使うこと。
