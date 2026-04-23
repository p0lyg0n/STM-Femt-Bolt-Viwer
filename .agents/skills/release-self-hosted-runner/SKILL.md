---
name: release-self-hosted-runner
description: Use when the user is about to release / push to main / push a v* tag, or when GitHub Actions jobs are stuck in "Queued". Reminds the user to start the self-hosted runner manually — this project deliberately does NOT install the runner as a Windows service.
---

# リリース前に self-hosted runner を起動する

このリポジトリの GitHub Actions ワークフロー `.github/workflows/build-and-release.yml` は `runs-on: self-hosted` で動くため、**このマシン上の self-hosted runner プロセスが起動していないと job は Queued のまま進みません**。

サービス化はしない方針 (ユーザーが配信タイミングを明示的にコントロールするため)。手動で起動する運用です。

## いつこの skill を使うか

以下のどれかに該当する時、**push の前にランナーの起動を案内する**:

- ユーザーが `main` に push しようとしている
- `v*` タグを作成 / push しようとしている
- `VERSION` ファイルを更新した直後
- ユーザーが「Actions が進まない」「リリースされない」と言っている
- `gh run list` / Actions ページが Queued のまま

## 動作確認

以下で状態チェック（シェルによってコマンドが異なる）:

**CMD:**
```cmd
tasklist /FI "IMAGENAME eq Runner.Listener.exe" /NH
```

**PowerShell:**
```powershell
tasklist /FI "IMAGENAME eq Runner.Listener.exe" /NH
```

**Git Bash / MSYS2**（`/` がパスに解釈されるため `//` が必要）:
```bash
tasklist //FI "IMAGENAME eq Runner.Listener.exe" //NH
```

出力に `Runner.Listener.exe` があれば起動済み。無ければ未起動 (Queued の原因)。

もう一つの確認方法: GitHub の Runners 設定ページ
https://github.com/p0lyg0n/STM-Femt-Bolt-Viwer/settings/actions/runners
緑の `Idle` バッジが出ていれば OK、`Offline` なら起動が必要。

## 起動方法

**手動で 1 コマンド** (管理者権限不要):

**CMD:**
```cmd
cd C:\actions-runner && run.cmd
```

**PowerShell:**
```powershell
cd C:\actions-runner; .\run.cmd
```

**Git Bash / MSYS2:**
```bash
cd /c/actions-runner && ./run.cmd
```

ターミナルを**閉じずに放置**。Queued の job が順番に処理されます。

起動したターミナルには以下のような出力が出る:
```
√ Connected to GitHub
Current runner version: '2.333.1'
Listening for Jobs
```

## リリースの手順 (参考)

1. **ランナー起動** (上記)
2. コードを main にコミット + push → `Auto Build (main)` プレリリース zip が更新
3. (正式リリースの場合) `VERSION` を更新 + コミット + push
4. (正式リリースの場合) `git tag -a v1.0.X -m "v1.0.X"` + `git push origin v1.0.X` → タグ名の正式リリース zip が公開
5. ランナー起動ターミナルで job が順次処理されるのを確認
6. 完了後 Releases ページで zip が更新されていることを確認:
   https://github.com/p0lyg0n/STM-Femt-Bolt-Viwer/releases

## なぜサービス化しないか

- PC 起動中ずっとランナーが動いていると、意図しないタイミング (他の作業中など) に
  重い build が走って迷惑
- 配信したい時だけ明示的に起動し、終わったら閉じる運用にして
  CPU / USB / ネットワーク帯域をコントロールする方針
- Windows のサービス化は設定復旧が面倒なので、必要になった時だけ有効化する

## Codex に期待する動作

ユーザーが `git push origin main` / `git push origin v*` / タグ作成系コマンドを実行しそうなとき、
**実行の前に** この skill の内容を抜粋して案内すること。特に:

1. 「ランナー起動されていますか?」と尋ねる
2. 起動してなければ、ユーザーのシェル環境に合った起動コマンドを案内（上記「起動方法」参照）
3. 起動の確認方法 (tasklist / Runners ページ) も環境に合った記法で伝える（上記「動作確認」参照）
4. push 後に Actions タブで進行を確認するよう促す
