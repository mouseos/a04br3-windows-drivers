# 親1015上のcodec capture候補

基点は実配置親のsource 13ae9f85c37331a3dd7324278e932956b9a840ef、署名SYS
SHA256 e7df942265a8216ba393152f1fe781e47d48ebe09b6da02f235dd8c2ba7d9752。
旧capture1024親をコピーせず、ADC/VUL/IRQ2/PortCls/KSに必要な変更だけを統合した。

`prototype/capture-rtc1015-profile.json`は署名済み1015 manifestの実効32flagを固定する。
A04_SHARED_WIFI_RTC_BORROW=1を含む既存値をすべて保持し、A04_SHARED_CAPTURE=1だけを追加。
RTC/PWRAP owner、GPU、WiFi、USB、SD resource/driverは変更しない。USB1005の取り込みもない。
新しいcapture ownerは親contextの末尾に追加し、既存fieldの位置を保持する。
AFE PDOへのcapture interface転送は1回だけ。playbackとの排他とDMA公開後の保持を維持する。

新tupleは親/Audioとも2026.11.7.1026。既存1015 INFは変更せず、独立INFを追加する。
`tools/ci/build-capture-rtc1015.ps1 -OutputDirectory <checkout内新規dir>
-CertificateThumbprint <既存開発証明書>`が単一親とPortCls録音SYS/KS clientを作成する。
既存flagを実ビルドmanifestと照合し、欠落/変更時は失敗する。

`-AudioCacheManifest`は同じrunnerの固定e0ac12f1 capture objectを再利用する指定。
compiler/flagsと、clangが列挙した全include/sourceのSHA256、object SHA256が一致した
objectだけを再利用する。変更のあるTUを再compileし、現在sourceとしてlink/署名する。
旧capture親SYSは再利用しない。SDK再構築・配置・実機操作は行わない。

`tests/capture_rtc1015_contract_test.py`はCapture OFF時の親C token完全一致と
RTC/他device source/flag不変を検証する。既存Engine PCM/DMA試験と、
`tests/audio_capture/`のcodec→VUL、METHOD_NEITHER safe copy試験も使用する。

独立build候補であり、実機録音・IRQ/CUR・標準Audio Engine・sleep/resume・独立DMA drainは
未検証。latencyは不明として従来の未対応gateを保持する。ゼロ遅延やfull-duplexを広告しない。
