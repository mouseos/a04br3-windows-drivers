# Verified native compile checkpoint（既定OFF）

通常entry `build-ge8300-from-source.ps1` に次の4引数をすべて明示した場合だけ、native67のcompile済みobject/libraryを再利用する。

```
-VerifiedNativeCompileCache C:/private/native.zip
-ExpectedNativeCompileCacheSha256 <native.zip SHA256>
-NativeCompileInputSnapshot C:/private/current-inputs.zip
-ExpectedNativeInputSnapshotSha256 <current-inputs.zip SHA256>
```

`-UnifiedDisplay`も必須。不完全な引数、cache不整合は停止し、cold compileへ黙ってfallbackしない。既定の引数なし経路は従来のnative67 compile/KMD/UMD/linkを維持する。既native builderの`ReuseVerifiedDirectory`と`LinkCheck` guardは一切変更しない。cache再利用後もmodules/KMD/UMD/clientは現sourceから通常処理を行う。今回はsource-only変更でありWindows通常wrapperの実行成功を主張しない。

## admission

`ge8300_native_compile_cache.py`はcompilerを起動せず、2archiveの明示SHA、CRC、安全なpath、全indexのsize/hash/file集合を検査する。旧451source（native builder含む）、Mesa source plan605file、map/source-plan/asset、67unit/source/order/実flags、ARMNT67objとlibrary67memberの逆順を照合する。witness_codec/infoの2TUはGNU targetを含むsource-plan由来のflagsを別に検査し、共通65TUのflagsで代用しない。

live admissionでは現在の6外部include rootをsnapshotと相対path集合/size/hash全一致で比較し、rootの実target、nested reparse拒否、tool3exe、WDK4lib/ntddk、source cleanを検証する。7番目のgenerated rootは固定asset JSONとrecipeから決定的なheader bytesを求めてsnapshotと照合し、そのbytesだけを新native出力へ復元する。C/header/ABIを変更する経路はない。

このschemaは実379の67TU＋同じlayoutに限定する。現在repo/output/include/toolのabsolute Windows pathを旧実argsと照合し、separator/caseのWindows上の同義だけを認める。任意のcheckout/output移設は`__FILE__`やinclude探索が未証明なので拒否する。新runnerでは同じtask pathへsource/inputを復元してfresh outputを使う必要がある。現在のCPATH等の暗黙include/config環境が非空なら拒否する。過去env取得済みとはしない。

検証成功後だけ旧nativeの全outputs/manifests/logを新native directoryへbyte不変でcopyする。旧source_commitを現commitへ書換えない。新receiptは別fileでcompiled source/current source・closure一致・reuse67・aftercompile snapshot/pastenv未取得・LinkCheck requested/completed=falseを明示する。

wrapperはguard exit0かつlive receiptのadmitted/current source/closure/67を確認し、receipt hashを保持する。最終source確認で旧native sourceを受理するのはこの場合だけ。現KMD/UMD source確認は従来どおり。旧native両manifest/libraryとreceiptのhashを最終時点でも確認する。最終manifestはnative componentのcompiled source、reuse67、previous_driver_objects_used=true、receipt hashを保存し、新compileと偽らない。

## 限界・検証

証拠種別は固定taskの展開起源＋before-nativeの正式PCO gate＋aftercompileの全root snapshotである。native各TUのcompile時dependencyや過去wrapper envを新たに証明するものではない。旧crt archiveとのUCRT差は実22621 snapshotを正本として扱い、別版へ置換しない。LinkCheck未実行を成功へ昇格せず、描画・実機動作とも分離する。

`--audit-only`は手元のarchive/source/static flags/ARMNT/member順のみ検証する。live Windows root/tool/libs検証・object復元は行わず、receiptは必ず`admitted=false/objects_reused=0`。通常wrapperはこれを受理しない。Windows実行とPowerShell5でのlive動作は未検証。Linuxで実native＋snapshotの正例とMesa header不一致拒否、PowerShell parserを確認した。
