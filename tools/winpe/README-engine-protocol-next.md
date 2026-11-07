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
