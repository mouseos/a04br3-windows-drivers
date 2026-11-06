# 搭載Windowsへの共有親・BU21025タッチ導入

`run-update-shared-touch-once.cmd` はWinPEから搭載Windows RTへ、MediaTek MT8167 Shared Resource Controller（`ACPI\\BEN0001`）とROHM BU21025タッチ子デバイスをDISM `/Add-Driver` で追加する。

- 対応付けは `map-offline-windows.c` の 2da263c 以降のビルドが必要。古いビルドは `ERROR_NOT_READY` で `VOLUME_MAPPING_REJECTED` になる（2026-11-06 実機で確認）。
- WinPEのMSDCプローブ経由の書込みは約6 KB/sと遅いため、端末内でDriverStore等を退避しない。ロールバックは書込み前にAndroid recoveryから取得したWindowsパーティション全体のホスト側イメージで行う。
- 中断で残った `C:\\a04backup\\shared-touch-v1`（STATE=BACKUP_STARTED）は削除してから続行する。それ以外のジャーナルがあれば変更せずに終了する。
- 結果は `uart-audit-stream.exe` の CRC 付き UART で回収する。表示診断の非同期出力と混ざらないよう、startnet は監査前に90秒待つ。
- `build-shared-touch-update-media.sh` は検証済みCPU/USB監査媒体を入力に、ハッシュを照合して媒体を作る。
