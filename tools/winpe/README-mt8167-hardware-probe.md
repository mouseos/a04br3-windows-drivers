# 通常OSの一回限りHardware Probeサービス

`MT8167HardwareProbe` は標準SCMのLocalSystemサービスとして動くARM32 EXE。
物理入力や対話desktopを必要とせず、署名済みの固定clientをCreateProcessWで各1回起動する。
`b508de8` のMT8167DeviceInstallのSCM登録・自サービス無効化・WDK署名build構造を利用する。
既定Tone modeはdriver導入を行わない。明示EnginePcm modeだけは末尾の固定3対象PnP再bindを行う。
UEFI変更、サービス再起動ループ、復旧は行わない。

## 固定配置と実引数

配置先はGetSystemDirectoryWが返す通常OSのSystem32配下だけ。

```text
System32\MT8167HardwareProbe.exe
System32\MT8167HardwareProbe\
  mt6630-wlan-probe.exe
  mt8167-audio-first-playback.exe
```

WLAN probeは本branchのtools/winpe/mt6630-wlan-probe.c、音声clientはHerschelの
`ae1c716` と統合済みv11のREADME/コードを確認した。**どちらも実引数なし**。
音声clientはGetModuleFileNameWから自分のディレクトリを求めてログを作る。
WLAN probeの作業ディレクトリも上の固定配置先に設定する。
サービス開始引数、registry値、環境変数からclientのパスや引数を受け取らず、
cmd.exe、PATH探索、汎用のコマンド実行、権限昇格brokerは提供しない。

CreateProcessWの直前に、固定ファイルを読取り権限・FILE_SHARE_READだけで開き、
reparse point/ディレクトリを拒否し、WinVerifyTrustで署名とローカル信頼chainを確認する。
署名なし・信頼不成立・検証API不在は起動しない。検証中からchild終了まで同じfile handleを保持し、
書換え/削除/名前の差替えを禁止する。親ディレクトリもreparseを拒否してrenameを禁止する。
署名検証はUIなし・cache-onlyで、WLAN未接続時のオンライン照会に依存しない。
署名のビルド時成功と、LocalSystemが対象OSで信頼できることは別である。
主担当は配置先をSYSTEM/Administratorsだけ書込み可能にし、開発用の公開証明書chainを
対象OSのmachine trustへ用意する。署名用private keyを対象へ配布する必要はない。

## 一回限りの動作

1. SCM serviceとして開始し、実行tokenがLocalSystemか検査。
2. 自サービスをSERVICE_DISABLEDへ変更。失敗したらclientを起動しない。
3. 固定配置先にattempted.binをCREATE_NEWで作り、書込みとFlushFileBuffersを確認。
   既存marker、作成失敗、保存失敗でもclientを起動しない。markerを削除しない。
4. WLAN、audioの順に署名を確認し、各clientを最大1回起動する。
   WLANは600秒、audioは120秒が上限。両者とも起動引数なし。
5. stdout/stderrを別の固定ファイルへ直接継承させる。
   STARTUPINFOEXのhandle allowlistでstdin=NULと当該2ファイルだけを渡す。
6. processはsuspendedで作成し、専用Jobへ割当て後にresumeする。
   timeout/SCM STOP時はそのchild Jobだけを終了し、5秒以内のprocess終了を確認する。
   終了確認できなければ次のclientも起動しない。再起動/再試行は行わない。
7. processの終了code、保存エラー、UART通知状態を記録し、SCMへSTOPPEDを返す。

登録は標準AutoStart（Start=2）で、遅延起動は指定せず、SCM recovery actionは空。WlanSvc依存でSCM開始自体を
阻止しないため、WLANコンポーネント不在でもサービスが試行を消費して無効化できる。
PnPとTOPRGU、WlanSvc/Native WLAN、Audio v11/KS/ksuser.dllの準備は主担当が行う。
準備不足によるclient失敗も同じbootで再実行しない。--startも無効化を解除しない。

timeoutでprocessを終了したことは、WLAN scan完了やaudioのSTOP/mute/DMA drainの証拠ではない。
既存音声driverは資源を保持する設計なので、audio timeout後のhardware状態は未確認として扱う。
SCM STOPは進行中のprocessを終了するが、PnP resetやhardware復旧は行わない。

## 試行中だけの自動スリープ抑止

marker作成後、clientの署名確認/起動/待機/終了処理の間だけ、
PowerCreateRequest → PowerSetRequest(PowerRequestSystemRequired)を1回取得する。
要求を作成/設定できなければ数値エラーを記録してclientを起動しない。
正常終了、署名/起動失敗、timeout、SCM STOPは同じcleanupへ進み、
取得済み要求をPowerClearRequestしてから、成功/失敗にかかわらずCloseHandleする。
Set失敗時も作成済みhandleをcloseする。登録補助/試行済み拒否では要求を取得しない。

SystemRequiredだけを使用し、DisplayRequired/AwayMode/ExecutionRequiredは要求しない。
画面点灯、恒久power policy、powercfg/registry設定を変更せず、ユーザー明示のSleepを尊重する。
Sleep後の要求再設定や、時間制限を回避するための定期的な再取得も行わない。

一次仕様:

- [PowerCreateRequest](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powercreaterequest): Windows 7以降のdesktop API、Kernel32、不要時はCloseHandle。
- [PowerSetRequest](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powersetrequest): SystemRequiredは無操作sleepを抑止。明示Sleepでは解除され、終了後は速やかなclear/closeが必要。
- [PowerClearRequest](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powerclearrequest): 取得した種類の参照を減らす。ExecutionRequiredはserviceでは使用できない。

RT8.1の提供確認として、既存Microsoft Surface RT回復媒体のinstall.wim image 1
（Windows RT 8.1 / CoreARM / ARM / 6.3.9600.16441）からkernel32.dllのPE export metadataを
読取り、PowerCreateRequest/PowerSetRequest/PowerClearRequestの3 exportを確認した。
当該DLL SHA256は `a7a035b4105ac4c3c9aa521571b919f54fdd2e60997fd2f3b8527e1f0c434c5b`。
DLLはソースrepoや配布成果物へ組み込まない。実OSのAPI受付/電源状態は未測定。

Microsoft仕様ではDCのConnected/Modern Standbyで、sleep timeout超過後の要求保持に
5分制限がある。SystemRequiredの取得成功を「10分間sleepしない」保証として扱わない。
対象の電源方式/policyに対する実効性は主担当の実測で確認する。

## 出力

同じ固定ディレクトリにservice.log、attempted.bin、wlan.stdout.txt、wlan.stderr.txt、
audio.stdout.txt、audio.stderr.txtを保存する。child出力はCREATE_NEWで、既存結果を上書きしない。
署名拒否などでprocessを作成しなかった場合、child出力が未作成の場合がある。
client自身のmt6630-wlan-probe.txt/audio-first-playback-*.logも同じ場所に残る。

service.logと既存TOPRGU UART queueへの短い通知は次の形式。全値は32-bit hexadecimal。

```text
A04HW RUN=12345678 ID=00000001 EVT=00000005 CODE=00000000
```

IDは0=サービス、1=WLAN、2=audio。serviceはclientのstdout/stderr本文をUARTへ中継しない。
WLAN client自身のUARTはcount/statusだけ。サービス通知にもSSID/BSSID/鍵/PSKを含めない。

| EVT | 意味 | CODE |
| --- | --- | --- |
| 1 | service処理開始 | 0 |
| 2 | 自サービス無効化 | Win32 error |
| 3 | attempted marker作成/flush | Win32 error |
| 4 | 固定EXE署名確認 | 0、Win32 errorまたはWinVerifyTrust status |
| 5 | process作成・resume | 0（probe動作成功とは別） |
| 6 | process終了 | 実process exit code |
| 7 | timeout | ERROR_TIMEOUT |
| 8 | SCM STOP等による中断 | ERROR_CANCELLED等 |
| 9 | 起動前失敗 | Win32 error等 |
| 10 | service終了 | 最初の非0結果、または保存/UARTエラー |
| 11 | 終了要求/確認 | API error、続いてprocess待機結果 |
| 12 | childログ保存エラー | Win32 error |
| 13 | service UART状態 | それまでの通知エラー（0も到着証明ではない） |
| 14 | 最終SCM結果（service.logのみ） | END自身のUART失敗も含む最終結果 |
| 15 | SystemRequired作成/設定 | Win32 error、0はAPI受付のみ |
| 16 | SystemRequired clear | Win32 error |
| 17 | power request handle close | Win32 error |

UARTは既存96-byteキューだけを利用する。通常は各通知後にhandleを閉じ、clientと共有する。
pending IOCTLは最大1秒、cancel後250msで打切り、未完了storageをprocess終了まで保持して
後続UART送信を止める。UART MMIO、COM port、新driverを追加しない。
IOCTL受付とホスト受信を区別し、実UARTの開始/終了レコードは主担当が確認する。
service終了0だけで実scan、WPA2接続、可聴成功を宣言しない。

## 主担当のビルド・署名・配置

WDK8.1 UM/shared ARM headers、kernel32.lib/advapi32.lib、LLVM clang/lld-link/llvm-readobj、
Windows PowerShellが必要。署名時はWDK8.1 signtoolとb508de8と同じ証明書store構造を使う。
WinVerifyTrustはSystem32のwintrust.dllから動的ロードし、検証不能なら起動拒否する。
build scriptはSCM登録・サービス開始・実機/runner操作を行わない。

```powershell
.\tools\ci\build-mt8167-hardware-probe.ps1 -OutputDirectory .\build\hardware-probe -WlanProbePath .\build\peripherals-integrated\wifi\wlan-probe\mt6630-wlan-probe.exe -AudioClientPath .\build\peripherals-integrated\audio-usb\client\mt8167-audio-first-playback.exe -Sign
```

- 入力clientは両方指定するか、両方省略してserviceだけをcompileする。
- -Signはserviceとコピーした2つのclientを署名する。元clientを変更しない。
- 出力manifestは署名後のhashを記録するが、対象OSのLocalSystemでの署名信頼は未確認とする。
- 統合build-mt8167-peripherals.ps1も未署名serviceと2つのclientをhardware-probe/へまとめる。

主担当のbulk適用builderがofflineで標準SCMサービスを登録する。
Service名はMT8167HardwareProbe、Type=0x10、Start=2、ObjectName=LocalSystem。
DelayedAutoStartは0または未指定とする。
ImagePathは引用符付き `%SystemRoot%\System32\MT8167HardwareProbe.exe` のみで、引数なし。
EXEの引数なし入口はStartServiceCtrlDispatcherWであり、対話起動や手動client起動を必要としない。
署名済みservice EXEはSystem32直下、署名済み2 clientsとログ/markerは同名directory配下へ置く。

通常OS上で登録する既存実行経路向けには、同じ契約の任意の登録補助手段も用意している。

```batch
"%SystemRoot%\System32\MT8167HardwareProbe.exe" --install
```

通常OSでPnP準備済みなら同じ既存実行経路から--startで直ちに開始できる。bulk登録では引数を付けない。
登録後の既存サービス更新/再有効化/marker削除は提供しない。
追加の物理操作や対話loginを求める実装ではない。

## 確認範囲

公開WDK参照headersでのARM32 compileと実DLL importのlink、script構文、固定allowlistと
自サービス無効化→marker→CreateProcessの経路を開発側で確認する。
模擬SCM/偽署名verifierの成功を実起動の証拠にしない。
WDK8.1の署名build、対象OSの署名信頼、LocalSystem開始、実probeとUART回収は主担当が確認する。

## 現行bulkのcopy-only構成と通知の限界

WinPEでSYSTEMをRegLoadして登録する旧prepare手順は使用しない。
現行bulk builderはTesla担当のnative-prepared clean SYSTEMと成功manifest/hashを必須入力にし、
private媒体でDISM前にcopy-only適用する。native registry recipeは別担当で、serviceの再武装を
このservice EXEや媒体builderが行うものではない。

run25ではinstaller自体のBOUND_STARTED/CM_PROBLEM=0/SCM_EXIT=0を主担当が回収した一方、
最終UARTのCreateFileはerror2で通常OSのScreenUart名がなかった。
probeの同名UARTも利用可能と仮定しない。UART最終通知だけで試行成功・未実行を判定しない。
主担当はSystem32/MT8167HardwareProbe配下のservice.log、wlan.stdout.txt、wlan.stderr.txt、
audio.stdout.txt、audio.stderr.txt、attempted.binをprivate diskログとして回収し、
子プロセス開始/終了、終了コード、EV_POWER_SET/CLEAR/CLOSE、UART errorを対応づける。
これらの実機ログやhiveを公開repo/CI artifactへ含めない。

## 明示EnginePcm mode（別service）

既定 `-AudioMode Tone` はMT8167HardwareProbeのまま。`-AudioMode EnginePcm` は
compile-time A04_HARDWARE_PROBE_ENGINE_PCM=1で別の固定serviceを生成する。
registry/起動引数から任意EXE・path・argsを選ぶbrokerは追加しない。

| 項目 | Engine mode固定値 |
| --- | --- |
| service / EXE | MT8167EngineProbe / System32\MT8167EngineProbe.exe |
| clients | System32\MT8167EngineProbe\clients\mt6630-wlan-probe.exe と mt8167-audio-engine-probe.exe |
| service/子stdout・stderr | System32\MT8167EngineProbe\logs\ |
| marker | System32\MT8167EngineProbe\attempted.bin |

旧MT8167HardwareProbeのkey/path/attempted.binは削除も再武装もしない。
Engine serviceは自身を無効化し、自分のmarkerをCREATE_NEW/flushしてから、
2つの固定clientをそれぞれWinVerifyTrustで検査・write/deleteを拒否するhandleで保持し、
WLAN→Engine音声の順に各一回・引数なしで起動する。旧tone名はEngine binaryへ入れない。
既存job/timeouts/SCM STOP・SystemRequiredの設定/解除・disk flushを保持する。
Engine serviceではUART errorを別記し、child exitとdisk failureを実行結果として扱う。
service成功やUART最終通知だけでAPI/DMA/可聴まで成功したと推定しない。
固定d0eb436 probe自身の必須ログは仕様どおりclients内にも作られるため、
clients/audio-engine-probe-*.logもprivate回収対象とする。

```powershell
powershell -File tools/ci/build-mt8167-hardware-probe.ps1 -AudioMode EnginePcm -Sign -CertificateThumbprint $MyDevelopmentThumbprint
```

公開build helperは特定の開発鍵を要求しない。EnginePcmとSignの併用時はCertificateThumbprintの
明示指定を必須とし、選択store内の秘密鍵付き・有効期間内の証明書を使う。
指定値は40桁SHA1として検証し、実際に選択した証明書のthumbprintをsigning_thumbprintへ記録する。
自己署名開発証明書の作成・対象Windowsでの信頼設定は各利用者が行う。秘密鍵を配布しない。
Toneの既存既定thumbprintと、Signなしのbuild経路は保持する。

今回のprivate snapshot媒体は別の固定入力契約であり、BCBF cert/hash/source pinを引き続き要求する。
主担当の既存 `-CertificateThumbprint BCBF6C456C6F0DD0818434763B189488FBF13570` 指定はそのまま使える。
他の鍵で公開helperをbuildできることと、今回private媒体の入力として受理されることは別条件。

この例は新serviceだけをbuild/署名する。既存client2本・SYSは再compile/再署名しない。
任意のclient staging引数を使う場合もmodeで固定したbasename2個だけを許可し、
Engine modeのSignはservice EXEだけを対象にする。modeの異なる出力directory再利用を拒否。
生成manifestは固定service、client/log/marker path、mode define、client allowlist、certを記録する。
新しいservice EXEはsource変更後のWDK build/link/署名が必要で、旧27ad206 EXEはEngine用に使えない。

### Engine専用の固定PnP再bind

run26r2では署名/service/PowerRequest/両client起動まで成功したが、WLANはINTERFACES=0、
旧toneはUART error2でKS開始前に終了した（主担当実測）。installed audio SYS不存在、Wi-Fiの
SetupAPI CM_PROB_FAILED_ADD=0x1fもあり、DISM stagingだけを開始済みの証拠にしない。
raw SYSTEMのAFE ConfigFlags=64は未replayでありlive状態とは断定しない。

924版Engine serviceは自分のmarkerを消費後、clients開始前に次の3対象を順番に各最大1回だけ
UpdateDriverForPlugAndPlayDevicesW(INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE=5)へ渡す。

| ID / log ID | 固定HardwareID | System32\MT8167EngineProbe\drivers以下の固定INF |
| --- | --- | --- |
| 10 / 0000000A | MT8167\AFE | audio\mt8167-afe-engine-pcm.inf |
| 11 / 0000000B | MT8167\USB20 | usb\mt8167-usb20.inf |
| 12 / 0000000C | A04BR3\MT6630SDIO | wifi\mt6630-runtime.inf |

各directoryとINF/SYS/CATを非reparse確認しwrite/deleteを拒否するhandleで保持する。
全3 CATをWinVerifyTrustで確認してから更新を開始し、INF/SYSのcatalog membershipはWindows標準PnPが検証する。
source/hash/cert別の入力pinは媒体builderが担う。CAT失敗/欠落、API解決失敗、disk failureやSTOPはclient開始を止める。
APIのBOOL/Win32 error/reboot、before/afterのCM query結果・一致数・status・Problem・started数をdiskへflushする。
present nodeのHardwareIDを厳密比較し、instance suffixを推測しない。照会失敗と対象不存在も区別する。
一回のafter観測であり、再試行や待機ループはない。CM_STARTEDでもWLAN interface/USB入力/音声動作の証拠とはしない。

reboot flagまたはERROR_SUCCESS_REBOOT_REQUIREDなら結果3010で後続更新と両clientを停止する。
自動reboot、marker削除、再armはしない。再起動不要のAPI/CM失敗は最終結果へ残したまま、
他対象の更新と2clientの独立診断を続ける。USB device role/外部VBUSでの開始拒否を音声試験の禁止条件にはしない。
924版の親は更新対象外。同期PnP API実行中のSCM STOPはAPIを中断できず、戻った後で停止する。

service.logのEVTはhex表記。13/14=before/after、15=list CR、16=locate CR、17=property CR、
18=matches、19=status CR、1A=status、1B=Problem、1C=started数、1D=package、1E=API開始、
1F=API BOOL、20=Win32、21=reboot、22=結果。複数一致時のstatus/Problemは列挙順に繰り返す。
CM一覧取得失敗ではmatchesを確定できず、list CRと最終結果を見る。旧toneのEVT番号は変更しない。

Engine親と旧f17親はともにDriverVer=2026.11.6.1001であるため、DISM完了だけでEngine親への
交換を保証しない。実ロード親のhash照合/必要な親の導入は主担当が別途判断し、924版serviceは親を更新しない。

## 次反復の親選択確認・正確なProblemStatus

次反復版はACPI\BEN0001親の観測を先頭へ追加し、ID=10親、11音声、12USB、13Wi-Fiとなる。
固定packageはdrivers/parent/{shared-parent.inf,mt8167-shared.sys,shared-parent.cat}を含む。
旧924の3対象serviceは初回媒体の互換入力として残すが、新rearm planには使用できない。

before/afterでCM_Get_DevNode_PropertyWをsystem cfgmgr32.dllから解決し、Windows 8.1 SDKの
DEVPKEY_Device_ProblemStatus（4340a6c5-93fa-4706-972c-7b648008a5a7, pid12）を照会する。
CONFIGRET成功、DEVPROP_TYPE_NTSTATUS、4 bytesの3条件が成立した場合だけraw DWORDを記録する。
取得失敗は0のNTSTATUSに変換せず、CR/type/bytesを記録する。
DEVPKEY_Device_DriverInfPath（pid5）とDriverVersion（pid3）の文字列も同じnodeから読み、
FIELD=INF/VERSIONでdiskへ出す。before/after区切りとIDを対応づけ、古いoem INF残留を検出する。
追加EVTはNT_CR=0x23、NT_TYPE=0x24、NT_BYTES=0x25、NTSTATUS=0x26、
BIND_CR=0x27、BIND_TYPE=0x28、BIND_BYTES=0x29、PARENT_ALREADY_SELECTED=0x2A。

WinPE staging後の次bootで、親の選択versionがstaged INFと一致し、CM開始済みで、
配置System32/drivers/mt8167-shared.sysがstaged署名SYSと全byte一致することを確認してから子試験へ進む。
配置byte一致もlive loaded imageの証明ではなく、後続fresh backupのhash照合と起動対応付けが必要。
親にtouch/buttons/GPU等の他のresource利用者が存在し得るため、serviceは親のUpdateDriverを呼ばない。
音声/USB/Wi-Fi3件が未開始でも全childのlifetime停止を証明したとは扱わない。
親の版/CM/配置が不一致ならPARENT_SELECTION_REQUIRED/ERROR_REVISION_MISMATCHで停止する。
APIを呼んでいないこの状態をWindows由来のreboot-requiredとして記録しない。
親の正規更新・次boot選択は主担当の別工程。INF昇版とoffline stagingだけで既存nodeが必ず切り替わるとは保証しない。
独自resource解除、直接SYS上書き、起動中の親強制交換、自動rebootは行わない。
子の標準APIがreboot-requiredを返した場合は後続子/clientを停止する。
PARENT_ALREADY_SELECTED=0x2A、PARENT_SELECTION_REQUIRED=0x2B、PARENT_SYS_MATCH=0x2Cを追加し、
PARENT_SYS_MATCHのCODE=0は配置全byte一致を表す。実I/Oの成功とは別。

nativeの既知disabled fingerprint、fresh backup、修正package/昇版と結び付いた明示rearm手順は
README-mt8167-shared-io-update.mdを参照。新service EXEだけのWDK build/署名が必要。
通常音声clientはd0eb436のままでよく、a9f8631 clientの再buildを前提にしない。

明示Engine専用の固定5package延期install→正規再起動→一回診断は
[README-engine-deferred-update.md](README-engine-deferred-update.md)を参照。
`-DeferredPackageMap`未指定では既存動作を維持する。この周辺機器版は実機未検証。


### Engine deferred: SYSTEM diagnostic snapshot (schema 1)

`-AudioMode EnginePcm -DeferredPackageMap <fixed-map> -WorkspaceRoot <root>`
のservice buildでは追加switchなしで有効。Tone/従来Engineの動作は維持する。
既存04ce固定checkout、SYS、clientは変更しない。HFP実装は別checkpoint。

保存先は固定
`HKLM\SYSTEM\CurrentControlSet\Services\MT8167EngineProbe\Parameters\LastDiagnostic\<policy64>\{Install|Diagnostic}`。
LocalSystem確認後、SCM workerのuser-mode PASSIVE contextだけで作成する。
`policy64`は生成policy全体のcanonical JSON SHA256（既存`deferred_token`）であり、
policy-manifest.jsonの実ファイルSHA256とは異なる。fresh backup pinもpolicyに含む。
同一世代keyが既存なら拒否し、上書き・削除・自動再試行しない。
旧世代は保持する。読出し側は期待policy64、Schema=1、Phase=1/2、
TargetCount/ClientCount、Complete=1を照合し、stale世代を使用しない。
Completeは収集終了を表し、driver成功を表さない。

`Node0..5HardwareId`は親/AFE/USB/WiFi/DOD/optional GPUの固定順。
`NodeNBefore`/`NodeNAfter`はREG_BINARY little-endian DWORD列：
`observed, discovery_rc(Win32), status_cr(CONFIGRET), status_valid, status, problem,
property_cr(CONFIGRET), property_type, property_bytes, nt_valid, ntstatus`。
未照会はobserved=0/valid=0、値0xffffffff。失敗queryのstatus/problem/NTSTATUSを
0に置換しない。Beforeはpreflight、Afterはinstall後、診断bootでは全client終了後の
read-only照会。optional GPUなしではTargetCount=5。

`Client0..2`はWLAN/Engine audio/optional brightness固定順、同DWORD列：
`observed, created, resumed, wait_finished, exit_valid, exit_code, result`。
exit_validが0ならexit_codeを使用しない。resultはlaunch/wait/I/O等の最終Win32結果。
`ClientNStdout`/`ClientNStderr`は同じ継承file handleから読んだ最大65536 bytesのraw REG_BINARY。
対応`...Info`は`read_error, file_size_low, file_size_high, captured_bytes, truncated`。
不確実な子終了時は読出しを行わずERROR_IO_INCOMPLETE。不存在・読出し失敗は
明示error、data valueの不存在を空出力成功と扱わない。元ファイルは保持する。
`ServiceResult/FileLogError/UartError/SnapshotWriteError`もDWORD。
全書込み後に一度だけRegFlushKeyし、closeしてからSCM STOPPED。
flush/close失敗はSCM errorとUART EVT=0x110で報告する。
Complete=1だけではflushの実成功や次Recoveryでの回収を証明しない。

run33消失原因は未確定。service.logのFILE_APPEND_DATAだけのopenを、
FlushFileBuffers文書が要求するGENERIC_WRITE＋明示EOF seekへ修正した。
既にGENERIC_WRITEだった全stdout/stderrの消失まで説明する修正ではない。
path誤生成・削除・越境書込みによる全消失はsourceから確認できていない。

旧service.log欠落時だけ、captureの既存`--engine-probe-state`に
`--allow-missing-engine-service-log`を明示追加する。marker/EXEは必須のまま。
plan生成も同名switchを明示し、既知旧EXE hash、既知marker、absent記録、
fresh backup manifest hashを束ねたmissing snapshot SHA256をplanへ固定する。
通常capture/planは欠落拒否を維持。旧packageより高版の修正hash、native既知七値fingerprint、
一度限定gateを保持する。既存state dir/marker/旧EXEとplanはarchiveし、marker削除はしない。
媒体はexpected-absentを直前にも確認し、予期せずlogが存在すれば停止する。
実Windows RegFlushKey永続化・WDK linkはhost compileだけでは未検証。

次のbrightness候補はloaderだけに限定追加：DODとnative client sourceは
`8a79f2735c27cf783b4d1d703b155dd80178f9b3`、DOD DriverVerは
`11/07/2026,0.0.1.5`の正確な組合せ。旧4fafe9a clientと旧DOD1.4も保持する。
任意client sourceへの緩和はしない。mapのsource、署名wrapper、build manifest、
EXE/INF/SYS/CATの実hashとcertificate照合は従来どおり必須。
新署名artifact自体の検証は回収後。今回loader変更は旧実署名mapでhost確認した。
