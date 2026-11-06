# 単一shared parentと周辺機器の一括更新

`build-mt8167-shared-io-update-media.py`は署名済みparent/audio/USB/Wi-Fiを一括でDISM適用し、固定vendor firmwareと任意の一回限りハードウェアprobeサービスを配置する保守媒体を生成する。ドライバ開始や実機動作の成功を媒体生成から推測しない。EDK2共通コードは変更しない。

入力は作業ルート、固定maintenance base、署名package、固定MT6630 firmware、直前のRecovery backup manifest、新規出力先、既存wimtools、完全source commit。`--probe-service`は署名済みMT8167HardwareProbe.exeを指定する。4 INFと単一親SYS、署名後全入力hash、WMT/WLAN既知hashを検査する。

SYSTEM/SOFTWARE/DRIVERSと必要なtransaction logsは、同じRecovery起動中にread-only mountからホストへ退避する。backup manifestのstatus=captured-unmounted、filesのbytes/hash、dirty hiveのlog存在を検証し、通常のPnP-only媒体へhive内容やserialを組み込まない。HardwareProbe付きは下記の明示private構成だけにclean候補SYSTEMを含める。旧WinPEで同じ大容量backupを再書込みしない。バックアップ取得後にWindowsを一度でも起動した場合は失効する。**writerは書込み直前にRecovery boot_idがmanifestと一致することを確認する。** 新UEFI bootのflashが必要なら、先にflashしてRecoveryへ入り、その後バックアップを取得する。

実機順序は新UEFI/profile準備→Recovery backup→媒体生成→同Recovery boot_id照合→ESP更新→CRC付きCOMMITTED＋release=0＋exit=0確認→通常ESPへ戻す→UARTで同一bootのPnP/IRQ/各機能確認。失敗または未完了のservicingで通常OSへ進めない。通常ESPへ戻す際も新しいshared IO profileのUEFIを保持する。

サービスはSystem32/MT8167HardwareProbe.exe、固定clientとログはSystem32/MT8167HardwareProbe/。LocalSystem/AutoStartで一回、試行前に無効化・markerを永続化。SSID/BSSID/鍵/個体校正をログへ出さない。USBキーボード、音声DMA、WLAN scan、通常音声endpointの成功条件はそれぞれ分けて記録する。

## HardwareProbeはnative-prepared SYSTEMのcopy-only

WinPEのprepare templateからSYSTEMのreg.exe load/query/add/unloadを除去した。
Tesla担当のnative recipeで **MT8167HardwareProbeだけ** を準備したclean SYSTEMを入力する。
コピーは4driverのDISM適用**前**に行い、DISMが追加したdriver登録を後から上書きしない。
WinPEのSOFTWARE証明書store更新と4driver DISMは従来どおりで、SYSTEMを直接RegLoadしない。
元のSYSTEM.LOGは候補と一緒に媒体へ持ち込まない。

`--probe-service`指定には次の追加入力をすべて必須とする。

- `--private-media`: private媒体の明示選択。出力はroot内・本drivers checkout外、新規directoryのみ。
- `--probe-manifest` / `--probe-certificate`: probe専用の署名manifestとDER cert。
- `--prepared-system` / `--prepared-system-manifest` / `--prepared-system-sha256`: native候補とresult.json、完全SHA256 pin。

probe EXE/manifest/cert、native候補/manifestはresolved pathでroot内を確認する。
候補/manifestとprivate出力を本drivers checkoutへ置く指定は拒否する。候補はmanifest内candidate.path（candidate/SYSTEMまたはSYSTEM）と
同じfile、またはresult.jsonと同じdirectoryへ回収したSYSTEMだけを許可し、
hash pin・manifestのcandidate SHA256・実file SHA256を照合する。平坦化回収は
relocated_after_native_preparationとしてprivate manifestに記録する。
状態はverified-clean-system-services-only、patch/verify workerのfinished/exit/status/load/unload成功、
clean regf1.5/checksum/sequence/extent、対象serviceだけの7値の型/内容を確認する。
Select・DeviceInstall/RpcSs直下の限定fingerprintがpatch前後とclean reloadで一致し、
source_filesのSYSTEM/LOG1/LOG2 hash/bytesが今回Recovery backupと一致することも必須。
全SYSTEM監査は行わず、whole_system_audit=falseとaudit_scopeをprivate媒体manifestへ残す。
Tesla側レシピの実装や現在適用中のprivate SYSTEMをこのbuilderが書き換えることはない。

probe署名は旧package/client署名と独立に扱う。probe manifest内EXE hashとsigning_requested、
signing_thumbprintを入力certのSHA1(DER)と照合し、旧certと新certの両方をそれぞれ
SOFTWAREのRoot/TrustedPublisherへ登録する。旧client2本は旧certのまま。
27ad206回収probeのcert thumbはBCBF6C456C6F0DD0818434763B189488FBF13570で、
旧packageの2655...と同一扱いしない。manifestのtrusted_certificatesはrole別に記録する。
このhash/cert照合はAuthenticodeチェーンや実機での信頼を検証したという意味ではない。

入力package directoryは丸ごとimportせず、検査したINF/SYS/CAT/client/certだけをstageする。
probeを含む出力はprivate_do_not_distribute=true、directory0700/file0600とし、
candidate SYSTEM・backup・private ESP/WIM・private manifestをrepo/CI artifactへ含めない。
候補のnative準備や主担当による実機適用の成功は、媒体生成成功と別に記録する。
旧tone clientを使うこの媒体はEnginePcm用ではない。

通常OSのScreenUart名はrun25でCreateFile error2となった。probeの最終UART通知が届く前提を
成功条件にしない。主担当がprivateなservice.log/子stdout・stderr/SCM終了結果を回収して
実行結果と通知経路失敗を区別する。builderは通常OSのUART device名やhiveを修正しない。

### 入力指定例（値はprivateな主担当記録から指定）

```sh
python3 tools/winpe/build-mt8167-shared-io-update-media.py \
  --workspace "$PROJECT_ROOT" --base "$MAINTENANCE_BASE" \
  --package "$SIGNED_TONE_PACKAGE" --firmware "$MT6630_FIRMWARE" \
  --backup "$FRESH_RECOVERY_BACKUP_MANIFEST" --tools "$LOCAL_WIMTOOLS" \
  --source-commit "$FIXED_SOURCE_COMMIT" --out "$NEW_PRIVATE_OUTPUT" \
  --private-media --probe-service "$SIGNED_PROBE_EXE" \
  --probe-manifest "$SIGNED_PROBE_MANIFEST" --probe-certificate "$PROBE_DER_CERT" \
  --prepared-system "$NATIVE_CLEAN_SYSTEM" \
  --prepared-system-manifest "$NATIVE_RESULT_JSON" \
  --prepared-system-sha256 "$EXPECTED_CANDIDATE_SHA256"
```

旧SYSTEM/LOGのnative入力は今回fresh Recovery backupと一致させる。
回収したcandidateはnativeのcandidate/SYSTEM配置、または同じresult.json横のSYSTEMだけ。
実機書込み前のRecovery boot_id照合とdiskログ回収は主担当が行い、このbuilderは実行しない。
