## 後段親1107 / audio1030：一回の実測producerと同一ring handoff

前段1105/1106で不足していた実行経路を追加。標準NewStreamでは実senseとownerのみを確認し、校正前にbuffer取得へ到達できる。PortClsへMDLを返す前のprivate checkoutで最小48KiBの同一Normal-NC/WC identity-mapped PCM ringをゼロ化し、初回だけ44.1kHz無音DMAを実startする。親は同じthermal V3 ownerのAUXADC leaseを取得し、CT3 codec-utilsのdepop設定／A待機50ms／DAC enableと22uFのsettle20ms／gain0のC待機50msを実行。左右各phase22sampleを実読してtrimmeanと係数を生成する。CUR進行、BASE/END、DL1/IRQと期限を実検査し、caller係数やphaseは受け付けない。

子の実DL1 stop・IRQ/DPC rundown→親のstop実観測→analog所有mask復元→48kHz digital rateへの変更→AUXADC終了→校正validという順序。MDL/SG/BASE/END/clockは変更・解放しない。bufferを標準PCM clientへ返し、次RUNは同じringでHP start-muted→retained DMA restart→resumeへ進む。校正失敗や停止不確実はownerをpoison/retainし、その起動で再校正しない。speaker/captureが先にexposeしたringは奪わない。

親HP ABIはversion2/build1107。thermal V3 build1104、audio v11は維持。既存1104/媒体/EDKに変更なし。親INF1107とaudio INF/NativeStart1030は一致させる。校正中はanalog操作が新規に行われるため、署名物だけを実機再生成功とは扱わない。

**latencyとdrainの意味を限定する。** 実測producerは7個のKeQueryInterruptTime時刻とraw/係数/同一ring/stopを400byte診断へ返す。これは起動・校正stageの経過時間で、標準KSRTAUDIO_HWLATENCYのFIFO/ChipsetDelay/CodecDelayではない。既存captureと同じadapter gateが標準HWLATENCY queryをNOT_SUPPORTEDで拒否し、未知sentinelでPCMを妨げない。標準latency値を合成しない。fabric outstandingの完全drain証明は依然無いので、同一ringをpinしたretained handoffだけを許可し、free/clock gate/別backingへの変更へは使わない。

`mt8167-hp-adc-read.exe --calibration-status` は既存ACCDET interfaceの固定read-only IOCTL0x00226560でreportを1回返す。`--raw` は既存単発ADC観測でphase UNKNOWNのまま。statusを読むだけでは校正は実行されず、通常の標準HP buffer checkoutがproducerを実行する。依存未充足時の同じProbe反復を指示しない。

source根拠: CT3 GPL7c478212 `mt8167-codec-utils.c` のcali_setup/enable/disable/get_hp_cali_val、`mt8167-codec.c` hp_depop_setupとHP DC Offsets、`mt8167-afe-pcm.c` trigger STOP。共通codec_domainの供給bitを保存しLinuxcleanupのglobal clock/power offやdma_free_coherentをコピーしない。XNU側に同じMT8167 codecの独立実装は確認できず、XNUの動作を根拠に遅延やdrainを認定しない。

再現script `build-mt8167-hp-calibrate1107.ps1` は親1TUとABI依存C++16TU/client1TUをcompileし、変わらない親13helper/AFE C8をcache照合して再利用。新しいmockを細分化せず既存production parent統合fixtureへ一回のA/B/C→STOP→48k同一ring遷移と稼働中finish拒否を追加した。mock ADC値は実測証拠ではない。

## 後段親1106: STOP時の親側観測

1105/audio1029のbackend ABIを維持したまま、親STOPでDL1 (`AFE_DAC_CON0` bit1) とIRQ1 (`IRQ_MCU_CON` bit0 / `IRQ_STATUS` bit0) の無効・未pendingを二度実読確認する。根拠はCT3 `mt8167_afe_dais_trigger` STOPと既存Windows `mt8167_dl1_stop`。親は追加のDMA/IRQ register writeを行わず、子のISR/DPC rundown責務も引き取らない。全1読取りや有効bit観測時はHP clamp/lease保持とエラーを返し、typed RETAINED STOP成功を合成しない。これはfabric drain証明ではない。

HP analog callbackは既存digitalPlayback prepared/retainedと共通TOP/AFE clock leaseも要求する。HP extension ABI build1105、thermal V3 build1104、audio1029は不変。新INF1106のみ。`build-mt8167-hp-stop1106.ps1` で親1TUだけbuild/signし、13helperを固定cacheから検証再利用する。既存1105署名物・1104/freeze・thermal・audioは再buildしない。校正producer/latency不足によるHP PCM拒否は継続。

# MT8167 headphone shared-owner backend（後段1105 / audio1029）

親1104 `97b87a4b0d226eb093507cd5b886acfdbf2c0391` を基点に、AFE1027とHP部品d54fを合流した後段候補。freeze中の親1104/thermal1.8 package、共通EDK、実機は変更しない。以下はsource実装であり、実測校正・実HP再生成功は未確認。

## 今回接続した実経路

- 既存親 `c->thermal` / `thermalPioMappings` / V3 boot proofを共有するAUXADC lease。thermalとHPは同じadmission gateで排他。別owner、別ADC resource、子mappingなし。consume済みboot proofは親内だけでcacheし、取得・終了時はlive clock/reset/master/ADC idleを検証する。timeout・不確実状態・removeはmapping/D0/objectを保持する。
- AUXADC7/8を各22sampleだけ取得する固定IOCTL `0x0022655c`。既存ACCDET interface/queueを利用し、72byte sample / 160byte pairでgeneration、parent NTSTATUS、valid、raw12、nominal averageを返す。`mt8167-hp-adc-read.exe --raw` の1回だけで、phase/係数/任意register指定を受け付けない。
- 既存 `Mt8167PnpBackend` の同一mutex/DL1/MDL/SG/rundownへ標準HP providerを接続。別名WaveRT/topologyを実factoryで登録し、開始時は親analog start-muted→既存DMA開始→実親resume、停止時はposition rundown→clamp→既存DL1/IRQ停止→親typed drainへ接続する。
- speaker/HP切替えは既存exposed retained routeを保持中なら、IRQ接続・allocation・buffer zero・route変更より前にBUSY。未証明drainではpages/SG/D0も解放しない。captureと既存audio v11は維持。
- GPIO19資格が成立した実senseのみKS jack propertyへ返す。不明をConnected固定にしない。GPIO writeは追加しない。

## 残る実機／source条件

raw observationはCT3の44.1k無音DMAと制御されたA/B/C測定sequenceの実行証明ではない。レジスタ形だけでphaseを認定せず、現APIは常にphase UNKNOWNを返す。係数を入力・合成するAPIは無く、`hpCalibrationValid` は取得できていない。無音calibration producer、測定に基づくHP latencyは未接続。HP stream生成はこの不足を明示して拒否する。既存speakerのzero latencyをHPへ流用しない。

既存 `ParentVerifyDmaDrained` はfabricのoutstandingを証明できず拒否する。その実callbackを使い、retained STOPとfull RELEASEDを区別する。DL1 disableだけをfreeの証拠にしない。無音calibration DMAから別PCM bufferへの安全な切替えもこの停止保証を必要とするため、bufferの差替え/freeやdrain値を合成して迂回しない。

したがって、今回の署名物も通常一括配備／HP再生用の完成候補ではない。親共有ADC返却と標準backendの接続を持つ後段候補であり、実機ownerがfreeze後に依存を評価する。実HW未検証。

## 互換性・再現・検証

親INFは1105、AFE INF/NativeStartは1029、HP extension version1/build1105。thermal V3のGUID/size/build1104は維持し、thermal1.8を再buildしない。

`tools/ci/build-mt8167-hp-shared1105.ps1` は固定親helper cache1d248a71の13objectとAFE1027 cache81f20c78のC8objectをsource/quoted include closureとhash照合して再利用する。private C++ backend layoutの変更により16 C++ TU、親1TU、固定raw client1TUだけ再compile。同BCBF identityでSYS/EXE/CATを署名し、ARMNT/CAT所属/署名hashを固定する。SDKの再構築はしない。

新しい寿命境界だけを `tests/hp_parent_lifetime_test.c` と `tests/hp_backend_lifetime_test.py` で検証する。前者はproduction parent include/ADC captureそのもの、後者はproduction backend関数を抽出してOS/DMAのみmockする。ASan/UBSanでclock/reset/master喪失時retain、排他、stale generation、raw UNKNOWN、start-muted、retained STOP、route切替え拒否を確認。校正fixtureはmock専用で、実測／実機成功とは報告しない。旧815 fault反復は実行しない。

## 以前の部品d54fの記録（当時の状態）

# MT8167 headphone route components (not deployment-ready)

CT3 GPL `7c4782127d4ea11965be00ce1343056be9bfb769` の一次sourceを根拠とする独立実装。MT6392は内蔵Class-D speakerであり、HP出力はMT8167内蔵codec。`mt8167-codec.c` のAIF RX→Left/Right DAC→Left/Right Audio Amp→HP Depop VCM→HPOUT Mux→AU_HPL/AU_HPRと、AU_LOL→MT6392 Int Spk Ampを混同しない。

## 実装した部品

`drivers/bus/mt8167-shared/codec_headphone.{c,h}` は既存単一親の排他audio leaseから呼ぶportable transaction。common domain/実speaker mute/実測generation/期限/資源をqualified callbackで要求し、voice/DAC/他DL activeを事前拒否する。GPIO write、独立mapping/owner、MT6392 unmute、thermal/RTC/USB制御は追加しない。

左右の実A/B/C AUXADC平均から選択gainの補償値を変換するpure関数、ステレオDC更新、cap/gain対応の2段ramp、HP depop、左右DAC/amp、所有maskのみのreadback・保存値復元を実装する。`0x4b4b`はCT3 gather_comp_valの明示DAC-stage値で、未知係数のfallbackではない。両channelの測定完了・generationなしでは生成/出力を拒否する。測定を行うprovider自体は含まれず、入力データを実測と合成しない。

I/O bank0=親APMIXED、bank1=親がbind済みのAFE。start後の失敗はpoison/heldを残し、blind rollbackしない。stopは独立のDMA/IRQ drain証明を必須とし、active bit消失だけでdrainとみなさない。既存capture/共通reference bitsはmask外として保存する。DC制御はhandshakeであり、古いcontrol raw値を書き戻さず保存coefficientsを再latchする。ownerがleaseを解除するのは正常stop後だけ。

`headphone_endpoint.{h,cpp}` は実HP backendが渡された場合だけ使える標準PortCls factory。別名Mt8167HeadphoneWaveRT/Topology、実HP WaveRT miniportを提供する必須factory callback、WaveRT pin1→topology pin0→KSNODETYPE_HEADPHONES pin1の接続。既存speaker/captureやsense-only pin2を改名/接続しない。DESCRIPTION/DESCRIPTION2はqualify済の実senseだけを返す。未知状態をConnectedにせず、イベント/動的format/不明latencyは追加しない。

backendのreference/dereference、真のHP WaveRT format/DMA/latency、再生stream制御、既存DL1のspeaker/HP排他、route選択、PnP/power revoke、DMA保持/restore契約を実ownerが実装してからStartDeviceでfactoryを呼ぶ。部分登録後のfailureはPortClsの通常StartDevice unwindを要求する。providerが無いのに成功するstub、synthetic backend、固定Connectedは無い。

## 現在の阻害条件（未解決）

現親1103にはHP A/B/C測定を供給する共有AUXADC lease/clock/deadlineとmeasurement routeが無い。既存AUXADC channel11はthermal ownerの資源/寿命契約で、HP channel7/8として流用/再mapしない。既存speaker WaveRTのzero latencyをHPの実測値へ流用しない。実HP WaveRT factoryも未供給。既存audio v11はspeaker/voiceを前提とし、HPのmode切替・drain/restore providerでもない。HP DC OffsetsはLinuxで実A/B/C変換とgain別測定を行う。cold zero配列や保存係数を測定値へ偽装しない。

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
