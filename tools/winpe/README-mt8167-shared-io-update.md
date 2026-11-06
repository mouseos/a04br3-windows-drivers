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

## Engine次構成の明示mode

既定はToneのまま。`--audio-mode EnginePcm`を明示するときだけ、新しい固定service
MT8167EngineProbe用のprivate媒体を作る。MT8167HardwareProbeや旧markerを再武装/削除しない。
Engine compileしたservice EXE/manifest/cert、run27 fresh backup、EngineProbe単独native candidateが必要。
古い27ad206 serviceや旧tone向けSYSTEMはEngine modeで拒否する。

追加必須入力：`--package`は旧windows-rt-peripherals-f17b743-a016d53、
`--engine-package`はwindows-rt-engine-pcm-de214c8-e4dff2b、
`--engine-client-package`は署名済みwindows-rt-audio-engine-probe-d0eb436。
既存の--probe-service/manifest/certificateには新MT8167EngineProbe EXEの署名出力を渡す。
componentごとのhash/source/INF version/certを検査し、検査した固定ファイルだけをstageする。

| component | runtime source / INF source | DriverVer | cert |
| --- | --- | --- | --- |
| 単一parent | de214c8 / e4dff2b | 11/06/2026,2026.11.6.1001 | BCBF... |
| Engine audio | de214c8 / e4dff2b | 11/06/2026,2026.11.7.1004 | BCBF... |
| USB/Wi-Fi | f17b743 / f17b743 | 11/06/2026,2026.11.6.1001 | 2655... |
| WLAN client | 旧bundle f17b743（bundle KS fix=a016d53） | EXE | 2655... |
| WASAPI client | d0eb436 | package2026.11.7.2 | BCBF... |
| Engine service | この更新後の固定source | EXE | BCBF... |

旧parent・旧audio・旧tone clientはstageしない。Engine親の実capability flagと単一parentを確認する。
署名後SYSとunsigned parent-manifestのSYS hashは別物なので、signed manifestのhashを署名後の基準に使う。
別certをroleごとに記録しRoot/TrustedPublisherへ両方登録する。chain検証は実Windows側で別途必要。

Engineではnative prepared SYSTEMをDISM前にcopyし、4driverを親→Engine audio→USB→Wi-Fiの順に適用する。
SYSTEM RegLoad禁止・freshbackupの状態/hash/bytes/native入力由来照合・Recovery boot_id照合方針を維持する。
旧service結果だけでEngineが実行されたと推測せず、新serviceのrunid、marker、client別終了と
WASAPI/API/DMAのdisk記録を対応づける。可聴は未確認として扱う。

### Engine限定のoptional AddDriver

`--brightness-package`と`--toprgu-package`はEngine modeだけで受理し、未指定なら何も追加しない。
Toneへ渡す指定は拒否する。指定時は4driverの後に固定DISPLAY/TOPRGUだけをAddDriverする。
任意driver directoryを受け付ける機能ではない。

- brightness：windows-rt-brightness-0c73f27、INF11/06/2026,0.0.1.3、BCBF cert。
  normal liveFull flag維持は主担当申告として記録。bootbrightness341894cdを前提候補と記録するが、
  bootを媒体へ入れたりflashしたりしない。boot採用・resource確認は主担当が別に行う。
- TOPRGU：windows-rt-toprgu-normal-uart-0821853、INF11/06/2026,0.0.11.0、BCBF cert。
  4 CPU/HANDOFF1/DEADLINE0/THERMAL0/UART1と旧normal同一7 kernel git blobの宣言を検査し、
  manifest/packageファイルhash/bytes/certを確認する。sleep/resumeやUART実在を検証済みとはしない。

それぞれcomponent別source/hash/INF版/certと前提条件をmanifestへ残す。BT c715ad2はWi-Fi排他profileの
別候補であり、今回のEngine/WLAN選択へは取り込まない。

```sh
# 前述のprivate/freshbackup/native引数を維持し、次を明示追加する。
--audio-mode EnginePcm --engine-package "$SIGNED_ENGINE_DRIVERS" \
--engine-client-package "$SIGNED_WASAPI_CLIENT" \
--brightness-package "$SIGNED_BRIGHTNESS_CANDIDATE" \
--toprgu-package "$SIGNED_NORMAL_UART_CANDIDATE"
```

Source-only実装であり、Engine service WDK link/署名、native Engine準備、媒体生成、実PnP、
API/DMA/音声・UART・brightness実機確認は主担当の次工程。current run26r2の媒体は変更しない。

### Engineの通常OS PnP開始

Engine prepareは、選択・hash検証済みaudio/USB/Wi-FiのINF/SYS/CAT各3個も固定の
System32\MT8167EngineProbe\drivers\{audio,usb,wifi}へcopyする。
新service manifestに固定3IDs/flags=5/各1回/parent除外/reboot時client停止/CAT署名必須の
契約が無い旧Engine serviceは入力検証で拒否する。旧tone serviceは変更しない。
新serviceのCAT検証後、標準PnPの固定再bindを行う。3010は成功扱いせず後続を停止し、
markerを残す。before/after CMとAPIのdiskログを主担当が回収する。
Engine親と旧親は同版2026.11.6.1001なので、単一parentの選択と実ロードhash一致は別の確認事項。
親のUpdateDriverはこのserviceで行わない。詳細とevent番号はhardware-probe READMEを参照。
