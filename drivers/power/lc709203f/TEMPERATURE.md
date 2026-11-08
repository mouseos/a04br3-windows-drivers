# LC709203F BatteryTemperature

既存I2C2 0x1100b000/0x1000・7bit address0x0bの同一gauge ownerを使用する。新driver/resource/親/UEFIは不要。stock CT3 Linux7c478212のlc709203f.c reg0x08とbattery_meter_hw_lc709203f.cのread_adc_v_bat_tempを根拠とする。XNU GaugeCoreのmode/電圧読出しだけでは温度対応済みとはしない。

メーカー作成[LC709203F/D Rev17](https://www.mouser.com/datasheet/2/308/LC709203F_D-1141798.pdf) 9/10/12頁では0x15=1がoperational、0x16=1がTSW/TSENSE thermistor、0x16=0はhost-supplied温度。初期値25℃を実測と扱わない。現在のmodeをPEC付きで確認できた場合だけreg08を読み、method/powerを再確認して0.1Kのまま公開する。メーカー記載の測定範囲0x09e4..0x0d04外は拒否する。これは入力妥当性であり充電上限/制御閾値ではない。mode/B定数/校正/profile/充電/警報レジスタにはwriteしない。

`BatteryTemperature` は標準[BatteryClass query](https://learn.microsoft.com/en-us/windows/win32/power/ioctl-battery-query-information)へULONG/4byteで接続。callbackはWDK8.1 SALと[DDI仕様](https://learn.microsoft.com/en-us/windows/win32/api/batclass/nc-batclass-bclass_query_information_callback)のPASSIVE_LEVELで、battery state lockを解放後にowner bus lockを取る。実read後にD0/タグを再検査し、D0退出・再入の値を跨いで返さない。短bufferはtransaction開始前に拒否する。

温度queryはその場で5read以内。新timerやキャッシュを作らず、既存PIOの10ms/transaction・poisonを維持。I2C mode/未operational/途中mode変化はSTATUS_INVALID_DEVICE_REQUEST、PEC/範囲失敗はSTATUS_DATA_ERROR。温度非対応を残量0へ置換しない。transport faultは既存admissionを閉じ既存battery invalidationへ合流するが、reset/recovery/充電writeはしない。class-handle lockを再帰取得するnotifyはquery内から呼ばない。

今回の完成buildはTemperatureObserveReadOnly、既存BQ/broker/trace flags全0。BQ15既署名write診断は凍結した別候補であり、このSYSへ混ぜない。既存BQ profilesではtemperature callbackをNULLに保ち、勝手に別broker越しへI2C accessを足さない。gauge sample/wire ABIは非変更。

固定consumer `lc709203f-temperature.exe` は引数無しで標準battery interfaceを列挙し、DeviceNameがLC709203Fの唯一のnodeからtag→BatteryTemperatureだけを読む。パス/serial/校正値を出力せず、READ_ERROR=0/RETURNED_BYTES=4/BATTERY_TEMPERATURE_TENTHS_KELVINが実read成功条件。充電control/残量再試験はしない。0x16=0等による非対応はそのまま失敗として記録し、mode1へのwriteで修復しない。

build入口はtools/ci/build-lc709203f-temperature.ps1。既存SDK/WDK/KMDF1.13小closureでLC4TU＋consumer1TUのみcompile/link/sign。tests/lc709203f_temperature_test.cは実PEC readerのprotocol試験であり実NTC測定ではない。標準classのD0/タグ/ロックと実bus/温度はmainの次通常一括で確認する。SoC thermal未取得を電池温度で解決したとは称さない。
