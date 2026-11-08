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

## run129後段client修正

実機fixed-filter1はstatic topology interfaceの検索成功であり、続くERROR490は旧clientがCreateFileWの全失敗をERROR_NOT_FOUNDへ置き換えていたため原因を示さない。親1104/AFE1027 NativeStart完了を未開始扱いしない。新clientは唯一のMT8167 AFE・末尾reference Mt8167Topologyを正確一致させ、選択INF/versionとinterfaceをbounded UTF16行で記録し、filter-openの実Win32 errorを保持する。original basenameとoem名はいずれもmetadataとして記録し、INF名でfilterを選択しない。標準pin2 GET2回、任意path/SET/streaming/再試行無し。driver/INFを再buildせず1TU clientだけ署名可能。

GPIO19のrun129実測はDIR0/MODE0/IES0x400/PULL0/SELECT0/DIN8、VALID0xFFF/reasons0x18で、全12readは成功している。現在の契約ではbias資格不足として正しくNOT_READY。CT3 accdet_setup_eintとpinctrl irq_request_resourcesはinput/mode0/IESのみを設定し、内蔵pullを設定しない。しかし外付けbiasの有無はこのsourceと安定DIN8だけでは証明できないので、Connectedを出すためにGPIO writeや資格緩和をしない。HP producerも同じsense資格が成立するまでfail-closed。現1104/媒体/実機へ変更なし。

## run133 filter-open ERROR2とKS登録名の寿命（audio1031）

実run133はStarted/problem0、fixed-filter1に続きCreateFileWがERROR_FILE_NOT_FOUND(2)を返した。generic access C0000000/overlapped40000000であり、access deniedと扱って権限を緩めない。元のMencius client/媒体は変更しない。

Microsoft一次仕様 PcRegisterSubdeviceのNameはdevice objectの全寿命で有効でなければならず、KsAddObjectCreateItemToDeviceHeaderのObjectClassもcreate比較時に参照する。採取RT8.1 portcls.sys（SHA256 f9d0f5ce9d6072785a9cf8d242c3f490e4b2af6455ff05fec6a21815bc3e9db6）のREA PcRegisterSubdeviceと0x2b7c0は、渡された同じName pointerをKS登録へ渡す。IoRegisterDeviceInterfaceには別途UnicodeStringとして渡すため、PnP interface列挙成功はcreate lookup名の後続寿命を証明しない。Evidence ev_e2d462d121589140b58aafa9fc2b0cdd3bf32f9a2d94b5467af6813d26e21d98、ev_af8b918c1e808f45e02303c140a2f8fd18b6202d6b01eddcd4f7087fc10eac9e、命令精度ev_a25afbbbadb0d9a8cc08636e5c1f25b238d9471f74d95690cd79911e183521d8。

実装のspeaker WaveRT/TopologyとHP WaveRT/TopologyがStartDevice内のautomatic WCHAR配列を登録していた。これをdriver常駐のstatic配列へ変更する。文字列/suffix/INF AddInterface名・大文字小文字・GUID・既存client選択・アクセスmaskは変更しない。capture名は既にstaticで維持する。KSNAME_Filter等の別名を推測してappend/置換しない。

さらにHP対応profileではspeaker/capture/HPの3pair=6subdeviceに対しPcAddAdapterDeviceが4枠のままだったため、HP対応時だけ6枠にする。これは親1107以降でHPが利用可能になった場合の実登録経路に必要で、親1104のHP不在時は既存render/captureだけを維持する。audio1031 INF/NativeStart revisionで識別し、親/thermalは再buildしない。

旧production登録codeへ保持pointerをreturn後に読む同じ試験を適用するとASan stack-use-after-returnを再現し、新codeでは成功。speaker/topology/capture/HP全6名のreturn後比較と既存部分登録unwindを試験した。実run133のkernel内Name bytesを観測したわけではないため、その場での破損値・修正後open成功は未確認。mainの通常batchで同じ固定--readを1回観測する。

一次仕様:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-pcregistersubdevice
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ks/nf-ks-ksaddobjectcreateitemtodeviceheader
