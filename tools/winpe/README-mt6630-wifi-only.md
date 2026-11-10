# WiFi1029単独・既maintenance DISM媒体

既`run-mt8167-shared-io-update.cmd.in`のstorage/map/guard/journal/DISM/release/UART方式をWi-Fiだけに限定する。追加EXE/helper/service/native hive/fingerprintは生成しない。候補としてstageするのは署名済み`mt6630-combo-runtime.inf/.sys/.cat`の3ファイルだけ。FW/親/BT/GPU/boot/GPTは更新せず、診断clientは0。媒体実行後の自動再起動はない。

## 入力契約

- 正式1029 native `signed-manifest.json`をそのまま使用。`signature_status=Valid`、`signer_thumbprint=BCBF6C456C6F0DD0818434763B189488FBF13570`、固定source commit、3ファイルのhashを確認する。`signature_and_catalog_verified`という未定義booleanや変換manifestは要求しない。最終SYS/CAT署名とcatalog membershipの実verifyログ・hashはmainが独立照合してから生成する。native receiptからmembership成功を推測・合成しない。
- 実`private-windows-rt-prebackup-run206/manifest.json`のみ。status/read_only/unmounted/errors/実backup全hashと、9 hive/logおよび旧combo SYSの原本を確認。serialは`0C0U265083`完全一致。fresh205の改名流用、backup未取得の仮manifestは禁止。
- `--recovery-boot-id`はmainが現在維持するRecoveryのboot ID。controllerは書込み直前に既`capture-windows-prebackup.py verify`を実行し、同Recovery・同serial・partition identity・unmountedを確認する。builder自身は実機操作しない。
- 既maintenance baseのWIM=`0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4`、ESP=`22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce`、BCD=`65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d`を継承。実baseのPE SOFTWAREにはBCBF thumbprint key文字列がなかったため、同baseの既DISM方式どおり`/ForceUnsigned`を使用する。これはPE servicingの制約への対応であり、runtime CI・Windows trustは変更しない。新たな署名拒否実機試験は追加しない。

## package完成後mainが生成

```bash
python3 -B "$src/tools/winpe/build-mt6630-wifi-only-media.py" \
 --workspace "$root" --source-commit "$commit" \
 --base "$root/port/xnu/out/windows-rt-cpu-usb-audit-scope-v2-mapfix" \
 --tools "$root/port/xnu/out/windows-rt-tools" \
 --package "$signed_package" --package-manifest "$signed_manifest" \
 --package-manifest-sha256 "$signed_manifest_sha256" \
 --backup "$root/port/xnu/out/private-windows-rt-prebackup-run206/manifest.json" \
 --backup-manifest-sha256 "$fresh206_sha256" --recovery-boot-id "$recovery_boot_id" \
 --out "$new_media_directory"
```

`--check-only`を追加すると実入力の検証だけで、媒体を作らない。sourceはclean固定HEAD、出力はroot内の新規directoryのみ。SDK/compiler/VMは使用せず、既wimlib/ESP置換ツールを使う。

manifestキーはcontroller206と一致：`schema=MT6630_WIFI1029_OFFLINE_ONLY_V1`、`serial`、`esp_sha256`、`token`、`host_backup_manifest_sha256`、`install_targets=['wifi']`、`diagnostic_clients=[]`。`trust_added/runtime_CI_changed/firmware_changed/parent_changed/BT_changed/boot_written/gpt_written`は全false。

CMDはWindows volume guard後にtoken付き一回marker/journalを作り、同じtokenの再実行を拒否する。DISMは1回のみ。0/3010はservicing成功として記録し、release成功後だけ`UPDATE_STATUS=COMMITTED`と`UPDATE_EXIT=0`を報告する。release失敗はstatus変更・EXIT8、DISM失敗はEXIT6。journalには実host backupのhashを記録するだけで、新しいbackupを取ったとは主張しない。失敗時の自動retry/rollbackはない。

main controllerが既decoderのCRCと`COMMITTED/UPDATE_RELEASE_EXIT=0/UPDATE_EXIT=0`を確認し、Recoveryで通常ESPを戻して通常Windowsを維持する。servicing成功と実driver選択・開始・Wi-Fi接続成功は別。追加の通常OS診断を同梱しない。

ホスト検証：`python3 -B tests/mt6630_wifi_only_media_test.py`。実backup/署名receipt契約の拒否条件と、実cmdのlabel/IF/GOTO/SETを通したDISM0/3010/失敗・release失敗・既markerでの再適用拒否を確認する。transportはmockでありWindows cmd.exe/実DISM/署名の試験ではない。
