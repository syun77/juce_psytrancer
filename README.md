# Psytrancer

## プリセットの保存と読み込み

1. プラグイン上部の `Preset` 欄にプリセット名を入力します。
2. `Save` ボタンを押すと、現在のパターンとパラメータを保存します。
3. 保存済みプリセットは `Preset` のリストから選択すると読み込めます。

保存先フォルダが存在しない場合は、`Save` 時に自動作成されます。

### 保存先

| OS | 保存先 |
| --- | --- |
| macOS | `~/Library/Application Support/Psytrancer/Presets` |
| Windows | `%APPDATA%\Psytrancer\Presets` |
| Linux | `$XDG_CONFIG_HOME/Psytrancer/Presets`（通常は `~/.config/Psytrancer/Presets`） |

### ファイル名

入力したプリセット名に `.psytrancerpreset` 拡張子を付けて保存します。たとえば `My Pattern` と入力すると、`My Pattern.psytrancerpreset` が作成されます。

OSでファイル名に使えない文字は自動的に置換されます。すでに `.psytrancerpreset` を入力している場合は、拡張子を重複させません。

### 保存結果とエラーの確認

`Log` ボタンを押すと、Psytrancerのログウィンドウを開けます。保存が失敗した場合はログウィンドウが自動的に開き、失敗理由と対象パスを表示します。

保存に成功した場合も、ログには保存先のフルパスが記録されます。

## ページ操作とコピー

シーケンスが16Stepを超える場合、フッター右側のミニマップでページを切り替えられます。黄色の枠は現在表示しているページ、水色の枠は再生中のページです。

`Follow` をONにすると表示ページが再生中のページへ自動追従します。OFFの場合は、ミニマップをクリックして任意のページを表示できます。

フッターの `Copy Step` / `Paste Step` は選択中の1Stepを、`Copy Page` / `Paste Page` は現在の16Stepページ全体をコピー・貼り付けします。
