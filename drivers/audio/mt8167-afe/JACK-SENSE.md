# MT8167 AFE / MT6392: ACCDET1102 jack sense

Audio1026 (2d623172) のspeaker pin1・0→1 connection、capture、PCM/DMA/IRQ/clock/power契約を保持し、既定OFFの `A04_AUDIO_ACCDET_JACK` を追加する。署名1027候補では1。既存の親ACCDET1102 ABI (16byte/schema1、GUID42d17690-b0a3-4e17-a921-8d069cc87731、IOCTL0x00226554) のconsumerだけであり、親/RTC/thermal/USBを変更・再buildしない。

## 標準KS API

再生 `Mt8167Topology` にsense-only `KSNODETYPE_ANALOG_CONNECTOR` bridge pin2を追加し、filter automation tableから `KSPROPSETID_Jack` の `KSPROPERTY_JACK_DESCRIPTION` / `KSPROPERTY_JACK_DESCRIPTION2` GET/BASICSUPPORTを提供する。KSP_PIN.PinId=2でアクセスする。pin0/1は空のKSMULTIPLE_ITEM、範囲外pin/SET/不正instanceを拒否する。size query/不足bufferは親を読まない。実際のGETだけ資格付き親sampleを要求する。

親成功でもsize/version/qualified/insertedを再検査し、不在/未資格/D0閉鎖/poison/removed/BUSY/timeout/不正ABIをConnected=falseへ置換しない。失敗は成功payloadなしで返す。成功時だけIsConnectedにsample.insertedを反映する。DESCRIPTION2はpresence detect bitのみ、DeviceStateInfo=0。未知の色・端子寸法・位置・channel/mic種別は推定しない。

外部jackの信号routeはまだ根拠不足のため、このpinをspeaker/captureやWaveRTへ接続しない。既存speakerをheadphoneへ改名せず、EP association/PCM format/headphone endpointを追加しない。本候補は標準KS経由の実機械socket senseの接続であり、headphone再生やWindowsの自動endpoint切替の完成ではない。KSEVENT_PINCAPS_JACKINFOCHANGE/安定debounce/抜挿し通知は提供しない。presence能力は明示GETで得る瞬時sampleを指す。

## lifetime / power

PASSIVE_LEVELのGETだけでenabled親interfaceが正確に1つのとき開く。任意path/GPIO/registerは不可。親WDFの既存D0 power-managed admissionとlock/二重readを使い、GPIO/PMIC/AFE writeなし。独立mapping/owner/worker/timerなし。

下位IRP・output・eventはnonpaged heapに置き、caller/completionの2refで所有する。100msの待機期限後はcancelしてcallerだけを返し、遅延completionが残りを回収する。IRP解放前までcaller refを保持するためcancel/完了競合でfree済IRPを使わない。IoSetCompletionRoutineExはaudio側DeviceObjectを指定しcallbackのunloadを防ぐ。file refは遅延completionまで保持し、DPC時の削除をdeferする。全driverで未完要求1本に制限し、応答しない親に対する資源増殖を防ぐ。親が永久に完了しない場合はその1本とdriver lifetimeを保持してfail-closedにする。完了前に資源を強制解放しない。audioのstream/owner lockは保持しない。PortCls teardown後のaudio miniport pointer/stack outputを遅延completionから参照しない。

## 再現・採用

`tools/ci/build-mt8167-afe-jack1027.ps1` に回収済WDK8.1 KitDirectory、portcls.lib、ARM user libs、既存signtool/Inf2Cat、checkout内の新OutputDirectoryを渡す。同じSignerBCBF6C456C6F0DD0818434763B189488FBF13570でSYS/EXE/CATを署名しCAT所属とARMNTを検証する。完全一致のaudio1026 object cacheが無いhostでは小audio closure23TUだけcompileする。SDK/親/GPUの全buildは行わない。

主担当だけが将来の別一括更新で採用する。凍結bundle122を変更しない。必要な親はACCDET1102を統合済みのcurrent親（GPIO read ABI1/16byteとflag1）。古い親1101へ巻き戻さない。audio1026の同一flags/復旧packageを保持し、1027 INF/SYS/CATの組を固定する。親が無い旧構成でも既存speaker/captureの開始を妨げず、新propertyだけNOT_READYとする。

署名client `mt8167-afe-jack-read.exe --read` は起動済MT8167\\AFEのMt8167Topologyだけを選択し、pin2 DESCRIPTION/DESCRIPTION2を各1回GETする。stdoutへsample/errorだけ出す。stream/SET/物理I/O試験/反復probe/任意device指定はない。sizeと能力を検査する。ユーザー就寝中に物理操作は依頼しない。資格不足を未知register writeで解消しない。

host fixture: `g++ -std=c++14 -fsanitize=address,undefined tests/afe_jack_test.cpp -o <root内出力>`。実property/transport headerを実行しbuffer ABI、ready/busy/timeout、不正sample、IRP即時/遅延/取消、遅延完了後の解放収支を検証する。DDIはmockでありWindows/実GPIO/抜挿し/音声成功の証拠ではない。

## 一次資料

- Microsoft [Jack Description Property](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/jack-description-property): filter property / pin instance / KSMULTIPLE_ITEM / automation table。
- [KSJACK_DESCRIPTION](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/ksjack-description): IsConnectedは検出可能な実挿入状態。接続型/位置enum。
- [KSPROPERTY_JACK_DESCRIPTION2](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/ksproperty-jack-description2): Windows7以降のpin-wise GET、KSMULTIPLE_ITEM+array。
- [IoSetCompletionRoutineEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iosetcompletionroutineex): callbackとDeviceObjectの所有driver一致、登録成功後は必ずIoCallDriver。
- 実GPIO19資格/極性は既存 `drivers/input/mt8167-accdet/README.md` と固定CT3 GPL7c478212のa04br3.dts/accdet_eint_func/pinctrl。ABI/資格を本consumerで変更しない。

この候補の物理動作・Windows実ロード・抜挿し・headphone audioは未検証。主担当の実機確認まで完了扱いしない。

## run133後段：KS登録名の寿命修正1032

Started/problem0/fixed-filter1後のCreateFileW ERROR2はaccess deniedではない。PcRegisterSubdeviceのNameはdevice全寿命の有効性が必要（Microsoft一次仕様 https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-pcregistersubdevice ）。実RT8.1 PortCls REAでも同じName pointerがKsAddObjectCreateItemToDeviceHeaderのObjectClassへ渡る。speaker WaveRT/Topologyのautomatic WCHAR配列をdriver常駐staticへ変更する。capture名は既にstatic。文字列・suffix・AddInterface GUID・既存client・access maskは変えず、KSNAME等を推測appendしない。元productionコードでは関数return後の保持pointer参照がASan stack-use-after-returnとなり、修正版はPASS。run133実kernel内の破損値は未採取なので、実機open成功は未確認。

本候補は搭載1027正本source81f20c785cebf7e7a65778a3b3a10687bb6bcb54へ、この寿命修正とNativeStart/INF版1032のみを適用する。HP1031の追加経路・6枠拡張・親1108/RTC/thermalを含めず、既存speaker/capture単一ownerと4枠・GPIO資格・開始停止を保持する。親と媒体は変更しない。main通常batchで既存署名Mencius client --readを1回、同じCreateFile/accessで観測する。
