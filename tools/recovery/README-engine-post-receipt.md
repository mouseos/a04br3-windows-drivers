# 固定Engine post receipt（native SYSTEM replay）

端末・runnerを自動操作しない準備/読取り/decoder。mainがobserver終了とfresh captureの
captured-unmounted/read_only/errors=[]を確認してから実行する。captureの兄弟非列挙scopeと
過去のEIO記録を維持し、全volume健全性・全driver成功を主張しない。

run47: policy `aa568ae666acbdb26f9740260cfe871a4399403bd2510cf4cc9743f328fdb0fa`。
Schema2/TargetCount6/ClientCount3、Node5=ACPI\MOU0013。GPU2.5は更新対象外。

## fresh48実pinを用意

```sh
python3 -B tools/recovery/prepare-engine-receipt.py \
  --backup /ROOT/port/xnu/out/private-windows-rt-prebackup-run48/manifest.json \
  --backup-sha256 ACTUAL_FRESH48_MANIFEST_SHA256 \
  --policy-hash aa568ae666acbdb26f9740260cfe871a4399403bd2510cf4cc9743f328fdb0fa \
  --out /ROOT/port/xnu/out/private-windows-rt-run47-post48-inputs
```

ROOTは実作業ルート、48hashは回収後の実file hash。placeholderのまま実行しない。
準備ツールは3hiveの元hash/bytesを照合し、private ZIP・固定native parameters・tool/source
hash provenanceを新規root内outへ作る。ZIP/hive/receipt/生stdoutをrepo/CI artifactへ入れない。

## 既存native loaderで読む

provenanceのsource commitを既存runnerの新しい専用dirへ固定checkoutし、既存の
SHA固定immutable helper方式で実行する。共有upload helperを上書きしない。
ZIPのhash確認/専用inputsへの展開後、native-parameters.jsonの固定8引数を
`tools/recovery/read-fixed-engine-receipt.ps1`へ渡す。

同scriptは3hiveを新しいprivate-replayへcopyし、既存OfflineSystemHiveNative.csの
RegLoadKeyでreplayする。Select/Current、固定serviceのpolicy世代/2phases、
Mt6630Runtime/Parameters/NativeCheckpoint、Mt8167Audio/Parameters/NativeStartだけを読む。
live registry/保護PnP Properties/全SYSTEM走査・任意registry path・service操作はない。
root/全child handleをclose→RegUnLoadKey→privilege restore→元3file不変確認。
Load/Unload/restoreとエラーをreceiptへ保存する。欠落世代はpresent=falseであり旧世代へfallbackしない。

読取りstatusがfixed-receipt-read-unloadedでもdriver成功とは限らない。phase Complete、
RunId、各valid bit、CM status/problem/NTSTATUS、各client exitを別に評価する。

## decode

```sh
python3 -B tools/recovery/decode-engine-receipt.py \
  --receipt /ROOT/port/xnu/out/private-windows-rt-run47-registry-post48/receipt.json \
  --receipt-sha256 ACTUAL_RECEIPT_SHA256 \
  --policy-hash aa568ae666acbdb26f9740260cfe871a4399403bd2510cf4cc9743f328fdb0fa \
  --out /ROOT/port/xnu/out/private-windows-rt-run47-registry-post48/decoded
```

全3client stdout/stderrの元bytesをphase別fileへ保存（最大各65536、truncatedとerrorを残す）。
brightness clientのstdout teeもClient2Stdoutから回収する。root logやsiblingsの列挙は不要。
VolumeFlushは7DWORD/28Bでattempted/identity/open/flush/close/elapsed/returned、別Win32 error。
file/registry flushとvolume flush成功、NTFS clean unmountを混同しない。
Audio1007はNativeStartComplete＋14DWORD/56Bを確認し、stage/resultとDMA情報を記録する。
WiFi1006はTesla bafafa9の正本decoderで全64eventを型付きdecodeし、WMT/WLANのread/hash/
select/boot、board/control/calibrationのsubstageを保持する。driver_build不一致は明示し、
採取できた旧checkpointを新1006の観測へ読み替えない。baseline generationが分かる場合だけ
--wifi-baseline-generationを渡す。未指定時のfreshnessはunknown。

Schema1の5node/6GPUも旧profile指定で読める。新Schema2の最大7nodeも既存binary layoutを
維持。run47 profileではNode5をGPUと誤表示しない。Audio1006等の旧読取りはdecoderへ
--audio-revision 1006 --wifi-build 1005を明示して、post47と混ぜない。
