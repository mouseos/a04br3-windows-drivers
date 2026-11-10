# MT8167 clock1120 単発raw client

親2026.11.10.1120＋thermal0.0.1.11候補のquery ABI1/resource1117に対するUM。128B snapshotの単発保存専用。温度Measureやclock/rail/DDR設定は行わない。driver packageや既batchへ自動追加しない。

`tools/ci/link-sign-mt8167-clock-snapshot-um.ps1` は既compile directoryのUM objectだけを既ARM UM kernel32/setupapi librariesでlinkし、既BCBF keyで署名する。source/include/object/library hashesを`clock-snapshot-um-contract.json`と比較し、実RT8.1 export whitelistに全importsが含まれることを要求する。新規outputのみ、compiler起動・SDK setup・key export・ARM実行なし。

実runner42729039661では既clock-compile1/mt8167-clock-snapshot.obj（SHA256 5ae52ca984a42d65f77c0d2b10c64b88cf780c59157b7ed915671d1a03e522c1）を使用。既復元17763 ARM UM libsでconsole6.3/ARMNTとしてlinkした。librariesの版だけでRT8.1互換を仮定せず、実RT8.1 kernel32/setupapi exportsへ全20importsを照合する。CRTへの依存なし。署名とimports成功は実Windowsでの起動成功ではない。

mainが次の必要batchへ独立seal後に組み込む場合、親/leafのmatched tupleを確認したうえで、新しい書込み可能な作業directoryをcurrent directoryとして以下を一回だけ実行する。現在の183へ追加しない。

```
mt8167-clock-snapshot.exe --snapshot
```

出力はcurrent directoryの`mt8167-clock-snapshot-v1.bin`。CREATE_NEWなので既存fileは上書きせず失敗する。既thermal device interfaceが一個だけ存在することとservice Mt8167Thermalを確認し、READ_DATA IOCTL0xa06を一回だけ送る。引数不正・interface不在/複数・service不一致・timeout・IOCTL不成功・返却長/ABI不一致では成功を返さない。最大2秒のoverlapped待機後にcancelし、さらに1秒待って未完ならExitProcessする。再試行なし。正常時は128B保存・flush後にCLOCK_SNAPSHOT_RC=00000000とflags/masks/countを出す。失敗時に残ったfileがあっても成功証拠に使わず、exit codeと128B ABI/flagsを併せて照合する。

raw回収後は既offline decoderへ渡す。reference引数なしではconfigured PLL算術もnull。referenceと出典を明示して得るconfigured PLL/mux親値をWindows実効CPU HzやDDR Hzと呼ばない。REFERENCE_UNKNOWN/DDR_UNAVAILABLE/EFFECTIVE_CPU_UNKNOWNはrawで明示される。現工程ではexe未実行、raw未取得、実効CPU/DDR Hz未取得。実機操作はmainのみ。
