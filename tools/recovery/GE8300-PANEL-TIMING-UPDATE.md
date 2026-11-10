# GE8300パネルタイミング更新

`run-ge8300-panel-timing.py`は既存ad06ba75のcontroller・PB状態機械・CRC回収を
再利用する。追加対象はrun209／実fresh208／回収210の組だけ。
未取得のfreshや最終profileを生成したことにはしない。

新旧driverを混ぜず、display＋render 0.0.3.69と、実LK timingをDTM0へ渡す
boot98deを一組で扱う。UEFIは機種パッケージのみ変更されたa82e2713、
FDはc731f8d6。実0943のL1 clean有効shimを保持したboot出自を検査する。
GPTは変更しない。

同Recoveryの補足取得は既capture-peripheral-preservation.pyへ
`--gpu-runtime-assets`を追加すると、既Present363/356と固定Clearログを一度に取得する。
最大128ログ・補足64MiBに制限し、実snapshotとread-only mountを前後確認する。
取得物とmutable-baseline.jsonは私用入力であり公開repoへ含めない。

入力は既生成器378b7b6fの実fresh/native/最終policyと、実Windowsで署名した
SCM・私用媒体。主担当のsealは旧GPU368契約に加えて次を持つ。

- `schema=GPU369_RUN209_MAIN_SEAL_V1`
- `service_name=data_directory=MT8167CombinedFollowup208`
- `controller_path`は本入口、`controller_engine`は同checkoutの既共有engine。
- `matched_boot`／`matched_boot_manifest`をpinned_filesへ含める。
- `matched_boot_sha256=98de46d30349379669c4a7300d54d7652b40883f01febb10c5b65992a987e322`
- `boot_write_required=true`。sealの`boot_written=false`は実行前の値。
- 更新対象はdisplay・render、予定Windows再起動1回、Present/helper各1回。

実行順は、同Recoveryのread-only preflight→排他的latch→boot17書込み→
maintenance ESP→CRCでCOMMITTED/release0/exit0→通常ESP→更新／予定再起動→
同bootのPresentとglobaldiag→terminal後Windows保持。
preflightは実snapshotとboot/EFIのpartition identity・未mountを検査する。
Recoveryにないawkや、set-eで停止しない否定grepを使わない。

boot書込みは既flash-boot.shと同じboot17／start294912／65536sectorsへ行い、
RAM転送ファイルをhash確認する。通常の物理readbackは行わない。
書込みに失敗した場合は状態をunconfirmedとして停止し、ESP配備・通常起動へ
進まない。未知の結果を未書込みと断定しない。

```sh
python3 -B tools/recovery/run-ge8300-panel-timing.py \
 --workspace "$PROJECT_ROOT" --seal "$ACTUAL_MAIN_SEAL" \
 --seal-sha256 "$ACTUAL_SEAL_SHA256" \
 --output "$PROJECT_ROOT/port/gsi/logs/ios-feasibility/windows-rt-combined-run209-runtime"
```

`--validate-only`は実機に触れない。通常terminal後の回収は、同じ引数に
`--collect-only`を追加して明示実行する。保存latch・UART・terminal proofを
再検証し、boot/ESPの再書込みやdriver/診断の再実行はしない。
回収は同checkoutのcollect-combined200.shを使う。

USB sidecarは本版に含めていない。将来の統合でhelperや再起動条件が変わる場合は
別途最終policy・署名・状態機械を結合してから使用する。
terminalやクライアントのcode0をGPU通常描画成功へ読み替えない。

host検証は既hold6件と新timing3件のみ。実mainをfake transportで実行し、
matched bootの順序、stale snapshotのwrite前拒否、boot失敗時の停止、
collect-onlyでの再書込み禁止を確認する。実機動作の検証ではない。
