# AMASA Monitor Specification

## 1. 目的

`amasa_monitor` は、Akeno Mini Air Shower Array / Akeno Mini Array のスケーラーデータを日次で処理し、各検出器チャンネルおよび Any トリガーのカウントレートをブラウザ上でインタラクティブに確認できるモニターページを生成するシステムである。

Ver.1 では、手動実行できる CLI による日単位の静的 HTML / JSON 生成を優先する。cron 化と公開先への自動転送は後続タスクとし、数分おきの準リアルタイム更新は行わない。

## 2. 想定する入力データ

Ver.1 の主入力は日付ごとの AMASA `.d` テキストデータである。Ver.1 の必須入力は `.d` ファイル内の `S` event スケーラーデータに限定する。

`.root` ファイル、ROOT `TTree`、ROOT ヒストグラム（例: `TH1`, `TH2`）、ROOT マクロが生成した中間出力（CSV / JSON / text / ROOT object など）は Ver.1 の必須入力には含めない。ただし、将来 ROOT 入力へ対応できるよう、データ読み取り部分は loader として分離して設計する。

## 3. 元データ保存場所とファイル名

元データは日付ごとに以下へ格納される想定である。

```text
icrhome04:/disk/alpaca/data/Akeno-MiniArray/data/
```

例:

```text
/disk/alpaca/data/Akeno-MiniArray/data/260707/26070701.d
```

カウントデータのファイル名は以下の形式とする。

```text
yymmddNN.d
```

- `NN` はその日 UT における通し番号。
- 1 run は 20 分。
- 1 日あたり `yymmdd01.d` から最大 `yymmdd72.d` まで存在する。

## 4. `.d` データフォーマット

1 行が 1 event を表す。基本フォーマットは以下である。

```text
run# type event_number date time [gps_subsec] ch data ch data ...
```

| 項目 | 内容 |
|---|---|
| 1列目 | run番号。`YYMMDDNN` の8桁 |
| 2列目 | event種別。`A` は空気シャワーイベント、`S` はスケーラーイベント |
| 3列目 | event number。`A` と `S` で独立 |
| 4列目 | 日付。UT |
| 5列目 | 時刻。UT、小数秒付き |
| 6列目 | `A` event の場合のみ GPS clock 秒以下 |
| 以降 | `ch number`, `data` のペア |

例:

```text
26070701 S 1 2026-07-07 00:11:50.001219 1 177 2 111 3 186 4 173 5 148 6 170 7 148 8 0 16 1068 17 0 18 0 19 0 20 0 21 10000000
```

## 5. Event とチャンネル定義

### 5.1 S event

`S` event はスケーラーの 1 秒値であり、Ver.1 のモニタリングでは主に `S` event を用いる。

| ch | 内容 | Ver.1での扱い |
|---|---|---|
| ch1–ch7 | シンチ検出器の PMT 信号 | 表示・Dead/Recovery/Missing Data 判定対象 |
| ch16 | Any1 | 表示・Dead/Recovery/Missing Data 判定対象 |
| ch17 | Any2 | 表示・Dead/Recovery/Missing Data 判定対象 |
| ch18 | Any3 | 表示・Dead/Recovery/Missing Data 判定対象 |
| ch19 | Any4 | 表示・Dead/Recovery/Missing Data 判定対象 |
| ch20 | AS trigger | Ver.1 では表示・判定に使用しない |
| ch21 / ch32 | reference clock 相当の可能性 | 解釈未確定。Ver.1 では使用しない |

### 5.2 A event

`A` event は空気シャワーイベントであり、データは TDC count である。TDC の単位は `100 ps/count`。`A` event では ch1–ch7 が PMT 信号、ch32 が trigger signal とされるが、イベントによってチャンネル抜けがある。

Ver.1 では `A` event の詳細解析は実装しない。

## 6. Ver.1 の処理対象

Ver.1 では、プロット対象は以下とする。

- 検出器チャンネル: ch1, ch2, ch3, ch4, ch5, ch6, ch7
- Any チャンネル: ch16, ch17, ch18, ch19

ch20, ch21, ch32 は Ver.1 では表示・判定に使用しない。

## 7. 時刻処理

- 元データの時刻は UT として読む。
- 内部処理では timezone-aware datetime を使う。
- モニターページおよび JSON 出力では JST に変換した ISO 8601 文字列を使う。

例:

```text
2026-07-07T09:11:50+09:00
```

## 8. データ生成方針

### 8.1 内部標準形式

raw data を直接 Plotly 用 JSON にしない。一度、内部標準形に変換してから、binning、Dead/Recovery 判定、JSON 生成を行う。

推奨する処理の流れ:

```text
raw data
  -> parser / loader
  -> canonical table
  -> binned table
  -> plot JSON
  -> Plotly HTML
```

S event の標準内部形式は、1秒1行の wide table を推奨する。

```text
time_jst, run_id, event_number, ch1, ch2, ..., ch7, ch16, ch17, ch18, ch19
```

チャンネル別処理や集計が必要な場合のみ long format に変換する。

### 8.2 欠損値

- 欠損は補間しない。
- `S` event の ch1–ch7 / ch16–ch19 が行内に存在しない場合、Ver.1 では欠損として扱う。
- 欠損を `0` として扱わない。
- 欠損は内部では `NaN`、JSON では `null` とする。
- Plotly では線をつながず gap として表示する。
- 欠損が 10 秒以上続いた場合は Dead ではなく Missing Data として扱う。

`0` は「実際のカウントゼロ」、`NaN` / `null` は「データなし」と区別する。Dead / Recovery と Missing Data は別のイベント種別として扱う。

### 8.3 粒度

Plotly が読み込むための JSON は、以下の粒度別に生成する。

- 1s
- 1min
- 20min
- 1h

binning では、各 bin について平均値と valid count を保持することを推奨する。

## 9. JSON 生成仕様

Ver.1 では、Plotly 用 JSON はチャンネルごとの配列形式を推奨する。

```json
{
  "metadata": {
    "date": "2026-07-07",
    "timezone": "JST",
    "source_timezone": "UTC",
    "bin_seconds": 60,
    "generated_at": "2026-07-08T00:10:00+09:00"
  },
  "time": [
    "2026-07-07T09:11:00+09:00",
    "2026-07-07T09:12:00+09:00"
  ],
  "channels": {
    "ch1": {
      "label": "ch1",
      "rate": [153.2, 148.1],
      "n": [60, 60]
    },
    "ch16": {
      "label": "Any1",
      "rate": [1040.2, 1012.5],
      "n": [60, 60]
    }
  }
}
```

JSON は detector と any に分けて生成する。

```text
detector_1s.json
detector_1min.json
detector_20min.json
detector_1h.json
any_1s.json
any_1min.json
any_20min.json
any_1h.json
```

## 10. 粒度の自動決定

1 秒値をそのまま長期間表示すると Plotly の描画が重くなるため、表示期間と表示チャンネル数から総データ点数を見積もり、表示点数が目標以下となる最小 bin を選択する。

基本式:

```text
表示点数 = 表示期間秒数 / bin秒数 * 表示チャンネル数
```

推奨初期値:

- 目標: 1万〜5万点程度以下
- 初期値: 2万点程度

候補:

```python
candidate_bins = [1, 60, 1200, 3600]  # 1s, 1min, 20min, 1h
target_total_points = 20000
```

ユーザーが明示的に粒度を選択した場合は、自動選択よりもユーザー指定を優先する。

## 11. プロット仕様

### 11.1 検出器チャンネルプロット

| 項目 | 内容 |
|---|---|
| 横軸 | 時間 |
| 縦軸 | カウントレート |
| 対象 | ch1–ch7 |
| 初期表示 | ch1–ch7 全て ON |
| y軸初期範囲 | 0–400 |
| 時刻 | JST 表示 |
| 欠損 | gap として表示 |
| ファイル境界 | 表示しない |

操作機能:

- 表示時間範囲の指定
- プリセット時間範囲の選択
- y 軸最小値・最大値の変更
- チャンネルごとの ON/OFF
- データ粒度の選択
- zoom / pan / pinch in/out
- 同一グラフへの重ね描き
- 複数段表示

複数段表示時には、各段の横軸および縦軸の zoom / pan 状態を同期させる。

### 11.2 Any プロット

| 項目 | 内容 |
|---|---|
| 対象 | Any1, Any2, Any3, Any4 |
| 対応 ch | ch16, ch17, ch18, ch19 |
| 横軸 | 時間 |
| 縦軸 | カウントレート |
| y軸初期範囲 | 0–1000 |
| 時刻 | JST 表示 |
| 欠損 | gap として表示 |

### 11.3 時間範囲指定

以下のプリセットを用意する。

- yesterday
- 1d
- 3d
- 7d
- 30d

ユーザーが任意の開始時刻・終了時刻を指定できるようにする。

## 12. Dead / Recovery 判定

Ver.1 では一般的な異常検知は実装しない。ただし、detector ch1–ch7 と Any ch16–ch19 の両方に対して Dead 判定、Recovery 判定、Missing Data 判定を実装する。

### 12.1 Dead 判定

あるチャンネルで、連続 10 秒以上カウントが 0 の場合、Dead と判定する。

```text
count == 0 が 10 秒以上連続 -> Dead
```

### 12.2 Recovery 判定

Dead 判定後、連続 10 秒以上カウントが 0 でない状態が続いた場合、Recovery と判定する。

```text
Dead 後、count > 0 が 10 秒以上連続 -> Recovery
```

### 12.3 Missing Data 判定

対象チャンネルの値が欠損した状態が 10 秒以上続いた場合、Dead とは別の Missing Data として扱う。欠損は `count == 0` ではないため、Dead 判定には含めない。

```text
missing value が 10 秒以上連続 -> Missing Data
```

### 12.4 判定結果の出力

Dead / Recovery / Missing Data の判定結果は、Ver.1 ではまず別 JSON として出力する。Web 表示では可能ならグラフ上に帯またはマーカーとして表示するが、表示が複雑になる場合は JSON 出力を優先する。

### 12.5 Ver.1 で実装しない異常検知

- 高レート異常
- 低レート異常
- 前日比異常
- file 数不足 alert
- ch21 / ch32 に基づく異常判定

Dead / Recovery / Missing Data 以外の異常検知は、今後必要に応じて拡張する。

## 13. 更新方式

Ver.1 では、まず手動実行できる CLI により対象日の HTML / JSON をローカル出力ディレクトリへ生成する。cron による日次実行は後続タスクとし、数分おきの更新は Ver.1 では不要とする。

## 14. 公開先

Ver.1 では、生成した HTML および JSON をローカル出力ディレクトリへ出力するところまでを優先する。将来的な Application server 上の公開先候補は以下である。

```text
wwwhome/asuke/public_html/amasa_data
```

このディレクトリへの自動転送方法は未確定である。転送処理は Ver.1 の必須処理に含めず、将来 `enable_transfer` のような設定値で切り替えられる設計にする。

## 15. Web 表示仕様

### 15.1 トップページ

Ver.1 のトップページは最低限でよい。日付選択、チャンネル選択、グラフ表示、判定結果の確認ができれば十分とする。日付一覧は月ごとに整理する。

例:

```text
2026年7月
  7月1日
  7月2日
  7月3日
```

日付をクリックすると、その日のプロットページへ遷移する。トップページの高度な UI 作り込みは後続フェーズで扱う。

### 15.2 日別ページ

日別ページには対象日のプロットを表示する。プロット周辺に操作メニューを配置し、以下を変更できるようにする。

- 表示時間
- y 軸範囲
- 表示チャンネル
- データ粒度
- 他グラフの重ね描き
- 他グラフの下段配置

## 16. 出力物

Ver.1 の出力物は以下とする。

- 日別 HTML
- Plotly 用の日単位 JSON
- Dead / Recovery / Missing Data 判定結果 JSON
- run ファイル数などを含む summary JSON
- `logs/` または `output/logs/` 配下の日付別ログ

推奨ディレクトリ構成例:

```text
amasa_data/
  index.html
  2026/
    07/
      07/
        index.html
        detector_1s.json
        detector_1min.json
        detector_20min.json
        detector_1h.json
        any_1s.json
        any_1min.json
        any_20min.json
        any_1h.json
        events.json
        summary.json
  logs/
    2026-07-07.log
```

## 17. 実行環境

### 17.1 研究所サーバー Linux

- Python 3.10+ を想定する。
- ROOT は Ver.1 の必須依存にしない。
- 実データ処理の主環境だが、Ver.1 では手動実行できる CLI を先に用意する。
- cron 化と Application server への自動転送は後続タスクとする。
- 実データはサーバー上の既存データ領域から読み込む。

### 17.2 Windows 11 + WSL2 Ubuntu

- 開発・テスト用環境。
- 実データは直接扱わず、小さい合成データまたは匿名化サンプルを使う。
- ROOT が未導入でも、`.d` parser、binning、JSON 生成、Dead/Recovery/Missing Data 判定はテストできる設計にする。
- ROOT 入力部分は Ver.1 の必須実装に含めない。将来対応時は mock または小さい sample ROOT file で検証する。

## 18. PyROOT / uproot / C++ ROOT の比較

| 選択肢 | 向いている用途 | 長所 | 注意点 | Ver.1での推奨 |
|---|---|---|---|---|
| uproot | Python から `.root` / `TTree` を読む、NumPy/Pandas へ変換 | Python 環境へ組み込みやすい。CERN ROOT 本体が不要な場合がある。CI/WSL2で扱いやすい | ROOT マクロ実行や C++ ROOT 固有処理には不向き | Ver.1 では必須依存にしない。将来 ROOT ファイル読込の第一候補 |
| PyROOT | Python から CERN ROOT オブジェクトや既存 ROOT 環境を直接操作 | ROOT ヒストグラムや既存 ROOT コードとの親和性が高い | CERN ROOT の導入が必要。環境差が出やすい | Ver.1 では必須依存にしない。既存 ROOT object / macro 連携が必要な場合に使用 |
| C++ ROOT | 既存マクロ、高速処理、ROOT ネイティブ解析 | ROOT 資産をそのまま利用しやすい。高速化しやすい | Python 系処理との接続設計が必要。ビルド/実行環境依存が強い | 既存 C++ ROOT マクロ再利用が必要な場合に限定 |

Ver.1 の AMASA `.d` スケーラーデータ処理では ROOT は必須ではない。将来 `.root` / `TTree` 入力を扱う場合は、まず `uproot` による読み込みを検討し、既存 ROOT マクロや ROOT histogram object の直接利用が必要な範囲だけ PyROOT または C++ ROOT を使う。

## 19. Ver.1 で実装する範囲

- 手動実行できる CLI による対象日・入力パス・出力先の指定
- 必要に応じて `path_date.txt` 互換の設定読み込み
- S event のスケーラーデータ読み込み
- ch1–ch7 の時間変化プロット
- Any1–Any4 の時間変化プロット
- JST 変換
- 欠損の gap 表示
- 粒度自動選択
- ユーザーによる粒度選択
- チャンネル ON/OFF
- y 軸範囲変更
- zoom / pan / pinch 操作
- 重ね描き / 複数段表示
- Dead / Recovery / Missing Data 判定
- Plotly 用の日単位 JSON 生成
- summary JSON と日付別ログの生成

## 20. Ver.1 で実装しない範囲

- cron 化
- 公開先への自動転送
- 数分おきのリアルタイム更新
- ファイル境界のプロット表示
- ファイル数不足アラート
- 高レート・低レート異常検知
- 前日比異常検知
- ch21 / ch32 の解釈および利用
- A event の詳細解析
- トップページの詳細 UI 作り込み
- 本物の実験データや大きな `.root` ファイルの Git 管理
