# MT8167 headphone route components (not deployment-ready)

CT3 GPL `7c4782127d4ea11965be00ce1343056be9bfb769` の一次sourceを根拠とする独立実装。MT6392は内蔵Class-D speakerであり、HP出力はMT8167内蔵codec。`mt8167-codec.c` のAIF RX→Left/Right DAC→Left/Right Audio Amp→HP Depop VCM→HPOUT Mux→AU_HPL/AU_HPRと、AU_LOL→MT6392 Int Spk Ampを混同しない。

## 実装した部品

`drivers/bus/mt8167-shared/codec_headphone.{c,h}` は既存単一親の排他audio leaseから呼ぶportable transaction。common domain/実speaker mute/実測generation/期限/資源をqualified callbackで要求し、voice/DAC/他DL activeを事前拒否する。GPIO write、独立mapping/owner、MT6392 unmute、thermal/RTC/USB制御は追加しない。

左右の実A/B/C AUXADC平均から選択gainの補償値を変換するpure関数、ステレオDC更新、cap/gain対応の2段ramp、HP depop、左右DAC/amp、所有maskのみのreadback・保存値復元を実装する。`0x4b4b`はCT3 gather_comp_valの明示DAC-stage値で、未知係数のfallbackではない。両channelの測定完了・generationなしでは生成/出力を拒否する。測定を行うprovider自体は含まれず、入力データを実測と合成しない。

I/O bank0=親APMIXED、bank1=親がbind済みのAFE。start後の失敗はpoison/heldを残し、blind rollbackしない。stopは独立のDMA/IRQ drain証明を必須とし、active bit消失だけでdrainとみなさない。既存capture/共通reference bitsはmask外として保存する。DC制御はhandshakeであり、古いcontrol raw値を書き戻さず保存coefficientsを再latchする。ownerがleaseを解除するのは正常stop後だけ。

`headphone_endpoint.{h,cpp}` は実HP backendが渡された場合だけ使える標準PortCls factory。別名Mt8167HeadphoneWaveRT/Topology、48kHz S16 stereo既存miniport、WaveRT pin1→topology pin0→KSNODETYPE_HEADPHONES pin1の接続。既存speaker/captureやsense-only pin2を改名/接続しない。DESCRIPTION/DESCRIPTION2はqualify済の実senseだけを返す。未知状態をConnectedにせず、イベント/動的format/不明latencyは追加しない。

backendのreference/dereference、再生stream制御、既存DL1のspeaker/HP排他、route選択、PnP/power revoke、DMA保持/restore契約を実ownerが実装してからStartDeviceでfactoryを呼ぶ。部分登録後のfailureはPortClsの通常StartDevice unwindを要求する。providerが無いのに成功するstub、synthetic backend、固定Connectedは無い。

## 現在の阻害条件（未解決）

現親1103にはHP A/B/C測定を供給する共有AUXADC lease/clock/deadlineとmeasurement routeが無い。既存AUXADC channel11はthermal ownerの資源/寿命契約で、HP channel7/8として流用/再mapしない。既存audio v11はspeaker/voiceを前提とし、HPのmode切替・drain/restore providerでもない。HP DC OffsetsはLinuxで実A/B/C変換とgain別測定を行う。cold zero配列や保存係数を測定値へ偽装しない。

したがって現adapterからHP factoryを呼んでいない。HP endpointは未登録、analog transactionも未接続。Audio1028の署名物は部品のARM link/署名証拠であり、HP使用目的で配備する候補ではない。実標準stackのHP再生、音量/抜挿し/restore成功は未確認。

GPIO19の資格未成立の可能性は前ACCDET1103理由観測で切り分ける。GPIO mux/IES/pullを推測writeして通さない。次の実装は既存親単一ownerでのHP measurement/route leaseをthermal ownerと競合なく接続し、共通DL1のendpoint選択を実装すること。owner変更権限と根拠なしに実機観測を代替しない。

## 検証・再現

`tests/mt8167_headphone_route_test.c` は実transactionの429 start/386 stop fault位置、実測不足拒否、左右符号・ramp、readback、保存値復元、既存voice/DAC拒否、独立drain不成立・poison保持をASan/UBSanで検証する。I/Oはmock。

`tests/mt8167_headphone_endpoint_test.cpp` は実factory/graph/propertyをPortCls/DDI fixtureで検証する。provider欠落/未準備なら登録0、pin方向/接続、実sense payload、partial-registration unwindを確認する。実Windows実行証拠ではない。

`tools/ci/build-mt8167-hp1028-component.ps1` は固定AFE1027 cache81f20c78を検証し、22object再利用＋adapter/endpoint/analogの3TUだけcompile、同SignerBCBFでSYS/CATを署名する。新規INFは1028、NativeStart revisionも1028。commonSDK/親/owner/UEFIをbuildしない。component entrypointsは明示link保持するが、adapterから自動実行しない。

## 一次source対応

- sound/soc/codecs/mt8167-codec.c: left/right DAC events、HP Audio Amp gain、hp_depop_setup/enable/disable、DAPM routes、headphone-cap-sel。
- sound/soc/codecs/mt8167-codec-utils.c: stereo DC latch、VCM↔DAC / DAC↔OP ramps、A/B/C measurement、gather_comp_val。
- sound/soc/codecs/mt6392-codec.c: Int Spk Amp/Class-D。
- sound/soc/mediatek/mt8167/mt8167_evb.c: AU_HPL/AU_HPRとAU_LOLのboard routes。
- Microsoft標準graph/KS propertyは既存JACK-SENSE.mdの一次仕様を継続。
