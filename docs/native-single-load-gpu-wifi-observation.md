# GPU/WiFi保存SYSTEMの同一load読取り

既入口`tools/recovery/read-fresh200-native-inventory.ps1`へ`-IncludeWifiCheckpoint`を追加した。GPU StartGap359は既readerを同rootで呼ぶ。WiFi処理はselected-reader619438d（実native208に使用）の固定読取りをloaded-root関数へ移した。新helper binary/service/profile/診断buildはない。

完成済み実captureのmanifest/hashとSYSTEM trioだけを、新しいWindows PowerShellプロセスへ渡す。未到着captureを作成・推測しない。

```powershell
& "$src\tools\recovery\read-fresh200-native-inventory.ps1" `
 -WorkspaceRoot $work -Manifest $Actual210Manifest -ExpectedManifestSha256 $Actual210ManifestSha256 `
 -SourceDirectory $Actual210SystemTrioDirectory -OutputDirectory $NewOutput `
 -PolicyPath $PinnedPolicyReference -ExpectedPolicyFileSha256 $PinnedPolicyReferenceSha256 `
 -TimingCaptureNumber 210 -IncludeWifiCheckpoint
```

PolicyPathは名称集合の参照。実値は同210 SYSTEMから取得する。既202/208サービスは動的発見対象なので、通常AdditionalServiceNamesは不要。new210不在を観測するだけで登録・再armしない。

- `receipt.json`の`nodes['ACPI\MOU0006']`/`nodes['ACPI\MOU0007']`/`nodes['A04BR3\MT6630SDIO']`: 実選択INF/DriverVersion/date/service。369/1029を期待値で代入しない。
- `startup_gap359.records`: 384BのMiniportV1/BridgeV1/BddV1/TransportQueryV1。raw_binary_hex、entered/completed mask、return status、FILETIME/interrupttime、step_statuses/valuesを保持。step/value対応は既`ge8300-start-gap359-contract.json`。build359は記録ABIでありGPU369の選択とは別。
- 既gpu_init/gpu_factory/module identity/POST stack/lifecycleも同root。
- `wifi-checkpoint/receipt.json`と4raw `.bin`: selected INFとDriverEntry/Initialize/Scan/RuntimeFailure。親inventoryのload/unload/privilege復元・原本不変の結果をcleanup後に継承し、失敗を成功扱いしない。追加loadなし。WLAN profile/SSID/credentialsは読まない。

回収後の集約はhiveを開かない。

```sh
python3 -B "$src/tools/winpe/decode-mt6630-checkpoint.py" --receipt-directory "$NewOutput/wifi-checkpoint"
```

`initialize_parent_bound`と`connection_parent_bound`を確認し、build/generation/parent_generationと`last_observed_connection_event`を報告する。旧RuntimeFailureは現接続へ結合しない。選択情報だけでloaded SYS byte一致を認定しない。StartGapの欠落/entry-only/zero/module identityだけで今回bootやGPU公開成功を認定せず、実bootとFILETIME/interrupttimeを別途結合する。未指定frequency fallbackを前提にしない。

既decoderのfixture testで1029結合/旧記録除外/raw改変拒否を確認。本公開変更後のWindows native実行は未実施。実capture/hash通知後の許可された一回に含めて検証する。
