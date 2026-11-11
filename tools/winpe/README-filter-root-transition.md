# 更新対象filterのroot値遷移

run228では両DIFが成功した後、`Mt6392Keys`のroot全値hashが変わり、
保持guardが1306で停止した。実before/afterで変化したのは次の2値だけだった。

- `DisplayName`: 旧OEM INFへの間接参照から、同じ表示文字列のREG_SZへ変化。
- `Owners`: baseline集合にcandidate INF basenameが追加されたREG_MULTI_SZ。

他のroot値と312 named値は不変だった。実証拠は作業root内の
`port/gsi/logs/ios-feasibility/windows-rt-wire378-controller-independent-review/`
の`main-postDIF228-filter-root-proof.json`と
`clean-prepared227-Mt6392Keys-root.json`。調査データはsourceへコピーしない。

## 有効化と境界

共有200系emitterに明示的な`filter_root_transitions`入力を追加する。
既存mapにはこの入力がないため、既存profileの保持条件は変わらない。
今回のdiag-only229をこのbranchで生成・配備することはしない。

入力の各行は`role`、`service`、`baseline_root`（path/sha256）のみ。
baseline_rootは、実baseline hiveに帰属する全root値のraw JSONであり、
そのdigestが同profileの`service_preservation[service].root_sha256`と一致する必要がある。
OEM番号はbaselineのOwnersから取得する。実行機ごとの番号をsourceへ固定しない。
baseline_rootの帰属は元capture/nativeの固定root digestとの一致で検査し、
そのraw JSONもinput pinsへ含める。

candidateは既存の署名package検証を通るINFの固定hashに結合する。
INFに対象filterの`AddService=service,0,section`と一意なDisplayNameがあることを要求する。
対応する限定INF構文以外（装飾サービスsection、localized Strings等）は拒否し、
推測で解決しない。関連付けserviceや保持対象の別serviceへ流用しない。

runtimeでは対象のDIFが成功するまでroot全値hashを厳密に比較する。
成功後、および同profileの予定phase1では、次の条件を満たした場合だけ2値を
baselineのtype/length/digestへ正規化し、元の全root digestを再計算する。

- DisplayNameはcandidate INFから取得したplain REG_SZと全byte一致。
- Ownersはbaseline集合とcandidate INF basenameの和集合に一致。
  順序と大文字小文字は比較上同一とし、重複・欠落・未知owner・不正終端を拒否する。
- rootにある他の全値は、名前・型・長さ・raw hashを従来どおり検査する。
  追加・削除も元の全root digestとの不一致になる。

named値のguard、NativeCheckpointの限定schema検証、他serviceのroot guardは変えない。
subtree全体を除外する機能も追加しない。新headerはSCM recipeで前後hashを照合して記録する。

## 検証と未確認範囲

`python3 -B tests/filter_root_transition_actual.py <作業root>`は既保存fixtureを使い、
e255.../1659...の両digest、実2値遷移、境界の拒否、別OEM番号を確認する。
Pythonのoffline契約検証であり、C runtimeを実行した証拠ではない。
今回compile/link/sign・VM・実機操作は行っていない。
採用時に必要なのはSCMの変更TUだけで、GPU/UMD/driverの再buildは不要。
将来のprofileは明示入力とcompile closureを独立reviewしてから有効化する。
