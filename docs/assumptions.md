# Ver.1 Assumptions

## 1. Scope baseline

Ver.1 は、本格実装前に以下の仮定に基づいて設計する。

- 主入力は AMASA の `.d` ファイルとする。
- 主対象は `.d` ファイル内の `S` event スケーラーデータとする。
- `.root` ファイル、ROOT `TTree`、ROOT histogram、ROOT マクロ出力は Ver.1 の必須入力に含めない。
- 将来 ROOT 入力へ対応できるよう、データ読み取り部分は loader として分離する。
- ROOT は Ver.1 の必須依存にしない。
- Python はまず Python 3.10+ を想定する。
- WSL2 Ubuntu では ROOT 非依存テストを基本にする。

## 2. Data assumptions

### 2.1 Channels

- detector channels は ch1–ch7 とする。
- Any channels は ch16–ch19 とする。
- ch20 AS trigger は将来拡張扱いとし、Ver.1 では表示・監視対象に含めない。
- ch21 と ch32 の正確な意味は未確定であり、Ver.1 では監視対象に含めない。
- ch21 と ch32 は将来確認事項として `docs/open_questions.md` に残す。

### 2.2 Events

- Ver.1 の主対象は `S` event とする。
- `A` event のモニター表示は将来拡張扱いとし、Ver.1 では実装対象に含めない。

### 2.3 Missing data

- `S` event の ch1–ch7 / ch16–ch19 が行内に存在しない場合、Ver.1 では欠損として扱う。
- 欠損は実カウント `0` とは区別する。
- 欠損が 10 秒以上続いた場合も Dead とは判定しない。
- Missing Data は Dead / Recovery と別のイベント種別として扱う。

## 3. Monitoring assumptions

- Dead / Recovery 判定は detector ch1–ch7 と Any ch16–ch19 の両方に適用する。
- Dead 判定は `count == 0` が 10 秒以上連続した場合とする。
- Recovery 判定は Dead 後に `count > 0` が 10 秒以上連続した場合とする。
- Dead / Recovery 判定結果は Ver.1 ではまず別 JSON として出力する。
- Web 表示では可能なら Dead / Recovery / Missing Data をグラフ上に帯またはマーカーとして表示する。
- 表示が複雑になる場合は、JSON 出力を優先する。

## 4. Web assumptions

- Ver.1 のトップページは最低限でよい。
- トップページでは、日付選択、チャンネル選択、グラフ表示、判定結果の確認ができれば十分とする。
- 複数段表示は、まず detector ch1–ch7 と Any ch16–ch19 の 2 グループを優先する。
- チャンネルごとの詳細表示は、可能なら同一ページ内で選択できる設計にする。
- Ver.1 では日単位 JSON を基本とし、巨大な単一 JSON を作らない。
- Plotly は Ver.1 では CDN 読み込みでよい。
- 将来的に Plotly 同梱版へ変更しやすい構成にする。

## 5. Operational assumptions

- Ver.1 ではローカル出力ディレクトリに HTML / JSON を生成するところまでを優先する。
- `wwwhome/asuke/public_html/amasa_data` への自動転送方法は未確定であり、転送処理は設定値で切り替えられる設計にする。
- cron 実行時刻は未確定であるため、Ver.1 では手動実行できる CLI を先に作る。
- cron 化は後続タスクとする。
- run ファイル数が最大 72 に満たない日について、Ver.1 では alert しない。
- run ファイル数は処理ログまたは summary JSON に記録する。
- ログは `logs/` または `output/logs/` に日付別ログとして出す設計案を採用する。
- 実データや不要に大きなログは Git 管理しない。

## 6. Sample data assumptions

- 小さい合成または匿名化サンプルデータを `data/sample/` に追加してよい。
- 本物の実験データや大きなファイルは追加しない。
- サンプルデータを追加する場合は、生成手順または匿名化手順を文書化する。
