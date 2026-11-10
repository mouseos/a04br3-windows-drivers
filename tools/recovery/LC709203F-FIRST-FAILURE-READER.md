# LC709203F 初回失敗の同一load読取り

`read-fresh200-native-inventory.ps1` の既引数に
`-IncludeBatteryFirstFailure` を追加する。既load内でcore inventoryの後に
`Read-Lc709203fFirstFailure` を呼び、receipt.battery_first_failureへ補足する。
本変更は端末collector・hiveload・service実行を追加しない。
取得済native221への追加replayは行わない。次の許可済single-loadで使う。

固定pathは `ControlSet001\Services\LC709203F\Parameters\NativeCheckpoint`。
`LoadEpochV1` REG_BINARY16B と `FirstFailureV1` REG_BINARY96Bだけを読む。
現在のNative.OpenPathはServices配下を既に許可しており、guard変更は不要。
ドライバ側初期READMEの「whitelist追加必要」という記述はこのreaderでは該当しない。

receipt追加形は `{path,present,same_loaded_root:true,hardware_success_claimed:false,
values:{LoadEpochV1:{present,type,bytes,raw_hex},FirstFailureV1:{present,type,bytes,raw_hex}},
epoch_matches:null|bool,schema_valid:null|bool}`。
REG_BINARYのtypeは3。欠落やstaleを成功扱いしない。
helperはepoch header/sizeとload epochの一致だけを検査する。
完全decodeにはdriver branchの `decode-lc709203f-first-failure.py` を使い、
同じreceiptのselected INF/version/SYS hashおよびboot証拠と結合する。
別load/bootの値を現在失敗として扱わない。保存成功を実機成功とは扱わない。
