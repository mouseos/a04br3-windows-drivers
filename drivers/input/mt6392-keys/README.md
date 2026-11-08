# MediaTek MT6392 Power and Volume Down Keys

既存共有親1101 source1d248a71のruntime `MT6392\Buttons` 子に、Windows RT8.1の標準 `mshidkmdf` 下位filterとして接続する。SYSは `mt6392-keys.sys`、serviceは `Mt6392Keys`、INF0.0.1.0。ACPI/親/UEFI変更は不要。旧button providerと同じPDOへ重ねてbindせず、この子だけの明示更新候補とする。

実ICはMT6392。固定CT3 Linux7c478212の `drivers/input/keyboard/mtk_pmic_keys.c` はCHRSTATUS bit1をpower、bit2をhomekey inputへ割り当てる。機種 `mt6392.dtsi` のkeycode116/114で後者がvolume downと確定する。`include/linux/mfd/mt6392/registers.h` のCHRSTATUS0x142、Linux実handlerのactive-low、既存 `pmic_read.c/button_state.c` と一致する。

読取りは既存親v1/ARM32byteの `ButtonPmicRead` を使う。親は既存単一PWRAP lock/期限/poison/D0 admissionの下で実CHRSTATUSを読む。この子はPWRAPをmap/初期化せず、PMIC設定/IRQ/power railへ書かない。raw/translatedの一個の排他keypad rangeは旧共通契約で資格確認するだけでmapしない。独自MMIO/親ownerを作らない。

既存button_stateとbutton_pipelineの3validsample debounceを再利用する。20ms pollで、両キーreleasedを確認した後だけarmする。起動時held keyから押下を合成しない。読取り失敗は押下stateをreleaseし、そのD0では再試行しない。通常D0退出はonline閉鎖、同期timer停止、neutral report、manual read queue purge。再入は新しいreleased確認から開始する。parentの既存admission/ref寿命を守り、InterfaceDereferenceはReleaseHardwareで一回行う。

標準reportは2byteの絶対状態：ID1 Generic Desktop/System Control/System Power Down、ID2 Consumer/Volume Decrement。それぞれbit0が押下で、残り7bitはconstant。READ_REPORTとGET_INPUT_REPORT、HID device/report descriptor/attributesを処理する。上位はinbox HIDClass/system-control/consumerで、古いone-byte toggle endpointや「queueが空」の仮定を使わない。報告待ちのstateは最新値へcoalesceするため、readが無い間やpoll間の短い全press/releaseを取り逃す場合がある。

MT8167 keypad側のvolume up/Home、interrupt/wake、S0 idle/低電力中入力、長押し政策、電源長押しのPMIC自律動作は対象外。HID System Power DownはWindowsの電源ボタンpolicyへ作用し得るので、通常GPU試験へ自動混載しない。実機・ユーザー物理キー操作・Windows側イベント/電源政策との結合はmainの後段確認。ソース/host試験/署名を実キー動作成功とは扱わない。

ビルドは `tools/ci/build-mt6392-keys.ps1 -OutputDirectory <専用checkout内新path> -CertificateThumbprint <既存thumbprint>`。既存WDK8.1/BCBF鍵を使用し、新規鍵export/SDK再構築はしない。親/他機器を再buildしない。host core試験は `tests/mt6392_keys_test.c` と既存button_state.c/button_pipeline.cをASan/UBSanで実行する。実register値を偽造してdevice成功とするstubは含めない。
