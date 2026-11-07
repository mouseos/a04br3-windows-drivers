# Recoveryからの更新前ホストバックアップ

`capture-windows-prebackup.py` はWindowsの更新前ファイルを、対象serialを固定したroot Recoveryからホストへ読み取り専用で回収する。Windows領域へファイルを書かず、修復・ハイブreplay・flash・再起動・adb root・工具のpushを行わない。RecoveryのRAM上に一意なmount directoryを作ることだけは必要。ホストはPython 3.11以上とadbが必要。

## 回収

指定serialは主担当の非公開設定から渡す。ソースに個体serialを記載せず、最初に見つかった端末を自動選択しない。全adb呼出しに `-s` を付け、get-state=recovery、get-serialno一致、UID=0を確認する。

```sh
python3 tools/recovery/capture-windows-prebackup.py capture \
  --serial "$A04_ADB_SERIAL" \
  --out "$TASK_ROOT/port/xnu/out/private-prebackup-new-run" \
  --target-sys mt8167-display.sys \
  --target-sys mt8167-ge8300.sys
```

`TASK_ROOT` は指定TAB-A04-BR3作業ルート。helperもソース配置からこのルートを確認する。出力先の親は事前に存在する必要があり、指定先自体は新規限定。ルート外へ抜けるsymlinkを拒否し、private directoryは0700、回収ファイルとmanifestは0600にする。

Windows aliasが `/dev/block/mmcblk0p20`、start=917504、size=29584384 sectorsであることを確認する。別領域を指定する引数はない。既存mountはroでも拒否し、他者のmountを勝手に解除しない。kernel NTFSを `ro,nosuid,nodev,noexec` でmountし、mountinfoでNTFS・対象source・roを確認してから読む。FUSEのmajor:minorがblock deviceと異なることも考慮し、source aliasも照合する。

Recoveryでkernel NTFSが使えない場合のみ、既にRecovery内にあるNTFS-3Gを主担当が明示選択できる。helperはバイナリを転送せず、失敗時に別方式やrwへ自動fallbackしない。

```sh
python3 tools/recovery/capture-windows-prebackup.py capture \
  --serial "$A04_ADB_SERIAL" --out "$TASK_ROOT/port/xnu/out/private-prebackup-new-run" \
  --target-sys mt8167-display.sys \
  --mount-driver ntfs-3g --ntfs3g-remote /sbin/ntfs-3g
```

NTFS-3Gには `ro,norecover,nosuid,nodev,noexec` を固定する。dirty filesystem等で拒否された場合、recover/remove_hiberfile等で強制的に進めない。

### 必須ファイル

`Windows/System32/config/` の以下9ファイルをすべて必須で回収する。

- SYSTEM、SYSTEM.LOG1、SYSTEM.LOG2
- SOFTWARE、SOFTWARE.LOG1、SOFTWARE.LOG2
- DRIVERS、DRIVERS.LOG1、DRIVERS.LOG2

DRIVERSはWindows 8.1の現DriverDatabaseを保持し、DISM AddDriverの更新対象となる。SYSTEM内の旧複製だけでstaging状態を判断しない。各hive本体が空、または9ファイルのどれかが欠落していればcapture-failedとし、省略した成功manifestは出さない。長さ0のtransaction logは存在を記録して回収する。ハイブは元のdirty sequenceのまま保存し、このhelperでは解析・replayしない。SAM/SECURITY等の無関係なハイブは回収しない。

`--target-sys` は `Windows/System32/drivers/` のbasenameのみで複数指定できる。任意パス・directory・raw LBAは指定できない。対象SYSが存在しない更新に限り、主担当が `--allow-missing-sys` を明示すると、その不存在を `missing_files` に記録する。registryの必須9ファイルにはこの例外を適用しない。

回収前にsourceの長さとSHA-256を取り、adb pullした各ファイルのホスト側長さ・SHA-256と照合する。終了前にsource全体の長さ・hash・存在とRecovery identityを再確認する。source内のsymlinkを拒否する。ホスト空き容量を確認し、1ファイル256MiB・合計1GiBを上限とする。

## private manifest契約

すべての回収・source再照合・unmount確認が成功した場合だけ `status=captured-unmounted` とする。

```json
{
  "schema_version": 1,
  "status": "captured-unmounted",
  "serial": "<非公開の対象serial>",
  "recovery_boot_id": "</proc/sys/kernel/random/boot_idの値>",
  "source_partition": "/dev/block/mmcblk0p20",
  "source_partition_start_sectors": 917504,
  "source_partition_size_sectors": 29584384,
  "source_partition_major_minor": "<Recoveryで確認した値>",
  "read_only": true,
  "unmounted": true,
  "errors": [],
  "files": [
    {
      "source_path": "Windows/System32/config/SYSTEM",
      "backup_path": "files/Windows/System32/config/SYSTEM",
      "bytes": 0,
      "sha256": "<回収前後とホストで一致したSHA-256>"
    }
  ]
}
```

上記はschema例で、bytes=0は実SYSTEMの有効値ではない。実manifestのfilesは9個の必須registryファイルと、存在する指定SYSを含む配列。serial・source_partition・boot IDはprivate manifest内にだけ保存し、stdoutへ出さない。stdoutはstatusとmanifestのSHA-256のみ。manifestは指定した新規出力先のmanifest.json。manifestと回収ファイルをESP、公開repo、CI artifact、Notion等へ埋め込まない。公開してよいのはhelperのソースと個体情報を含まない仕様だけ。

失敗時はファイルを削除せず、`capture-failed` のmanifestと回収済み部分をprivate directoryに残す。成功mount後も途中失敗後もfinallyで通常umountを試み、mountinfoで対象が未mountと確認する。force/lazy umountは使わない。切断・Recovery停止・kernel停止時に物理的なunmount完了を保証することはできない。その場合は成功扱いせず、主担当が接続状態と残mountを確認する。

## writerのwrite直前ゲート

主担当の媒体builderへ渡すのは**private manifestのSHA-256**。hive内容・serial・boot ID・partition identityをESPや公開ソースへコピーしない。writerはホスト側でprivate manifestを読み、実writeの直前に同じserial・同じRecovery boot ID・UID=0・同じWindows partition・未mountを照合する。helperのverify modeをそのゲートに使用できる。

```sh
python3 tools/recovery/capture-windows-prebackup.py verify \
  --serial "$A04_ADB_SERIAL" \
  --manifest "$TASK_ROOT/port/xnu/out/private-prebackup-new-run/manifest.json" \
  --manifest-sha256 "$PINNED_BACKUP_MANIFEST_SHA256"
```

このmodeはファイルを転送せず、mountもwriteも再起動もしない。固定manifest hashと必須9ファイル・回収内容の長さ/hash、現在のRecovery identityと未mountを検証し、成功時だけexit0と `verified-same-recovery-unmounted` を返す。writerは成功後に待機や別工程を挟まず、同じ対象指定のままwriteする。helper自体はwriterを起動しない。

**バックアップ後にWindowsを起動したら、更新前の証拠として失効する。** Recoveryへ戻ってもboot IDが異なるため、旧manifestを再利用せず新規captureする。RTC時刻やファイル時刻だけでは有効性を判定しない。Recovery再起動でも保守的に失効する。同じboot IDでも別プロセスがWindowsをrw mount・変更した場合や対象partitionを変更した場合は失効し、再回収が必要。boot IDだけであらゆる書込みの不存在を証明するものではない。

主担当のWinPE媒体は、固定したhost backup manifestのhashとwriter直前検証を根拠に、大容量hive backup copyを省略する方針。builderとwriterへの接続は主担当が実装する。検証結果を保持し、WinPE処理中に通常Windowsを挟んだり、別のbackupへ自動fallbackしたりしない。private backupは更新成功後も任意に削除しない。

Engine次反復ではcaptureの明示 `--engine-probe-state` が固定Engine attempted.bin/service.log/service EXEも回収する。
任意path指定は受け付けない。書込みやservice再armは行わない。
後続captureの `--compare-engine-media <private media manifest> --compare-engine-media-sha256 <hash>` は
回収SYSを媒体expected_installed_sysと照合し、不一致ならexit3、private比較JSONを保存する。
parent/audio/USB/Wi-Fiと採用display/TOPRGUの全basenameをtarget-sysへ指定する。
read-only回収成功と新package実配置一致は別statusとして保持する。
