# ROOT Data Format Considerations

## 1. 目的

この文書は、`amasa_monitor` が将来 `.root` ファイル、ROOT `TTree`、ROOT ヒストグラム、ROOT マクロ由来の出力を扱う場合の設計上の注意点を整理する。

Ver.1 で確定している主入力は AMASA `.d` テキストファイルの `S` event スケーラーデータである。ROOT 入力対応は、データ読み込み層を分離したうえで段階的に追加する。

## 2. 想定する ROOT 系入力

以下の入力形態を想定する。

1. `.root` ファイル内の `TTree`
   - event 単位または scaler sample 単位の branch を持つ可能性がある。
   - time、run、event type、channel、value の対応を確認する必要がある。
2. `.root` ファイル内のヒストグラム
   - `TH1` / `TH2` など。
   - 既に時間 bin や channel bin に集約済みの可能性がある。
3. ROOT マクロ由来の出力
   - CSV、JSON、text、ROOT object など複数形式があり得る。
   - マクロ側の binning、時刻変換、欠損処理の有無を確認する必要がある。
4. 既存 AMASA `.d` ファイル
   - Ver.1 の主対象。
   - ROOT 非依存で処理できる。

## 3. Loader 分離方針

ROOT に依存する処理は、将来 `src/amasa_monitor/io/` 配下に閉じ込める方針とする。

想定する loader:

- `.d` text loader
- uproot-based ROOT file loader
- PyROOT-based ROOT object loader
- C++ ROOT macro wrapper

どの loader も、最終的には共通の canonical table へ変換する。

## 4. Canonical table への変換

ROOT 入力であっても、下流処理には ROOT object を直接渡さない。下流処理は以下のような標準形を受け取る。

### 4.1 S event / scaler 向け wide table

```text
time_jst, run_id, event_number, ch1, ch2, ch3, ch4, ch5, ch6, ch7, ch16, ch17, ch18, ch19
```

### 4.2 汎用 long table

```text
time_jst, run_id, event_type, event_number, channel, value
```

TTree や histogram の構造が wide / long のどちらに近いかを loader 側で吸収する。

## 5. PyROOT / uproot / C++ ROOT の使い分け

| 方法 | 主用途 | 採用判断 |
|---|---|---|
| uproot | `.root` ファイル、特に `TTree` を Python から読む | Python 処理へ直接つなげたい場合の第一候補 |
| PyROOT | ROOT histogram、ROOT object、既存 ROOT 環境との連携 | ROOT object の直接操作が必要な場合に採用 |
| C++ ROOT | 既存 C++ ROOT マクロの再利用、高速 ROOT ネイティブ処理 | 既存マクロの維持や性能要件が明確な場合に限定 |

Ver.1 の `.d` データ処理では ROOT は必須ではない。ROOT 入力を実装する場合でも、下流の binning、Dead/Recovery 判定、JSON 生成、Plotly 表示は ROOT 非依存に保つ。

## 6. 時刻と欠損の共通ルール

- ROOT 入力の時刻が UTC か JST かを必ず確認する。
- 下流処理へ渡す前に timezone-aware datetime に変換する。
- JSON 出力は JST の ISO 8601 文字列とする。
- 欠損は `0` に変換しない。
- ROOT histogram で欠損と 0 が区別できない場合、その制約を metadata に記録する。

## 7. サンプルデータ管理

- 本物の実験データや大きな `.root` ファイルは Git 管理しない。
- 小さい合成または匿名化サンプル ROOT ファイルのみ `data/sample/` 配下に置いてよい。
- sample ROOT file を追加する場合は、生成手順または匿名化手順を文書化する。

## 8. 未確認事項

ROOT 入力を実装する前に、少なくとも以下を確認する。

- 実際の `.root` ファイルに含まれる key 一覧。
- `TTree` 名、branch 名、型、単位。
- time branch の基準時刻、timezone、精度。
- channel 番号と detector / Any の対応。
- histogram の bin 軸が時間、channel、rate のどれを表すか。
- ROOT マクロが既に JST 変換や binning を行っているか。
