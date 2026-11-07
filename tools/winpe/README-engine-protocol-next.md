# 固定Engine更新protocol 次候補

現run42媒体、Audio1007/3d83署名bundle、Recovery full45は変更しない。
source生成、WDK/署名成功、実機動作は別々に扱う。

## 固定brightness世代

client `ccef15ae47ebae71577ba266bfa88bcae6476ca8`だけに
`C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log`を許可する。
manifestのstdout_tee=true、stdout_max_bytes=16384、Engine-state-archive、
legacy_root_log_modified=falseも検証する。旧4f/8a sourceの旧root path契約は維持。
新世代は旧Engine state全体を既知planのarchiveへmoveした後に作る。
新clientの場合、旧root brightness logを削除/moveせず、新log作成guardも旧rootへ
向けない。WinPE replayで旧root logが再出現しても新世代を塞がない。
未知source/pathを許可する一般brokerではない。captureの新pathはoptionalだが
対象のEIOをabsent扱いしない。43の兄弟inode EIO記録を消さない。

## 終了時volume flush

新deferred serviceはclient終了/各file flush-close/service.log close後に
固定 `\\.\C:` をGENERIC_WRITE、FILE_SHARE_READ|FILE_SHARE_WRITE、OPEN_EXISTINGで開く。
LocalSystem SIDとWindows directory C:\Windowsを確認してからFlushFileBuffersを
一度だけ呼びcloseする。任意path、raw read/write、lock/dismount、retryは無い。
実open/flush/close error、attempted/returned/elapsedを7DWORDで保存しUART111で通知する。
既存ServiceResultを保持し、CompletionResult/SystemVolumeFlushErrorを分ける。
その結果を含め既存の一度RegFlushKeyで保存し、closeしてSCM STOPPEDを返す。
volume flush後のregistry記録はRegFlushKeyで永続化するのであり、先行volume flushに
含まれたと主張しない。flush return成功はNTFS clean shutdown/全inode可読/eMMC健全性の
証明ではない。API同期waitに時間上限の保証は無い。現run42原因の断定に使わない。

根拠: [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)、
[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)、
[RegFlushKey](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regflushkey)。

## 明示staging profile

mapの省略時/`staging_profile=offline-all`は従来のoffline DISMを維持する。
`staging_profile=normal-deferred`は既存Engine/freshbackup/再arm専用。
親/TOPRGU/GPU bootstrap実SYSを照合してから、nonbootの親/Audio/USB/WiFi/DOD/optionalSDを
normal固定署名installerへ集約する。完全一致のTOPRGUもoffline再追加を省略する。
変更したTOPRGUはboot-startのためoffline経路を維持。eMMC/WinPE storage/bootstrapは不変更。
GPUを更新しない構成は既知source860/2.5/SYSa8f226を保持する。
final builderにも`--rearm-previous-media`を明示し、そのSHAをnative planの旧媒体pinと照合する。
trust/native clean SYSTEM copy/firmware/固定file配置/freshbackup/archive/releaseは残す。
normal側は全fixed present node/署名/CAT membership/hashを検査後、一度のDIF_INSTALLDEVICE+
DI_DONOTCALLCONFIGMGで延期し正規rebootを一度行い、selected版/date/実SYSを照合して診断する。
旧親で対象nodeが列挙されない場合は更新前に拒否。初回OS/bootstrap導入には使わない。

offline DISM後にnormalが必ずフル再copyするという意味ではない。既存same版/date/SYSは
normalでskip、SetupCopyOEMInf自身も同一INF/CATを再利用する。現prepare所要時間の主因は
実時間log未照合のため断定しない。[SetupCopyOEMInf](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupcopyoeminfw)、
[DIF_INSTALLDEVICE](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/dif-installdevice)。

## Optional SD3/親1010

map `sd_tuple`はdirectory/manifest_sha256/source_commit/certificate_thumbprintだけ。
Nash `9e07f0a6cdf20f6efe9a4a75ed5f42fcd6ee583c`のtuple-manifest.jsonで署名済み
親1010+SD3を選択する。ecaa1008全flags+SD_POWER=1だけ、thermaloff、touch/buttons/audio/
USB/WiFi/RTC保持、eMMC無変更を検証する。両SYSを再compile/再署名しない。
SDはACPI\MOU0013、SCSIAdapter、Mt8167Sd、StartType3の固定INFだけ。
新SD-only ACPI firmwareのMOU0013列挙が前提。protocol builderはFD生成/書込みをしない。
全node存在の事前検査で、古いACPIでの部分更新を防ぐ。

既存5node、optional GPUのindex5を保持。SDだけならindex5、GPU併用時SDはindex6。
新registry Schema2のTargetCountは5..7、ClientCountは2/3。Node/Client binary layoutは維持。
旧Schema1の5/6nodeを含むbounded readerはtools/recovery/Read-EngineLastDiagnostic.ps1。
旧main readerのNode[0-5]固定では7nodeを回収できないため、新reader関数へ接続する。

SD初回の再armはfreshbackupへ固定mt8167-sd.sysの明示ENOENTが必要。
captureの`--sd-component-state`でこの1ファイルだけoptional記録し、従来7SYSは必須維持。
full45はこの追加scopeが無いのでSD有効のfinal gateには使わない。SD無効候補はfull45を利用できる。
実機read-only再captureとACPI適用はmain専有、盲単独試験はしない。

## 既知LastDiagnosticを残すnative再arm

run42後のfull45はserviceに自身のParameters/LastDiagnosticを持つ。旧native recipeの
「subkeyは全て拒否」ではこの正規状態も拒否されるため、Engine明示再armだけに
boundedな例外を追加する。service直下の七値の既知raw fingerprintは従来どおり必須。
許可するのはParameters/LastDiagnosticの、旧署名policyと過去の固定rearm planから
引き継いだ最大16個の64hex世代、Install/Diagnosticだけ。値名/type/record長/nodeIDと
個数はSchema1/2に限定し、未知subkey/valueは拒否する。全leafのraw value fingerprintと
階層形状から自身の診断subtree fingerprintを計算し、別clean copyの再load後に一致確認。
既存診断は変更/削除しない。Select/DeviceInstall/RpcSsの限定fingerprint監査も維持。
これは全SYSTEM監査ではない。最初の旧recipe拒否結果はfailed-do-not-deployのまま保存する。


## run47 SD初回installとpartial failureの区別

run47はSDのDIF_INSTALLDEVICEだけE0000204となり、既に延期した他対象のため
ServiceResult3010で正規rebootした。診断phaseはarmされず、次bootのEngine Startは4。
第二boot自動PnPでのSD Configure成功/Code31は別のkernel開始問題である。
Audio1007はNativeStart stage14/result0だがclient未実行なのでendpoint成否は未観測。

次serviceではSDだけSPDIT_COMPATDRIVERを使い、固定INF内の唯一nodeと版/dateを検査し、
DIF_SELECTBESTCOMPATDRV後の選択driver版/date/type/名称と正確なSCSIAdapter ClassGUIDを確認する。
DIF_REGISTER_COINSTALLERS、DIF_INSTALLINTERFACESを一度ずつ呼び、
DI_DONOTCALLCONFIGMG/DI_NEEDREBOOTを再設定してDIF_INSTALLDEVICEを一度だけ呼ぶ。
他対象の既存class-list経路、CAT membership/hash、live親交換禁止は維持する。
ClassGUID/keyの直接作成、任意INF/class、失敗後のretryは無い。
この変更は初回class選択/登録の標準契約を補う候補であり、E0000204の唯一原因や
実機修復成功とはまだ確認していない。

根拠はMicrosoftの[SetupDiBuildDriverInfoList](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdibuilddriverinfolist)、
[SetupDiSetSelectedDriverW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdisetselecteddriverw)、
[DIF_REGISTER_COINSTALLERS](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/dif-register-coinstallers)、
[DIF_INSTALLINTERFACES](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/dif-installinterfaces)、
[SetupDiInstallDevice](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiinstalldevice)。

Schema2のPnpBatchResult/CompletionResultは元のoperation errorを保持し、
SCMのServiceResult3010を全対象成功と解釈しない。InstallFailureValid/Target/Step、
AllInstallStepsSucceeded、DiagnosticArmed、RebootPending、3markerの実query結果を保存する。
Targetは0-based、特定不能はFFFFFFFF。Stepは0=準備/照合、1=list、2=選択、
3=coinstaller、4=interfaces、5=install、6=pending marker/診断arm。Completeはsnapshot完了であってinstall成功ではない。
旧schemaで存在しない追加値はreaderでunavailable。最大7node/3clientでも64値以内。

全driver不変の再armは既定拒否を保持する。plan生成の明示
`--allow-known-sd-protocol-repair`だけ、旧88aの固定署名EXE、freshbackupの実EXE/log hash、
単一RUN内のSD E0000204/batch E0000204/END3010、全package不変、
別commit/別EXEかつ延期runtime実source変更を検査する。最終builderでも旧媒体/新service/
同snapshotを再照合する。これは旧run47一回のprotocol修正専用であり一般retryではない。
旧log/marker/registry世代は既存の明示archive/fingerprint契約で保存する。

USB次候補はsource `44899b9755cdb081954a8d4ccb6271b839b9dd0d`と
`11/07/2026,2026.11.7.1005`の組だけ追加許可する。新署名manifest/hashが必要で、
旧1004/既存親の再compile/再署名はしない。StartのWDF execution-level問題の修正は
USB owner側であり、本protocolの初回SD処理とは独立する。


### SD4を独立追加する次構成

`sd_tuple`は従来の9e07親1010/SD3署名tupleのまま保持し、明示`sd_package`だけで
Nash source `16ed4f853788f6810f5a4644f94c638fa6f8d2e7`、SD4版11/07/2026,2026.11.7.4、
署名SYS f028660d…、checkpoint schema1/revision4を検査してSD componentだけ置換する。
親1010/SYS4638ab23…は不変、nonboot MOU0013/Start3/PIO/SDpower interface1固定。
新署名manifest/cert/hashとhash済みbuild metadataが必要。Code31根因解決は未確認。

SD初回selectionは[公式DIF_SELECTBESTCOMPATDRV](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/dif-selectbestcompatdrv)
をSetupDiCallClassInstallerから呼ぶ。default handlerの直接呼び出しはしない。
[SetupDiSelectBestCompatDrv](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiselectbestcompatdrv)
のclass移行契約と、run47第二bootのhardwarePnP selection順序に合わせる。
SetupDiSetSelectedDriver自体にも管理者によるclass変更契約があるため、旧直呼びだけを
E0000204唯一の原因と断定しない。固定唯一driverは選択前後に照合し、未知候補は拒否する。


WiFi校正次候補はsource `2d726d42702831aedbcee136d901595f65b39eee`、版
`11/06/2026,2026.11.6.1007`の既知署名manifest layoutだけを1006に追加して許可する。
全3artifactとbuild-manifest実hash/certを検査し、親1010はtuple側で選択した署名bytesを使う。
WiFi単独buildで使用した既存親1008を媒体へ持ち込まない。起動改善は実機未確認。


## run49 brightness launch guard

run49 ccefはEngine/logsへ書く契約だが旧serviceのrun_clientにroot logの存在拒否が残り、
旧root189Bを保持するprotocolと矛盾してB7となった。次serviceは生成済み署名policyの
正確なccef source/logからDEFERRED_BRIGHTNESS_SCOPED_LOGを決定する。
scoped時だけEngine/logs/brightness-native.logをguardし、旧rootは検査/移動/削除しない。
旧4f/8a sourceは従来root guardを維持。新log既存/reparse/予期しない属性errorは拒否し、
CREATE_NEW/旧Engine state archive/明示再armは維持する。任意path指定は追加しない。
