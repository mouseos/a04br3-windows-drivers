# MediaTek MT8167 AFE / MT6392 通常Windows音声probe

通常OSのAudioEndpointBuilder / AudioSrv、MMDevice、WASAPI shared modeから
固定PCMを送出する独立コマンド。既存のprivate tone service・直接KS clientの入口は変更しない。
WinPE標準音声サービスなしの構成では使用せず、EnginePcm用の次構成へ分けて組み込む。
ユーザーの手操作、マイク、カメラ、観測USBを必要としない。

## 固定した対象と出自

主担当が回収した作業ルートの
`port/xnu/out/windows-rt-engine-pcm-de214c8-e4dff2b/` を対象とする。

| 対象 | source / 条件 |
| --- | --- |
| parent/audio runtime SYS | de214c8b7495913d0716f149b24fd210ef42de8a |
| 署名時INF source | e4dff2b25cc164892dce1d534cb564218845c29c |
| audio INF DriverVer | 11/06/2026,2026.11.7.1004 |
| signed-manifest audio_mode | engine_pcm_retained |
| audio/mt8167-audio.sys SHA-256 | 2f56e9a4cb742c2d09284117d9e781493d827a2800a7925238bdc352b2b346ef |
| parent/mt8167-shared.sys SHA-256 | 99338c1bbe30205b09af6c594a7c79cc4ef8a7c080a692330d36cdbb973f86ca |

上記は回収済みbundleのmanifestとファイルを読み取り確認した値であり、
このprobeが実機にロードされたSYSのhashを照合したという意味ではない。
primaryの署名・媒体manifestでruntime SYS、INF、probe EXEの出自とhashを別々に記録する。
元のbuild-manifestのdriver_dateはde214c8時点の11/07、署名INFはe4dff2bの11/06であり、
raw sourceの履歴と署名用INF sourceを混同しない。probe-manifestのexpected値も実ロード証拠ではない。
fixed-tone版のparent/audio bundleをEnginePcm版と混ぜない。

## probeだけをbuildする

```powershell
powershell -File tools/ci/build-mt8167-audio-engine-probe.ps1
```

このhelperは専用CのARM32 compile/linkとPE import回収だけを行う。
parent/audio/USB/WiFiのSYS再compile、署名、WIM編集、実機・runner操作は行わない。
出力はcheckout内 `build/mt8167-audio-engine-probe/`。

- `mt8167-audio-engine-probe.exe`
- `probe-manifest.json`（probe source、expected driver/INF source、EXE hash）
- `pe-imports.txt`、`build.log`、このREADME

WDK 8.1 SDK、clang、lld-link、llvm-readobjを既存build環境で使う。
CRT / uuid.lib / ksuser.dllは不要。hard importsはkernel32 / ole32 / advapi32 / setupapi / cfgmgr32。
MMDevice/WASAPIのCOM実装は通常Windowsの標準コンポーネントを使用する。

主担当の既存の固定コマンド実行経路から、次構成で一度だけ呼ぶ。

```text
<配置先>\mt8167-audio-engine-probe.exe
```

引数は使用しない。実行に必要なPCMはEXE内に固定されている。
サービスとして登録せず、外部PCM・EXE・DLL・scriptの指定や任意コマンド実行を受け付けない。
固定のWindowsサービス2個が停止している場合だけSCMのStartServiceを使う。
既に実行中ならqueryだけで進む。無効化・欠落・権限不足は明示して失敗し、
service設定の変更や独自brokerによる代用はしない。
主担当の既存実行主体にdisklog保存と音声read-only診断へのアクセス権が必要。UARTは補助輸送である。
既存toneのCMD/serviceからこのprobeを自動起動しない。

## 一回のコマンドが行う処理

1. EXEと同じdirectoryへ `audio-engine-probe-<runid>.log` を新規作成し、
   stdoutと既存TOPRGU `A04BR3ScreenUart` も試す。disklogを作成・書込み・flushできなければ再生しない。
   UARTのCreateFile error 2（device名なし）を含むopen/send失敗は、補助輸送エラーとしてdisklogへ残す。
   disklogを確保できれば標準AudioSrv/WASAPIとDMA診断へ進み、UARTだけの失敗でabortしない。
   UART失敗後は再試行を繰り返さず、TOPRGUやboot設定を変更しない。
   各passのStart前にもdisklogをflushする。pass間で保存失敗を検出した場合は次のpassを開始しない。
2. endpointの有無に依存せず、presentな`MT8167\AFE\` devnodeのCM status/problemと
   `DEVPKEY_Device_ProblemStatus`のraw NTSTATUSを一度回収する。親のinstance/statusも記録する。
   property読取り失敗や型/長さ不一致を成功やNTSTATUS=0へ読み替えない。
   固定のAudioEndpointBuilder / AudioSrvをqueryし、停止時のみ開始する。
   サービス開始は各15秒、endpoint列挙は最大約10秒まで待つ。
   この待機はOSサービスの準備待ちであり、DMA静止を推定するwaitではない。
3. render endpointをMMDeviceで列挙する。DeviceTopology connector 0から
   GetDeviceIdConnectedTo → IMMDeviceEnumerator::GetDevice → PKEY_Device_InstanceIdへ辿り、
   `MT8167\AFE\`の実devnodeに接続するactive endpoint一つだけを選ぶ。
   default endpoint、friendly nameの推測一致、USB音声へのfallbackは使わない。
   endpoint ID、実instance ID、OSのfriendly nameは正確なUTF-16で保存する。
4. 同じinstanceのKSCATEGORY_RENDER interfaceをSetupAPIで一つだけ開く。
   GENERIC_READで既存v11 read-only snapshot IOCTLだけを使い、KS pinを生成しない。
   KSCATEGORY_AUDIOにはTopologyも存在するため、旧clientの単一interface前提は流用しない。
5. WASAPI clientをshared modeで初期化し、GetMixFormatの形式を使う。
   対応は48 kHz / 2chのPCM16かIEEE_FLOAT32（通常/EXTENSIBLE、FL|FR）。
   AudioSrvがfloat mixからdriverのPCM16へ変換する場合も標準engineに任せる。
   その他の形式を勝手に解釈せず、unsupportedとしてraw HRESULTを残す。
6. IAudioRenderClient::GetBuffer/ReleaseBufferで、組込みPCMを3秒送出する。
   1秒1500 Hz → 1秒silence → 1秒750 Hz、左右同じ、peak 1/256 FS。
   IEEE_FLOATも正確なbit列を生成し、targetのFP変換helperやCRTは使わない。
   これは任意payloadではなく、同じプログラムに固定したPCMである。
7. GetCurrentPaddingで補充量を決め、GetDevicePeriod由来の1–10ms間隔で補充する。
   全144000 framesをqueueした後、padding=0を実際に確認してStopする。
   queued frame数、hash、clock進行だけで末尾再生済みとは扱わない。
   各passは最大10秒。固定silenceも実PCM packetとして送る。
8. IAudioClockのfrequency、position、QPC、paddingを約37msごとにメモリへ記録する。
   別threadも約37msごとにDMA/codec snapshotを取得し、render補充をI/Oで塞がない。
   render loop中にはUART/file出力をしない。
9. Stopと全WASAPI service/client interfaceのRelease後、別IAudioClientを作って同じ3秒を送出する。
   API上のclose/reopenは試すが、AudioSrvがkernel pinをidle中保持することはありうる。
   **kernelのSTOP→RUNやNewStream再作成が発生したとは断定しない。**
10. 2回の送出または失敗の後、診断threadを停止して記録をUART/stdout/logへまとめて出す。
    グローバルendpoint volume/muteは変更しない。可聴成功は常に未確認。

snapshot readはoverlappedで500msを上限にcancelを要求する。
cancel完了までI/O storageを維持する。正常なcleanup cancelは診断失敗に数えない。
壊れたdriverがcancelを完了しない場合、mainは2秒のjoin期限後にdiag-stuckを記録する。
未完了I/Oのstorage/handleを手で解放せずprocess終了へ進み、正常終了とは扱わない。
kernel自身が停止している場合のprocess終了まで保証するものではない。
失敗したPCM Startを再送せず、再起動・復旧・独自KS操作へ進まない。

## 観測の意味とレコード

各行は96 bytes以内の `A04AUDENG r=<8桁runid> <tag> a=<hex> b=<hex> c=<hex>`。
数値は全て16進数。phase=0は非RUN区間、1/2は各WASAPI passのStartからStop要求まで。
非同期snapshotの前後でphaseが変わった標本は境界標本として進行判定から除く。
必須の回収証拠はEXE横のdisklogとprocess exit statusである。
UART IOCTL成功はqueue受付にすぎず、UARTを使えたrunだけhost側end受信を追加の輸送証拠にする。
UART不在runではhost側end受信を要求しない。run25通常OSでdevice名がなくCreateFile error 2だったという
主担当の既知観測を前提に、UART状態と標準音声の成否を分離する。

| tag | a / b / c |
| --- | --- |
| begin | probe ABI 1 / tick / probe source先頭32bit |
| transport | disk error / UART error / 1=UART optional・disk mandatory |
| uart-optional | UART open error / 0 / 0（音声失敗を意味しない） |
| disk-required | 必須disklog error / 0 / 0（PCM未開始） |
| expected-src | runtime source短縮値 / INF source短縮値 / version末尾（実ロード証明ではない） |
| devnode | CONFIGRET / CM status / CM problem（endpoint列挙とは独立） |
| problem-status | property Win32 error（0=取得成功） / DEVPROPTYPE / 取得できたraw NTSTATUS（error非0なら未取得） |
| parent-query / parent-node | 親取得CONFIGRET / 0 / 0、または親CM CONFIGRET / status / problem |
| devnode16 / parent16 | instance IDのUTF-16、通常のwide record形式 |
| devnode-count | 対象devnode数 / 0 / 0 |
| service | 0=EndpointBuilder,1=AudioSrv / SERVICE状態 / Win32 exit code |
| endpoint, route | endpoint index / HRESULT / state等 |
| instance16, endpoint16, friendly16 | UTF-16 code unit offset / 2 units packed / 次の2 units packed |
| text-end, text-trunc | UTF-16長 / 0 / 0（長い文字列の上限は1024 units） |
| mix, mix-pcm | pass / rate / tag、およびchannels / bits / blockalign |
| period | pass / reported default 100ns / reported minimum 100ns（low32） |
| initialize | pass / HRESULT / shared mode 0 |
| latency | pass / HRESULT / AudioClient報告latency 100ns low32（実測校正ではない） |
| clock-hz | pass / frequency low32 / high32 |
| submitted | pass / ReleaseBuffer完了frames / payload FNV-1a32 |
| wasapi-stop, pass-result | pass / HRESULT / runningだったか、または下記stage |
| clock-delta | pass / delta low32 / high32（単位はclock-hz） |
| sample, sample-end | sample index /取得tick/phase開始、およびindex/phase終了/Win32 error |
| state, valid | driver stage / last NTSTATUS / failure stage、およびsnapshot valid / codec valid / online |
| dma, cur | base / end / bytes、およびraw CUR / ring offset / exposed |
| irq, afe | notification count / faults / raw status、およびDAC_CON0 / AFE_TOP / IRQ接続状態 |
| codec, speaker, trim, rails, lease | snapshotの実観測値。valid bitがないgroupを0や成功と解釈しない |
| clock | pass / tick / GetPosition HRESULT |
| clock-pos, clock-qpc | low32 / high32 / paddingまたは0。position単位はclock-hz、QPCはAPI仕様の100ns単位 |
| dma-progress, dma-fault | pass / 有効標本数 / CUR変化数、およびpass / fault観測 / 0 |
| summary | 標準API完了bitmask / DMA進行観測bitmask / 音声・診断のresult（輸送エラーを除く） |
| output-state | log error / UART error / diagnostic error |
| audible | 常に0 / 0 / 0 |
| end | process result / tick / 補助UART error（0以外でも音声成功は可能） |

pass-result stageは、1 Activate、2 GetMixFormat、3 GetDevicePeriod、4 Initialize、
5 GetBufferSize、6 GetStreamLatency、7 render service、8 clock service、9 GetFrequency、
10/11 prefill GetBuffer/ReleaseBuffer、12 Start、13 padding、14 clock、
15/16補充GetBuffer/ReleaseBuffer、17 drain/clock進行期限、18 Stop、19完了。

API完了bitmaskのbit0/bit1は各passのStart、PCM提出、padding=0、clock増加、Stop、client closeが成功した場合。
DMA進行bitmaskは各passで、valid bit付きの同一ring内CUR標本が2個以上あり、変化があり、
IRQ fault/codec poison/有効なovercurrent異常を見ていない場合。
IRQ通知数をperiod総数と解釈せず、modulo CUR差から経過framesやwrap回数を推定しない。
別groupのcodec/rails状態はraw値とvalidを併せて評価する。

exit 0は両mask=3かつ必須disklog保存・診断にエラーがないことを示す。
UARTだけの不在/送信失敗はexit statusと音声resultを変更しない。
例: summary a=3,b=3,c=0、output-state a=0,b=2,c=0、end a=0,c=2 は
「標準APIとDMA進行は成功、disklog保存あり、UART device名なし」を表す。
exit 1はsetup/API失敗または未開始、3は観測不足等、4は診断thread未完了、5は必須disklog失敗。
最後のflushなどでexit statusがさらに失敗へ変わる場合もあるため、sender endだけで成功としない。
記録済みpayload hashはclientが標準APIへ渡した内容の証拠であり、DMA内容のCRCではない。
AudioSrvの変換・global mute/volume、実DAC出力、可聴性、欠落のない実波形まで証明しない。

## 一次仕様と検証範囲

- [Microsoft Rendering a Stream](https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream):
  shared bufferへのGetBuffer/ReleaseBuffer、paddingに基づく補充とinterface解放によるclose。
- [Device Topologies](https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-topologies) と
  [GetDeviceIdConnectedTo](https://learn.microsoft.com/en-us/windows/win32/api/devicetopology/nf-devicetopology-iconnector-getdeviceidconnectedto):
  endpointからadapterのdevice IDを得てGetDeviceへ渡す標準経路。
- [IAudioClock::GetPosition](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition):
  clock unitsとQPC位置。推測したkernelのpositionへ置き換えない。
- GUID/PROPERTYKEYはSDK宣言を使用。C MIDLのextern IIDをuuid.libへ依存させず、使用分をローカル定数で定義。
  値はMicrosoft win32metadataのRecompiledIdlHeaders/umのmmdeviceapi.h、Audioclient.h、
  devicetopology.h、functiondiscoverykeys_devpkey.hと照合した。

ローカルでは実APIヘッダー（MinGW-w64 11）でC構文とARM32 COFF生成を確認した。
新規の模擬再生試験は作成していない。ARM COFFはmachine=0x1c4、未解決参照は標準API importのみ。
追加ARM算術helper/CRT/未定義GUID参照がないことを確認したが、WDK 8.1 compile/linkとは区別する。
このprobeのWDK compile/link、通常Windowsのendpoint/API実行、DMA/codec観測は未確認。
実機・runner・SSH・REA・EDK2は操作していない。

## 旧fixed-tone clientのUART条件（変更していない）

`tools/winpe/mt8167-audio-first-playback.c` のrunはopen_uart失敗時、
uart_lost=1、result=3、goto doneとなり、ksuser.dllのload・KsCreatePin・RUNには到達しない。
open_uartは約5秒間openを試す。初期openに成功してもRUN直前のuart_lost確認でabortし、
最後にもUART輸送失敗をresult=3へ反映する。したがってrun25のdevice名不在が続く構成では、
旧tone clientも同じ理由でPCM試行前に止まる。
この修正は新しい通常音声probeだけに適用し、旧tone入口とTOPRGUは変更していない。

## run27のCode 10と親SYSの残留

run27のSetupAPIはaudio bind成功、CM_PROB_FAILED_STARTを記録するが、
開始失敗のNTSTATUSは記録しない。System.evtxのaudio関連記録はサービス登録と
install成功であり、StartDeviceの戻り値ではない。endpoint0と80070490は
endpoint選択失敗を示す。これらから開始失敗NTSTATUSを作らない。

run28直前の回収済み実配置は、audioが上表の2f56e9a4...、parentが旧fixed-tone版の
b64fe0af0ec1cb700159cf4c6b588eea4bd8c9591ad39bb031d2d37dbf92e201だった。
run27媒体のparentは99338c1b...と一致し、媒体と実配置が食い違う。
新旧parent INFも同一hash/DriverVerだったため、新しいEngine親が実際に選択された
動作結果としてCode 10を扱わない。親の不一致が今回のCode 10の直接原因だとは未確定。
Engine専用parent INFは11/06/2026,2026.11.7.1005として分離した。
[親の再包装手順](../../drivers/audio/mt8167-afe/README-engine-parent-package.md)を参照。

このprobe改良は、Code 10でKS interfaceもendpointも存在しない場合にも
標準PnP propertyを取得するための最小追加で、PnP再bind、再起動、driver登録や
独自kernel制御は行わない。propertyが未提供なら取得エラーを残すだけである。


## run49後のbounded出力候補

run49のd0ebは2 WASAPI pass/StopがS_OK、clock前進を記録した後、全snapshotの
三重出力中にserviceの上限でterminateされた。正常終了・可聴出力は未確認。
次clientは100ms read-only snapshotをcollect中に集約し、開始前/各passのfirst-last/
最初のinvalid/終了後の最大7代表だけ出す。実running/実CUR/IRQ-event前進を確認し、
未知値を0成功にしない。時計もfirst-lastに集約する。固定3sec×2のPCMとreopenは保持。
出力は合計16KiB以内、stdout/private fileへbuffered write、既存file flushを保持する。
per-row UART IOCTLは行わず、serviceのprocess start/endと保存ログ/nativeを使う。
上限延長、kernel/parent再compile、任意path/argsや偽の可聴判定は追加しない。
API Stopとterminal snapshotは別に記録し、bus drain/物理speaker音を証明しない。

集計DMA/IRQ/API結果は代表dumpより先にevidence-summaryとして書き出しflushする。
これは途中証拠であり、最終summary/end・process exit・file flushと区別する。
Audio1007のSYS/親の動作は変更しない。
