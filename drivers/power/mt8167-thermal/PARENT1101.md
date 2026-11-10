# MT8167 thermal + MT6392 RTC / parent1101

親1026 source2d623172へRTC1100を載せたc3cda7a0を基点に、既存03593b57のnative PIO実測ownerと正規8rangeのみ選択統合した。親を旧1022へ置き換えていない。独立branch work/mt8167-thermal-rtc1101-20261108。

新親2026.11.8.1101、thermal0.0.1.7。native IF v2はParentBuild1101を厳密比較する。別GUID/ABIのRTC calendar0.0.1.0と署名client/INF/CATはrtc5 manifest49f698b8のbytesを保持し、再compile/再署名しない。Audio1026/capture v1、RTC32k borrow、USB1005、WiFi1012、GPU v2/SD1、AFE DMA forwardingは基点を保持。

105x HFP/AFE/SCOの新ABIは取り込まない。このtupleは1026のAudio v11/capture v1であり、105x候補の代わりとはしない。将来合流時は資格とsourceを再照合する。親ownerの複製、別SPM/PMIC ownerはない。

採用にはe3b18c0f POST保持の対応機種firmware6c96c6f6、8range、今回起動のLK校正・PIO stop recordが必要。旧6range boot/旧1022親と混ぜない。Linux由来の200us settleとchannel11 old-ready0→trigger→25us→new-ready1、校正devinfo60/61/62と変換は既存sourceを再利用する。500ms期限、通常timeout失敗/未知停止retain、readout freshness1000msを保持。新しい保護閾値やOS時計補正は追加しない。

出力SYSはmt8167-thermal.sys、表示名はMediaTek MT8167 Thermal Sensor。起動時一回のD0測温結果はA04THM PIO mc=とIDLE restored parent1101へ出力する。continuous保護、反復測定、forced remove/suspend時の物理保持、精度、実温度は実機未確認。

ビルド入口 tools/ci/build-mt8167-thermal-rtc1101.ps1。既存WDK/Signerを使用し、専用checkout内のOutputDirectoryとFrozenRtcDirectoryを指定する。鍵のexport/再作成やSDK再構築は不要。親/thermalだけをcompile/signし、凍結RTCを同梱してsource/flags/全出力hashを保存する。
