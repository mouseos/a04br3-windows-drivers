# 診断終了後の明示collection reboot

既定 `A04_COLLECTION_REBOOT=0`。現run232・9917署名品・媒体・sealを変更しない。
actual233未回収のため、正式次profile・SCM署名品・実機投入の準備は未完了。
新captureの値やRecoveryを仮置きせず、既known-profile whitelistは拡張しない。

次のactual-profile adapterは、通常の全baseline/候補検証後、policy hash計算前に
`enable_next_profile(p)`を呼ぶ。`draft=false`、main承認、`capture_number>231`を
要求し、profileへ`-collection-reboot-v1`と固定CONFIGを付す。pendingは拒否する。
そのactual生成入口・host contract whitelistはactual233取得後に一度だけ正式化する。
共通header generatorはこのCONFIGから2macroを生成する。

正式buildは既recipeの `-CollectionReboot` を明示し、通常variantの期待profile名に
上記suffixを加えた正確な一致とCONFIG検査をcompile前に要求する。
新headers・Python helper/decoderはcompile前後hashをmanifestへ保存する。
mediaも同closureと生成headerを比較し、実service manifest参照を出力する。
main sealにはcollection CONFIG、service manifest、`source_closure(repo)`の全leafを
workspace相対pathでpinする。hostはrepo相対refをroot内実体へ正規化して照合する。
`--collection-reboot`がなければopt-in profileは拒否する。既profileへflagだけ追加も拒否する。

client二つのcreated/WAIT_OBJECT_0/exit_valid、独立I/O cleanup、final snapshot、
finalize、terminal log flush/close、非cancel/非readonly_uncertainを要求する。
診断結果1/4等はそのまま保持し、3010に置換しない。停止SetServiceStatus成功後だけ
readyをsignalする。継承handle-listは実親process/ready/cancelの3objectだけ。
waiterは実親終了・exit・SCM停止結果・同run/PID/policy・永続onceを照合する。
署名検査は既signed_imageに加えて固定BCBF signerを必須とする。

独立proofはlittle-endian216B、magic0x31524341/version1/bytes216。
14DWORD＋policy lowercase ASCII64B＋client2×40B＋reserved16B。
既CombinedResult schema2の36B clientを変更しない。
`decode-collection-reboot.py proof.bin`は値を読取るだけでshutdown/実機成功を認定しない。
collectorは `logs/collection-reboot-proof-v1.bin` をexact216Bで回収する。

waiterは既InitiateSystemShutdownExW経路を一度だけ呼ぶ。
hostは同run/PID/policy/result/API0のUART proof後、専用process groupとforeground timeoutで
対象serialのfastbootを最大180秒待つ。外側probe timeoutは未検出として同deadline内で継続。
異常・期限終了時はRST/retryせず履歴を保存。collection経路のrecovery commandには
UART reset命令を含めない。v1は初回実行から自動回収までの経路のみで、旧collect-onlyと併用しない。

検証: `python3 -B tests/collection_reboot_test.py`。
root内ログのWindows compileはテスト専用headerコピーでSCM1TUのみ。
正式profile/署名品ではなく、link/sign/deviceは0。実機shutdown成功は未検証。
