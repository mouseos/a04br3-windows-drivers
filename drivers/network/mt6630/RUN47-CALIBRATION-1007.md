# run47 校正読取り修正候補1007

## 確定した観測と未確定事項

実配置SYS一致の1006は、FW read/hash、board/control、WMT/WLAN bootまで成功。
DIAGNOSTIC_CALIBRATION=-1の後、InitializeはC0010011を返した。
校正内のread/decode/runtime/submitは旧記録では区別できず、単一rootcauseは未確定。
FW再配置やboot opcode変更で解決したとは扱わない。

## 読取り側の変更

- `SystemRoot`をdirectoryとして開き、参照付きfilesystem base deviceから
  `IoGetDiskDeviceObject`でstorage volumeを得てextentを照会する。
  symlink targetがHarddiskVolume数字であるという文字列表記制約を除く。
  filterの上端をfilesystem本体と誤認しないよう`IoGetDeviceAttachmentBaseRef`を使用。
  全参照はI/O完了後に解放する。drive letter/disk number/partition offsetは固定しない。
- private synchronous handleのpending IOCTL/readを完了待ちし、実IO_STATUS_BLOCKと
  read長512を確認する。pending時にstack/outputを解放する旧挙動を除く。
- 無関係なdisk interfaceのopen/query失敗では列挙を継続し、system volume extentと
  一致する唯一のdiskだけを採用する。GPT名nvramの唯一性、境界、disk lengthを検証。
- NVRAMのAllMap/AllFile、全体checksum、WIFI512+checksumを検証する既存parserは維持。
  read-only、個体から動的取得、失敗時拒否、buffer wipeを維持する。

これらの旧コード上の制約/寿命問題がrun47で実際に発生したかはまだ未観測。
推測NTSTATUS、default校正、個体値の埋込みを追加しない。

## 校正コマンドの照合

CT3 vendor source `7c4782127d4ea11965be00ce1343056be9bfb769` のgen3
`common/wlan_lib.c:wlanLoadManufactureData`、`CFG_Wifi_File.h`、`nic_cmd_event.h`と照合。
既存2G経路のTX power 0x24、条件付きchannel offset 0x41/RSSI 0x43、domain 0x0f、
country limits 0x49、条件付きband edge 0x36/AC power 0x42、original512の0x48は維持。
新opcode、payload/順序変更、未実装5G capabilityは追加しない。

## 固定checkpoint

schema1/1624byte/64event/10flushの既存契約を維持。個体値は記録しない。
追加は成功経路で最大6event。Native attrs失敗後のcleanup/returnまで最大64event。
per-sector/per-commandの大量記録、追加flush、OID/IRQからのregistry操作はしない。

|Stage|Kind / detail|
|---|---|
|56 CALIBRATION_ALLOCATION|OBSERVATION0、allocation失敗時のみ|
|57 CAL_SYSTEM_EXTENTS|NTSTATUS、phase1 root open/2 file reference/3 volume device/4 extent IOCTL|
|59 CAL_DISK_MATCH|NTSTATUS、low16 match数/high16 skip数（飽和）|
|65 CAL_NVRAM_READ|NTSTATUS、phase<<24 / parser_result低8bit<<16 / read回数低16bit（飽和）|
|66 CAL_REGULATORY_DECODE|実backend戻り値|
|67 CAL_RUNTIME_BEGIN|実backend戻り値、呼出し時のみ|
|68 CAL_MANUFACTURE_SUBMIT|実backend戻り値、detailは最後/失敗command ID|

Stage65 phase: 3 allocation、4 GPT layout、5 nvram partition、6 disk length、
7 sector adapter、8 parser。parser-only失敗はSTATUS_DATA_ERROR、実read失敗は
その最初のNTSTATUSを保持。値/offset/MAC/country/NVRAM内容は保存しない。
未到達stageと末尾flushは既存通り未観測であり成功扱いしない。

## 再現と受入条件

固定clean sourceから`tools/ci/build-mt6630-runtime.ps1 -OutputDirectory ...
-SharedParentDirectory ...`、続いて`tools/ci/package-mt6630-runtime-1007.ps1
-BuildDirectory ... -OutputDirectory ...`。全出力はcheckout内の新規directory。
親は既存署名1008をhash/interface blob/critical flagsで検証して再利用し再buildしない。
INFは2026.11.6.1007、checkpoint driver_buildも1007。旧1006候補を上書きしない。

host限定fixtureは校正read/送信失敗、wipe、元record転送、trace実結果、合成NVRAM
checksum/境界を確認。WDK build/sign成功と実機成功は別。
主担当の次回統合試験で配置SYS hashとselected1007を照合し、新しい世代のcheckpointで
校正成功または具体substageの実失敗を確認する。無根拠な単独再試験は行わない。

DDI一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-iogetdiskdeviceobject
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-iogetdeviceattachmentbaseref
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwdeviceiocontrolfile

## 追加照合：GPT、READ制約、形式の根拠と限界

- 既存のWindows用GPT生成layoutと2026-11-06の検証済み保存snapshotは、nvramを
  5 MiB（40×0x20000）として保持している。Windows化で削除したlayoutではない。
  ただしrun47現在のGPTを新たに実機採取した確認ではない。driverはその保存offsetを
  固定使用せず、現在のdisk stackからGPT名/長さを取得して判定する。
- mainのSCSI codecは本branchと同一blob
  `29ead65ac56d4b226b77fc6db1c7f96d37ddf482`。
  READ(10/16)は容量内か、最大8sectorか、buffer長かを検証する。
  main miniport blob `a2d78d4e343551b5db0ebbc23429654e051982c1`でも
  Windows/ESP partition guardの条件はA04_SCSI_WRITEだけ。
  NVRAMの1sector READをWindows領域外という理由で拒否する条件はない。
  OS disk stackでの実read可否までsource確認で成功扱いしない。
- Linux gen3 `os/linux/platform.c`の`kalCfgDataRead16`は
  `/data/nvram/APCFG/APRDEB/WIFI`をfilesystemから読む。これはraw nvram GPT領域を
  ext4としてmountする処理ではない。Android側のlibnvramによる復元とは役割が異なる。
- Windows parserは既存XNU `package/wifi/a04br3_wlan_nvram.h`と同じ
  AllMap/AllFile container形式を扱う。0x20000 block/triple bitmap/0xaabbccdd、
  12+52byte header、140byte entry、WIFI LID7/514byte、全体/record checksumを照合。
  parserはext4 filesystem parserではない。異なる形式を推測で受理しない。
- 既存XNU runtime candidateの記録もhardware_verified=falseで、合成fixtureの成功と
  実機のraw container適合は別。今回も実個体NVRAMの全量抽出/公開は行わず、適合は
  Stage65の実read NTSTATUS/parser resultとStage66..68の実結果で判別する。
  raw read成功・parser不適合ならSTATUS_DATA_ERRORを保持して拒否し、defaultへ逃げない。
