# Implementation Plan

## 0. 方針

この計画は、実装前の設計整理である。Ver.1 では、AMASA `.d` ファイルの `S` event スケーラーデータを日単位で処理し、静的な Plotly HTML と JSON をローカル出力ディレクトリへ生成する。

Ver.1 は ROOT を必須依存にしない。`.root` ファイル、ROOT `TTree`、ROOT histogram、ROOT マクロ出力は Ver.1 の必須入力には含めない。ただし、将来 ROOT 入力へ対応できるよう、データ読み取り部分は loader として分離して設計する。

実装時は、本物の実験データ、大きな `.root` ファイル、不要に大きなログを Git 管理しない。

## 1. 推奨アーキテクチャ

```text
configuration / CLI
  -> input loader
  -> canonical data model
  -> data transformation / binning
  -> monitoring judgment
  -> JSON / summary / log generation
  -> dashboard / static HTML
```

### 1.1 ROOT 依存部分

責務:

- 将来の `.root` ファイル、TTree、histogram、ROOT マクロ出力の読み込み。
- ROOT 固有構造を canonical table へ変換。

配置方針:

- 将来 `src/amasa_monitor/io/` 配下に分離する。
- Ver.1 の `.d` loader も同じ入力層として扱う。
- 下流処理へ ROOT object を直接渡さない。
- ROOT は Ver.1 の必須依存にしない。

技術選定:

- `.root` / `TTree` 読み込みは、将来実装時に `uproot` を第一候補にする。
- ROOT histogram や既存 ROOT object 操作が必要な場合は PyROOT を検討する。
- 既存 C++ ROOT マクロを維持する必要がある場合のみ C++ ROOT wrapper を検討する。

### 1.2 データ変換部分

責務:

- raw event を canonical table へ変換。
- `S` event の ch1–ch7 / ch16–ch19 を抽出する。
- UTC から JST へ変換する。
- 欠損を `NaN` / `null` として保持する。
- 1s / 1min / 20min / 1h へ binning する。
- detector JSON と any JSON を生成できる中間構造を作る。

推奨 canonical table:

```text
time_jst, run_id, event_number, ch1, ch2, ..., ch7, ch16, ch17, ch18, ch19
```

### 1.3 モニタリング判定部分

責務:

- detector ch1–ch7 と Any ch16–ch19 の両方に判定を適用する。
- Dead 判定: `count == 0` が 10 秒以上連続。
- Recovery 判定: Dead 後に `count > 0` が 10 秒以上連続。
- Missing Data 判定: 欠損が 10 秒以上連続。
- 欠損と 0 を区別する。
- 判定結果を event list JSON として出力する。

Ver.1 で実装しないもの:

- 高レート異常。
- 低レート異常。
- 前日比異常。
- file 数不足 alert。
- ch20 / ch21 / ch32 に基づく異常判定。

### 1.4 ダッシュボード / レポート部分

責務:

- 日別 HTML を生成する。
- 日単位 Plotly 用 JSON を読み込む。
- detector plot と any plot の 2 グループ表示を優先する。
- 日付選択、チャンネル選択、グラフ表示、判定結果の確認を可能にする。
- 表示範囲、チャンネル ON/OFF、y 軸範囲、粒度選択、zoom / pan / pinch を提供する。
- 粒度自動選択を実装する。
- Dead / Recovery / Missing Data は可能ならグラフ上に帯またはマーカーとして表示する。
- 表示が複雑になる場合は、events JSON 出力を優先する。

## 2. 開発・実行環境

### 2.1 研究所サーバー Linux

用途:

- 実データ処理の主環境。
- 手動 CLI による Ver.1 処理の実行。
- 後続タスクで cron 化と公開先転送を追加する環境。

構成案:

- Python 3.10+ を想定する。
- Python 仮想環境を用意する。
- ROOT は Ver.1 の必須依存にしない。
- 実データは Git 管理外の既存データ領域から読む。
- 出力はまずローカル出力ディレクトリへ生成する。
- Application server への転送は設定値で切り替えられる設計にする。

### 2.2 Windows 11 + WSL2 Ubuntu

用途:

- 開発、単体テスト、ドキュメント更新。
- ROOT 非依存部分の検証。

構成案:

- Python 3.10+ の仮想環境を用意する。
- 小さい合成または匿名化サンプルデータのみ使用する。
- ROOT がない場合でも、`.d` parser、canonical table 変換、binning、Dead/Recovery/Missing Data 判定、JSON 生成をテスト可能にする。
- ROOT 入力テストは Ver.1 の必須対象外とする。

## 3. Ver.1 実装マイルストーン

### Milestone 1: 基盤と `.d` 入力の最小縦切り

目的:

- ROOT 非依存で動く最小の処理パイプラインを作る。
- 手動 CLI で `.d` サンプルから canonical table、summary JSON、events JSON の土台を生成できる状態にする。

成果物:

- Python package の骨格。
- 手動実行 CLI の入口。
- `.d` loader の最小実装。
- ch1–ch7 / ch16–ch19 の canonical wide table 生成。
- UTC -> JST 変換。
- 欠損と 0 の区別。
- run ファイル数を含む summary JSON の雛形。
- 小さい合成または匿名化サンプル `.d` データ。
- ROOT 非依存の単体テスト。

Milestone 1 で実装しないもの:

- Plotly HTML。
- 1min / 20min / 1h binning。
- Dead / Recovery / Missing Data の完成版判定。
- cron 化。
- 公開先への自動転送。
- ROOT 入力。

具体タスク:

1. `pyproject.toml` と package ディレクトリ構成を決める。
2. `src/amasa_monitor/io/` に `.d` loader 用の場所を用意する。
3. CLI で `--input-dir`, `--date`, `--output-dir` を受け取る設計にする。
4. `DATE` 未指定時は将来「実行日の前日」を使えるようにするが、Milestone 1 では明示指定を基本にする。
5. `.d` ファイルの `S` event のみを読み込む。
6. `A` event は読み飛ばす。
7. 対象チャンネル ch1–ch7 / ch16–ch19 を wide table にする。
8. 行内に対象チャンネルがない場合は missing value とする。
9. `0` は実カウントとして保持する。
10. UTC timestamp を JST の timezone-aware timestamp へ変換する。
11. summary JSON に対象日、入力ファイル数、読み込み event 数、対象チャンネル、生成時刻を入れる。
12. unit test で parsing、欠損と 0 の区別、UTC -> JST 変換を確認する。

受け入れ条件:

- ROOT が入っていない WSL2 Ubuntu でもテストが通る。
- 合成 `.d` サンプルだけで CLI または主要関数を検証できる。
- 実験データや大きなファイルを Git 管理しない。
- 出力はローカル `output/` 配下などに限定できる。

### Milestone 2: binning と Plotly 用 JSON

目的:

- canonical table から日単位の detector / any JSON を生成する。

成果物:

- 1s / 1min / 20min / 1h の binning。
- mean と valid count を保持した channel-array JSON。
- detector と any の JSON 分割。
- 巨大な単一 JSON を避ける日単位出力。

### Milestone 3: Dead / Recovery / Missing Data 判定

目的:

- Ver.1 の最小監視判定を実装する。

成果物:

- Dead / Recovery / Missing Data event list JSON。
- 欠損と 0 を区別する判定ロジック。
- 境界条件の単体テスト。

### Milestone 4: 最小 Plotly ダッシュボード

目的:

- 日別 HTML で detector / any の 2 グループを確認できるようにする。

成果物:

- Plotly CDN を利用した日別 HTML。
- 日付選択、チャンネル選択、グラフ表示、判定結果確認。
- detector / any の 2 段表示。
- 粒度選択と粒度自動選択。
- 欠損 gap 表示。

### Milestone 5: 運用準備

目的:

- 手動 CLI を研究所サーバーで運用できる形にする。

成果物:

- `path_date.txt` 互換または設定ファイル読み込み。
- 日付別ログ。
- summary JSON への run ファイル数記録。
- 転送処理を設定値で切り替えられる設計。
- cron 化手順のドキュメント案。

### Milestone 6: ROOT 入力拡張の事前調査

目的:

- Ver.1 の実装を止めずに、将来 ROOT 入力へ備える。

成果物:

- 実際の `.root` ファイル構造の調査項目。
- `uproot` / PyROOT / C++ ROOT の採用判断メモ。
- ROOT loader が canonical table へ変換するための interface 案。

## 4. テスト方針

- 本物の実験データを使わず、小さい合成データでテストする。
- `.d` loader は数行のサンプルで event parsing を検証する。
- 欠損と 0 の区別をテストする。
- UTC -> JST 変換をテストする。
- binning の mean / valid count をテストする。
- Dead / Recovery / Missing Data の境界条件をテストする。
- ROOT がない環境でも Ver.1 のテストが通る構成にする。

## 5. Git 管理方針

- 実験データ、大きな `.root` ファイル、秘密情報、不要に大きなログはコミットしない。
- `data/sample/` には小さい合成または匿名化サンプルのみ置く。
- sample data を置く場合は、生成手順または匿名化手順を文書化する。
- `logs/` または `output/logs/` に出力される実行ログは原則 Git 管理しない。
