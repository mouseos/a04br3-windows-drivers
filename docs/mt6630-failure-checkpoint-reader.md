# MT6630 provider1027 failed-session固定checkpoint reader

main取得済みSYSTEM trioをprivate cloneへcopyしてRegLoadKeyでreplayする。
`ReadonlyInventory200HiveNative.cs` は公開source45ba19dの既存helperをbyte不変で同梱する。
7ec backup/restore既存key open、private mount登録、REG_LINK拒否を保持し、ACL/値writerは追加しない。
BT用read-mt6630-start-checkpoint.ps1は使用しない。

対象はControlSet001\Services\Mt6630Runtime\Parameters\NativeCheckpointの5値だけ。
GenerationはREG_QWORD8、DriverEntry/Initialize/Scan/RuntimeFailureはREG_BINARY1624。
record schema1、magic50433441、header88、event24×64、1027内部legacy build1023を検査する。
有効eventだけが使用され、unused領域zeroも検査してからbinをexportする。
raw/原本pathや自由文字列、SSID/password/MAC/IPをreceiptへ出さない。固定numeric fieldのみ。
recipe/入力manifest/trio/各binaryのhashをreceiptに残す。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\recovery\read-mt6630-failure-checkpoint.ps1 `
 -WorkspaceRoot C:\mt6630-failure205 -Manifest C:\mt6630-failure205\failed205\manifest.json `
 -ExpectedManifestSha256 53d1220b0ae9b4feda0466293924504d2624b554f36cab86331b25228490e166 `
 -SourceDirectory C:\mt6630-failure205\failed205\config `
 -OutputDirectory C:\mt6630-failure205\out205 -EvidenceRole failed-session-snapshot
```

204は別source manifest/trio/出力で同じworkerへbaseline-onlyとして入力する。
原本hash不一致・schema違反・unload/privrestore失敗はreceipt.status=failed。
private cloneはreplayで変化しうるが原本trioは前後hash不変を要求する。
欠落値を補完しない。読み取れたことは接続成功や原因判明ではない。
SYSTEM205のcapture時刻とrecordのsystem_time/interrupt_time/generation/parentを別途結合する。
RuntimeFailureはadapterごと最初1件、Scanは最大4snapshotであり今回試験を必ず表さない。
イベント単位timestampやassociation/key/port独立のcheckpointは現schemaに存在しない。

実byte decoderは同source `tools/winpe/decode-mt6630-checkpoint.py` を使う。

```sh
python3 -B tools/winpe/decode-mt6630-checkpoint.py RuntimeFailure.bin --after-generation BASELINE_GENERATION
```

--after-generationは各channelの実baseline recordのgenerationで、無ければ指定しない。
Global Generationカウンタだけで失敗試験への結合や成功を主張しない。
Decoderのstage88 rawはNDIS status/detailはOID、stage89はbackend RX error/detail RX phase、
stage87はruntime result/detail caller phase。OBSERVATION kindのrawはNTSTATUSではない。

対照source1027:
checkpoint.c:29/51/57 保存先、checkpoint_wire.h:30 struct、ndis_runtime.c:62 snapshot。
native_control.c:163 association待機/:173 completion/:297 OID snapshot、station_runtime.c:63 auth/:71 assoc。
ndis_runtime.c:198 管理frameをstationへ返す/:203 early EAPOL、native_security.c:155 key/PN/portのRX境界。
native_hif_rx.c:39 translated RX拒否。ただしその形式が今回来たかは未確認で1028適用根拠にしない。

PS1はUTF8BOM。host testはABI/decoderとsource制限までで、Windows parser/clone実行は別証拠が必要。
