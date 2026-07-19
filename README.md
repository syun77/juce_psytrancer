# Psytrancer

## ダウンロード

| プラットフォーム | ダウンロード |
| --- | --- |
| macOS | [macOS版をダウンロード](release/macOS.zip) |
| Windows (x64) | [Windows版をダウンロード](release/x64.zip) |

PsytrancerはVST3形式のMIDIシーケンサー・インストゥルメントです。オーディオ入力は使用せず、ホストのテンポに同期してMIDIノートを出力します。

### インストール
ZIPを展開し、`Psytrancer.vst3` (フォルダごと) をVST3が管理されている適切なフォルダに配置してください。
基本的に DAW 側の VST を検索するパスに入っていれば認識されるはずです。

例としては、次のVST3フォルダへコピーしてから、DAWのVST検索パスを追加、DAWでプラグインを再スキャンします。

| OS | コピー先 |
| --- | --- |
| macOS | `~/Library/Audio/Plug-Ins/VST3` または `/Library/Audio/Plug-Ins/VST3` |
| Windows | `C:\Program Files\Common Files\VST3` |

## シーケンスの基本操作

上部の `Root`、`Scale`、`Octave` で基準となる音階を設定します。`MIDI Key` をONにすると、入力したMIDIノートをルート音として使用します。`Gate Mult` はすべてのStepのゲート長にまとめて掛かります。

グリッドの各列がStepです。Step番号部分をクリックすると有効／無効を切り替え、`G`（Gate）、`C`（Cut）、`H`（Hold）の各行をクリックすると発音タイプを設定できます。`Cut` は短いゲートで発音し、`Hold` は新しいノートを発音せず、直前のノートを保持します。

`Pitch`、`Oct`、`Gate`、`Vel` の各行は、上下ドラッグまたはマウスホイールで編集できます。該当セルを中クリックすると、値を初期値へ戻せます。

キーボードでは、矢印キーでStepの選択とPitchの変更、`Shift` + 上下矢印でスケール単位のPitch変更ができます。`G` / `C` / `H` で発音タイプ、`Delete` でStepを無効化、`+` / `-` でGate、`[` / `]` でVelocityを変更できます。

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

複数ページのシーケンスでは、フッター右側のミニマップでページを切り替えられます。黄色の枠は現在表示しているページ、水色の枠は再生中のページです。

`Follow` をONにすると表示ページが再生中のページへ自動追従します。OFFの場合は、ミニマップをクリックして任意のページを表示できます。

フッターの `Copy Step` / `Paste Step` は選択中の1Stepを、`Copy Page` / `Paste Page` は現在のページ全体をコピー・貼り付けします。`Paste All Pages` はコピー済みのページを全ページへ貼り付けます。

`Pages` は1〜8ページを指定します。1ページのStep数はRateにより決まり、たとえば `1/8T` では12Step、`1/16` では16Step、`1/32` では32Stepです。Rateは `1/8`〜`1/32` の通常・3連・付点から選択できます。
