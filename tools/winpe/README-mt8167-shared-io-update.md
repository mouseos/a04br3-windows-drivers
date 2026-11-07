# 単一shared parentと周辺機器の一括更新

`build-mt8167-shared-io-update-media.py`は署名済みparent/audio/USB/Wi-Fiを一括でDISM適用し、固定vendor firmwareと任意の一回限りハードウェアprobeサービスを配置する保守媒体を生成する。ドライバ開始や実機動作の成功を媒体生成から推測しない。EDK2共通コードは変更しない。

入力は作業ルート、固定maintenance base、署名package、固定MT6630 firmware、直前のRecovery backup manifest、新規出力先、既存wimtools、完全source commit。`--probe-service`は署名済みMT8167HardwareProbe.exeを指定する。4 INFと単一親SYS、署名後全入力hash、WMT/WLAN既知hashを検査する。

SYSTEM/SOFTWARE/DRIVERSと必要なtransaction logsは、同じRecovery起動中にread-only mountからホストへ退避する。backup manifestのstatus=captured-unmounted、filesのbytes/hash、dirty hiveのlog存在を検証し、ESPへhive内容やserialを組み込まない。旧WinPEで同じ大容量backupを再書込みしない。バックアップ取得後にWindowsを一度でも起動した場合は失効する。**writerは書込み直前にRecovery boot_idがmanifestと一致することを確認する。** 新UEFI bootのflashが必要なら、先にflashしてRecoveryへ入り、その後バックアップを取得する。

実機順序は新UEFI/profile準備→Recovery backup→媒体生成→同Recovery boot_id照合→ESP更新→CRC付きCOMMITTED＋release=0＋exit=0確認→通常ESPへ戻す→UARTで同一bootのPnP/IRQ/各機能確認。失敗または未完了のservicingで通常OSへ進めない。通常ESPへ戻す際も新しいshared IO profileのUEFIを保持する。

サービスはSystem32/MT8167HardwareProbe.exe、固定clientとログはSystem32/MT8167HardwareProbe/。LocalSystem/AutoStartで一回、試行前に無効化・markerを永続化。SSID/BSSID/鍵/個体校正をログへ出さない。USBキーボード、音声DMA、WLAN scan、通常音声endpointの成功条件はそれぞれ分けて記録する。
