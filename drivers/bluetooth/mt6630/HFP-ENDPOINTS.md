# MT6630 Windows HFP bypass / KS endpoint 接続契約

この単位は BT 固有の host source。`hfp_endpoint.c` が Windows HFP の固定
IOCTL と既存 `sideband.c` の実 MRG core を接続し、`ks_endpoints.cpp` が
speaker/render と mic/capture の独立 WaveRT miniport を提供する。
`hfp_topology.cpp` / `hfp_pnp.cpp` は実topologyとPnP登録・撤去処理を提供する。
**Audio v11 の実 providerとAudio FDO呼出し元は未接続。音声動作成功ではない。**
`windows_driver.c` の BTHX SCO 能力は None/0 のまま。Windows 8.1 標準 stack
の必須 HCIBypass 契約を満たした候補として扱わない。既存 02e3915 raw HCI
候補／7999b7b 親／署名手順は変更しない。BT/WLAN は排他 profile を維持する。

## Microsoft 一次契約

確認日: 2026-11-07。実装文のコピーではなく、公開 DDI/形式/手順に基づく。

- [Microsoft Sysvad BthhfpDevice.cpp](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/BthhfpDevice.cpp)
  の `StreamStart`, `StreamSuspend`, `GetBthHfpCodecId`,
  `EnableBthHfpStreamStatusNotification`。
- [Microsoft narrowband speaker table](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/bthhfpspeakerwavtable.h)、
  [mic table](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/bthhfpmicwavtable.h)。
- [bthhfpddi DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthhfpddi/)、
  [WaveRT stream DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nn-portcls-iminiportwavertstream)。
  WinBlue の `GET_CODEC_ID` を使う。Win10 限定の audio-capabilities indication は使わない。

`GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASS` の実到着通知を受け、
現在その GUID に列挙される link のみ kernel 内で attach する。
ユーザーから path/任意 HCI/IOCTL/firmware を受ける broker はない。
`IoRegisterPlugPlayNotification` の callback は PASSIVE work へ到着／撤去を
転送し、callback 内で blocking attach/detach しない。

最初の KS RUN で実 connection status を取得し `STREAM_OPEN`、
`GET_CODEC_ID(version=1)`、実 PCM 整合検証、非同期 status watch、
MRG prepare/start、方向別 client DMA 接続を行う。Windows bypass codec **0** が
CVSD。無線 HFP codec ID 1 と混同しない。8 kHz mono S16_LE だけを受理する。
`STATUS_DEVICE_BUSY` は他 owner の可能性があるため成功へ変換しない。
二方向目は同じ SCO/MRG の参照を共有し、自方向 ring だけを接続する。
PAUSE/STOP は自方向を drain し、最後の RUN 終了で MRG 全体を drain/復元して
watch を cancel、`STREAM_CLOSE` する。pin 作成だけでは SCO を開かない。

固定 METHOD_NEITHER 要求は IRP と nonpaged buffer/file を completion まで保持。
待機 timeout は 2 秒、cancel 後も遅延 completion が触る領域を解放しない。
状態不明の timeout は sticky quarantine。通知失敗は実 owner の passive rundown
へ転送する。時計や tone による DMA position／完了の模擬値はない。

## v11 Audio / IRQ / DMA owner に要求する具体 interface

`hfp_endpoint.h` の `a04_bt_hfp_provider` version 2 を **BT 専用契約**として
定義した。`A04_SHARED_AUDIO_INTERFACE` version 11 の構造を変更／cast しない。
既存 v11 は analog codec/DL1、48 kHz playback、masked IRQ と観測を提供するが、
MRG duplex/DAI capture、方向別 ring rundown、paired combo ownership は提供しない。
`AcquireMuted` や `PreparePlayback` 成功を BT lease と読み替えない。

| callback | owner が実装する動作 |
| --- | --- |
| reference/dereference | Audio/BT 両 context、コード module と mapping の寿命を固定 |
| match_hfp_target (v2追加) | 実HFP stackと保持中のMT6630 controllerのPnP親子関係を照合。GUID/名称一致だけでは許可しない。無関係radioはSTATUS_NOT_SUPPORTED、owner故障にしない |
| validate_pcm | 実 GORM/FC72/controller PCM と CVSD0/8k/mono/16bit、master-clock 関係の整合を確認。checksum/version reply だけで通さない |
| sideband.acquire/live | combo と Audio の世代付き排他 lease。GPIO36..39 function1、MRG/DAIBT、O02/I02/O11、clock、WMT DAI_PAD ACK/GORM 初期化を所有 |
| sideband.read_afe/update_afe/pins/delay_us | 所有期間の masked read/write、実 pin snapshot の復元と実 100us 待機。MRG core が四 register を保存／復元 |
| sideband.prepare_dma/start_dma | 実 I05 playback と O11/DAI capture の duplex DMA/IRQ2/cache を構成・起動。最初は client ring 二方向とも inactive |
| allocate/buffer_live | client と同じ実 DMA MDL、非零 generation、MmNonCached。専有する全 page を公開前に zero、page aligned/page multiple、MDL byte count と一致。最大64KiB |
| set_pin_active | RUN で自方向 ring へ接続。FALSE は自方向 DMA/IRQ/cache access の drain まで完了し、相手の ring/clock を保つ。inactive は実 silence/discard buffer で clock を保つ |
| free_drained | 相手 RUN 中も指定 MDL だけの独立 drain を証明して解放。失敗なら MDL/context/lease を保持 |
| position | 実 hardware byte offsets を ring 内・2byte frame 境界で返す。IRQ counter/timer の推定だけで進めない |
| device_description/latency | 実 DMA device description と FIFO bytes／chipset・codec 100ns delay を返す。未確認値を成功で返さない |
| sideband.stop_dma_drained/release_quiescent | 全 bus access/IRQ を独立に drain、MRG 復元後に lease 解放。disable write だけでは証明にならない |
| fault/retire | fault は <=DISPATCH から実 passive rundown を queue。retire は module/context を固定し rundown 後の cleanup を一度だけ非同期実行。inline 実行や失敗再試行 loop 禁止 |

reference/dereference/buffer_live/position/fault は <=DISPATCH、他 callback は
PASSIVE。position/buffer_live は spinlock 下で実行されるので wait/HCI 要求は禁止。
所有済み mapping の bounded MMIO position read は provider 側で行う。
Audio adapter は state/allocate/free 呼出しを各 stream 内で直列化する。
GetPosition は独立 gate により解放中の MDL に触れない。
失敗 teardown では所有が不明な object/MDL/module を保持し、自動 retry しない。
実 provider がない間は factory を PnP へ接続しない。常時拒否 provider stub はない。

## Audio adapter 側の接続順序と残依存

1. 実provider v2を構成し、AudioのPortCls FDOに4 subdevice分の余裕を予約する。
   既存endpointの枠を減らさない。`hfp-interfaces.inf.inc` の固定reference nameと
   interface登録を、Audio owner側の署名INFへ統合する。本変更は既存INFを編集しない。
2. ownerのPnP/D0開始で `Mt6630HfpPnpStart(audioFdo, resourceList, &provider, &handle)`。
   成功は通知登録だけを意味する。HCIBypass能力やHFP接続成功ではない。
   実到着をBT専用workerへFIFOで渡し、実descriptor2のcategory/containerを取得する。
   FriendlyNameのpointerは参照・保持・ログしない。固定表示名は実IC MT6630。
3. workerはspeaker/micのtopology/WaveRTを `PcNewPort` / `Init` /
   `PcRegisterSubdevice` で登録し、speaker Wave bridge1→Topology input0、
   mic Topology output1→Wave bridge1を `PcRegisterPhysicalConnection` で結ぶ。
   部分登録失敗でもobjectを保持し、実unregisterの完了なしに解放しない。
4. topologyは実category、`JACK_CONTAINERID` と都度実HFP照会する
   `JACK_DESCRIPTION.IsConnected` を返す。connection変更通知を2 endpointへ転送し、
   `JACK_DESCRIPTION2` のpresence detectionとjack-info-change eventを接続する。
   SupportsVolume機には実HFP取得のrange/stepによるvolume nodeを追加し、speaker/mic
   GET/SETとvolume-change eventを転送する。非対応機はvolume nodeを追加しない。
   未実装のmute、NREC、dynamic format、connect/disconnect propertyは広告しない。
5. ownerのD0停止/REMOVE前に `Mt6630HfpPnpStop(handle)` をPASSIVEで呼ぶ。
   通知解除をdrainし、PnP workerを最大10秒待ち、topology observerをrundown、
   HFP変更通知IRPをcancel/completion join（各2秒）し、sessionの新RUN受付を閉じて実MRGを
   drain、physical connection/subdeviceを逆順に撤去する。成功時だけhandleを破棄。
   ただし既存KS handleの最終Releaseは後発し得る。sessionがAudio FDOを参照保持し、
   providerの全referenceが解放されるまでmodule/context/必要mappingを保持する。
   Stop成功だけを全client reference消滅とみなさない。
   失敗時はFDO/provider/resource/moduleを保持し実owner rundownへ。自動再試行なし。
   Start/StopとownerのPnP/power操作は直列化し、faultからinlineでStopしない。
6. profileは1 remote HFP pairのみ。無関係radioはproviderが拒否し、保持中のpairが
   ある間の別arrivalは採用しない。全系故障時はworkerの新しいhardware操作を止める。
   callback/workerの未処理linkはruntimeだけ保持し、解放前にzeroする。
7. 実owner/codec/PCM/DMAとAudio FDO呼出し元が揃った固定構成で初めて
   HCIBypass能力の判断を更新する。能力判定にactive HFP linkを必須とすると
   列挙前に循環するので、backend所有可能性と個別RUNの検証は分ける。

**主担当へ必要な共有側ABI/変更**: `hfp_endpoint.h` provider v2の全callbackを実ownerで
供給（特にMRG/DAI capture/IRQ2と方向別drain、match_hfp_target）、4 PortCls枠、
Audio FDOのStart/Stop呼出し、INF fragment統合、最終driver linkにportcls.libを追加。
BT WDF FDOをPortClsとして扱わない。親／Audio共有ファイルは本変更で編集しない。
MRGの電気的連続性、controller PCM word formatの実設定、DAI capture DMAとIRQ2の
v11統合、標準stack列挙、音声入出力、電源遷移は未確認。
Linux/boardのregister根拠は [SIDEBAND.md](SIDEBAND.md) を参照。

## volume同期・jack通知

Microsoftの[GET_VOLUMEPROPERTYVALUES仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthhfpddi/ni-bthhfpddi-ioctl_bthhfp_device_get_volumepropertyvalues)
は、返却するKSPROPERTY_VALUESとそれが参照するデータを同一buffer内に置く。
本実装はIRP-owned buffer内でpointer/長さ/型を検証し、LONGの単一stepped rangeを
取り出してBASICSUPPORTへ直列化する。外部pointerをuserへ返さず、0～15やdB換算を
埋め込まない。未知のrange形式は明示的に未対応とする。
GET_VOLUME_STATUS_UPDATEはbImmediate=TRUE、変更通知はFALSE。SET成功だけで
cached値やeventを捏造せず、実GET／実notification completionの値を採用する。

connection、speaker volume、mic volumeの3 IRP（volume非対応機はconnectionのみ）を
1 workerで処理・再armする。real値の変更時だけIPortEventsへ通知。notification失敗や
異常な連続completionは停止してowner faultへ送り、自動再試行loopにしない。
observerはrundown保護し、PortCls unregister前に通知callbackをdrainする。
キャンセル未完了時はIRP/buffer/engine/session/moduleを保持し、解除成功と扱わない。
PortClsがevent entryの解除を所有する。HCIBypass/MaxScoChannels広告は従来通りNone/0。

provider **v2の構造は変更しない**。BT-private metadata/observer APIのみ追加。
Ramanは同じ固定sourceのheader/libraryをまとめて統合する。topology factoryを直接
使う場合はsession events開始と`Mt6630StopHfpTopology`が必要だが、通常の
`Mt6630HfpPnpStart/Stop`内では実装済み。新しいAudio volume callbackは不要。
provider用Linux根拠・IRQ2共有の注意は [PROVIDER-EVIDENCE.md](PROVIDER-EVIDENCE.md)。

## 固定 source のビルド単位

依存: Windows WDK8.1 ARM headers（portcls/ks/ksmedia/bthhfpddi を含む）、
LLVM clang/clang++ と lld-link、PowerShell、git。runner/実機はこの helper から呼ばない。

```powershell
# clean checkout の固定 commit から。全出力/tmp は checkout 内。
powershell -NoProfile -File tools\bluetooth\build-mt6630-hfp-endpoint-core.ps1
```

`build/mt6630-hfp-endpoint-core/mt6630-hfp-endpoint-core.lib` と source/hash manifest
を生成する。SYS/INF/CAT/新 probe/音声 client を再 build しない。Audio adapter 側で
library を明示 link し、owner provider と Audio FDO の Start/Stop 呼出しを実装する必要がある。
最終driverは ntoskrnl/hal/portcls の kernel import を使う。独自 CRT/例外/RTTI は不要。
既存 source から同じ sideband object を最終 image に二重 link しない。

host での WDK10 公開参照 header・WinBlue guards による ARM32 compile/archive は
実 WDK8.1 の最終 driver link、標準 stack 接続や実機 SCO 成功と区別する。


## bf989後のBT専用継続checkpoint

Audio側はRamanの`4b2a461`（owner/common）と`a94ee27`（Stop/最終解放分離）を
読み取り照合した。共有Audio/親ファイルは本変更で編集せず、provider v2の構造/offsetと
`Mt6630HfpPnpStart/Stop`契約を維持する。HCIBypass/MaxScoChannelsはNone/0のまま。
凍結済みSD/USB署名tupleとrun42、eMMC/EDKは変更しない。

- WaveRT streamの正常な最終ReleaseがDISPATCHで来ても、偽のDMA faultにしない。
  BT-private cleanup workへ1回だけ転送し、session/FDO/providerをcallback完了まで固定。
  PASSIVEで自方向の実free_drainedと必要なSCO suspendを行い、相手RUNを妨げない。
  実drain失敗だけを既存fault/retire経路へ渡す。queue失敗はobject/MDLを保持し、
  自動retry loopや不明DMA解放をしない。通常解放とowner rundown後のretireを混同しない。
- sideband Acquireの`-1`かつtoken0は契約上の無副作用拒否。watch/STREAM_CLOSEを実完了
  してからNOT_READYを返し、session全体へsticky faultを捏造しない。
  次の明示RUNで再取得できる。`-1`でもtoken非0ならFAULT/保持とし、再取得しない。
- [KSNODEPROPERTY_AUDIO_CHANNEL](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-ksnodeproperty_audio_channel)
  のChannel=-1は全stream指定。本mono endpointではchannel0と同じ実volumeへ適用する。
- [KSPROPERTY_AUDIO_VOLUMELEVEL](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/ksproperty-audio-volumelevel)
  は範囲外SETを単に拒否する契約ではない。実HFPが返したrange/stepへclampし、
  stepに沿う下側の値をSETする。GET/通知が実target値を返し、SET受付だけでcache/eventを
  生成しない。固定0..15や架空のdB換算は使用しない。
- [Exposing Multichannel Nodes](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/exposing-multichannel-nodes)
  が要求するBASICSUPPORT_MULTICHANNELを明示し、MembersCount=1の実mono rangeを返す。
- 全5 TUへNoFP/NEONの4flagsを同一適用し、source/header hashesとfinal link依存を
  archive manifestへ記録する。正規libcntpr/portclsを使い、SDK改変やCRT stubを足さない。

### Ramanへのsource統合要点

Ramanの`build-hfp-portcls.ps1`は現在BT依存を63c32へ固定している。
新BT commitの`drivers/bluetooth/mt6630`を**全体**でarchive/include/linkへ切替える必要がある。
追加`hfp_volume.h`とBT-private `a04_bt_hfp_defer_cleanup`を含む同じ5 TUをまとめて使い、
63/bf989のobjectや別sideband objectを混在・二重linkしない。Audio/shared親コードの
置換はしない。provider v2自体の変更や新しいAudio callback追加は不要。

残依存は実BT controller登録/到着順、FC72 PCM/master-slaveの実設定整合、独立DMA drain、
実codec/FIFO latency。`4b2a461`はVerifyDmaQuiescent不在でfactory公開前に拒否しており、
本BT変更はその資格を証明しない。登録やlatencyを常時成功/ゼロstubで補わない。
標準Bluetooth列挙、SCO duplex、音声、電源遷移の実機成功は依然未確認。


### 切断後の既存KS handle

PnP detachでsessionが停止してもPortClsが保持するstream objectの最終Releaseは後発する。
その間に同じRUNを要求された場合も、`state==RUN`だけで成功せずsession status、
readableな実buffer、providerの実generationを確認する。STOP/PAUSEはcleanupを試みられる
経路を残し、実suspend/drain失敗を伝播する。切断後の新SetFormatも成功化しない。
これはhost sourceの受付契約修正であり、BT controller登録やSCO音声の成功ではない。

### 版予約の訂正（main central表）

作業ルート`port/gsi/logs/ios-feasibility/windows-rt-parent-version-reservations.json`を参照。
1008はecaa固定署名親、1009は廃止された旧thermal予約で新規署名tuple作成禁止。
1010は凍結SD署名tuple、1012はCarver thermal/BQ、1014はGPU stop/power owner予約。
旧署名package内BUILD-HANDOFFの1009表記を現行予約と解釈せず、この訂正とcentral表を優先。
凍結ZIP/同梱文書は書き換えない。CarverのSD-only ACPI/boot作業も別工程、RTCを混ぜない。

Raman HFP checkpointの親1007は歴史的source profileであり、現行tupleの版予約ではない。
新HFP SYS/親/Audioの署名tupleを作る際はmainの新予約と全flags/sourceを固定する必要がある。
本BT library/source作業では親INFを変更・予約せず、1007/1008/1009/1010/1012/1014の
別SYS再包装を行わない。SD/USB署名ZIPは凍結したまま。


## HFP service-level 接続要求と今回の候補

両topology filterへ `KSPROPSETID_BtAudio` の
`KSPROPERTY_ONESHOT_RECONNECT` / `KSPROPERTY_ONESHOT_DISCONNECT` を追加した。
Microsoft Sysvad の `PropertyHandler_BthHfpOneShotReconnect` /
`PropertyHandler_BthHfpOneDisconnect` と同じ GET action であり、SETではない。
BASICSUPPORTだけではI/Oを発行しない。実GETでは、すでにproviderが物理照合済みの
session targetへ、入力・出力0の固定 `IOCTL_BTHHFP_DEVICE_REQUEST_CONNECT` /
`IOCTL_BTHHFP_DEVICE_REQUEST_DISCONNECT` を一回発行する。
任意opcode/path/addressは受けない。返却statusは要求受付であり、SLC確立ではない。
実接続は既存 `GET_CONNECTION_STATUS_UPDATE` とjackイベントで判定する。
自動再試行・接続成功cache・合成通知・暗黙STREAM_OPENは行わない。

一次source:
- [Sysvad topology handler](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/bthhfptopo.cpp)
- [Sysvad topology property table](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/bthhfpspeakertoptable.h)
- [Sysvad SetBthHfpConnect/SetBthHfpDisconnect](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/BthhfpDevice.cpp)
- WDK8.1 `km/bthhfpddi.h`, `shared/ksmedia.h` を実buildで確認する。

raw HCI transportは実monotonic時計（Windows `KeQueryInterruptTime`）を必須入力とした。
CHLPCR所有権600ms / TX drain1s / WMT・HCI応答2sは、poll回数だけでなく
SDIO処理中の経過時間も確認する。下位SDIO request自身の有限timeoutはそのまま維持する。
これは単一D0全体のdeadlineではない。最初のtransport failureを初期化失敗番号で
上書きせず、実timeoutは `STATUS_IO_TIMEOUT` でD0Entryへ返す。

raw transport INFは `11/06/2026,2026.11.7.2003`。旧2002の別SYSを生成しない。
全BT/common TUはNoFP/NEON flags、stack protector無効、WDK8.1宣言aliasを揃え、
正規WDK `libcntpr.lib` をlinkする。WiFiの共通helperは非編集。
HFP coreは別archiveであり、このraw SYSへendpointを二重linkしない。
HCIBypass None/0は維持。raw候補を標準stack/HFP動作候補とは呼ばない。

Raman/main向けの残依存と固定build手順は [HFP-INTEGRATION-HANDOFF.md](HFP-INTEGRATION-HANDOFF.md)。


### 実WDK8.1で確認したcodec DDIの訂正

旧checkpointは新しいheaderのWinBlue guardを根拠に `GET_CODEC_ID` を必須化していたが、
実WDK8.1には型/IOCTLがない。Microsoft公式の対応開始もWindows 10。
その要求をWin8.1 sourceから除去し、本機Windows RT8.1の狭帯域CVSD profile
（provider引数0/8000/1/16）へ直接接続した。0は要求profileであり、受信codec応答ではない。
実FC72/PCM/clock照合はproviderの必須条件として残し、未知設定で成功させない。
WBS/mSBCを広告しない。これより上の旧記述にある「codec ID query必須」は撤回する。

- [GET_CODEC_IDの公式対応OS・入出力](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthhfpddi/ni-bthhfpddi-ioctl_bthhfp_device_get_codec_id)
- [ONESHOT_RECONNECTの受付と実接続の区別](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/ksproperty-oneshot-reconnect)
- [REQUEST_CONNECTの固定入出力・Windows 8対応](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthhfpddi/ni-bthhfpddi-ioctl_bthhfp_device_request_connect)
- [Sysvad: WBS非対応stackでCVSDを用いる経路](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/BthhfpDevice.cpp)
