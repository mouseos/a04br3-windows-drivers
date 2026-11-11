# post223-resume224の同一load読取り

基点ec825431は不変。別branchのreaderだけを更新する。
新profile、capture alias、仮manifest/receipt、書込み、再起動は作らない。
実capture完成/hash通知までは実行しない。

既entryの必須実入力引数を維持し、次を追加する。

```powershell
& "$SourceCheckout/tools/recovery/read-fresh200-native-inventory.ps1" `
 -WorkspaceRoot $WorkspaceRoot -Manifest $ActualPostManifest `
 -ExpectedManifestSha256 $ActualPostManifestSha256 `
 -SourceDirectory $ActualSystemTrioDirectory -OutputDirectory $NewOutputDirectory `
 -PolicyPath $ExistingNameReferencePolicy -ExpectedPolicyFileSha256 $PolicyRawSha256 `
 -ExpectedPolicySha256 $PolicySemanticSha256 `
 -TimingCaptureNumber 223 -CaptureSuffix resume224 `
 -IncludeBatteryFirstFailure -IncludeWifiCheckpoint `
 -IncludeGraphicsTopologyCache -IncludeDisplayDeviceBindings
```

変数は回収後の実入力へ設定する。`ActualPostManifest`はworkspace内の実
`private-windows-rt-prebackup-run223-resume224/manifest.json` のまま転送する。
`ActualSystemTrioDirectory`と隣接Driversはそのmanifestの実fileを使う。
名前参照policyは既方式のままで、過去snapshotを現在値として扱わない。
`CaptureSuffix`の許可値は`resume224`だけ、捕捉番号223だけに結合する。
省略時は従来の`private-windows-rt-prebackup-run<N>/manifest.json`だけ許可。
別番号、legacy200、別suffix、suffix無しでの当該post入力は拒否する。
workspace内判定とGetFullPath後の末尾literal一致を併用し、manifest.jsonのdotもliteral。

実manifest SHA、ro/unmounted/errors、Recovery ID、SYSTEM各3本のhash/size検査と
原本不変確認は変えない。receiptに実source_manifest_pathとcapture_directory_suffixを追加する。
捕捉番号は223のまま。各optional readerは既Load/Unload内へ接続され、別loadしない。
電池欠落は成功ではなく、WiFi/sys/epochも既世代検査を維持する。

WiFi1031は公開source6aa39105のcheckpoint_wire.hに基づくschema1/1624Bと
RESET_BOUNDARY128（boundary4=WLAN-only）だけを追加解釈する。
unknown buildへconnection trace意味を拡張せず、Initialize/DriverEntryとの世代結合は維持する。
rawを保存し、後処理は既`decode-mt6630-checkpoint.py --receipt-directory`を使用できる。

hostでは既Python decoder試験と差分検査のみ。PowerShell実行/Windows parser、
native Load、端末/VM操作、driver buildは今回未実施。
