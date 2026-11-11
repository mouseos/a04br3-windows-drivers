# wire378: private queryの厳密owner4契約

実377 SYS b4a2a17fのlink mapでGeoQueryAdapterInfo=0x0040a840を特定。
REA ev_09527d2cceda5c87403e1c4285bd50f9d97d2bca2120ff67ca290bff5e1b5298のcase0は
96byte/版8/owner4/control2/magic31494f47/format21/vertex65536を組み立て96byteコピーする。
保持UMDはowner3だけを受理し、実226同evaluator09e4でquery成功後ADAPTER_CONTRACT_FAILURE1。
他の動的field不一致の併存や、この修正後のD3D/Present成功は未検証。

adapter_info.hへpointer-free wire v8のtuple/96byte layout/失敗maskを共通化。
wire owner4をnative owner ABI4とcompile時に一致検査し、3を偽装せず厳密拒否する。
native owner構造、動的aperture/容量/draw値、feature/caps広告は変更しない。
typedは既ログにADAPTER_CONTRACT_MASKを出力し、両UMDは失敗時だけ実fieldをumd-wire-PID-TICK.logへ保存。
既collect-combined200.shの^(clear|umd)-[a-zA-Z0-9_.-]+\.log$が受理する。追加診断stage不要。

実headerを呼ぶtests/adapter_wire.cは17境界を検証。producer静的定数のfixtureであり、runtime payloadを捏造した証拠ではない。

tools/ci/build-sign-ge8300-wire378.pyは既archive parser/signature helperを再用。
固定ge8300-wire378-inputs.jsonは実377 kernel/native/package、実30cb UMD archive/entry、
元3TU compile receipts、実30cb compiler-adaptersの10lib/pairにhash結合する。
query_adapter.c + typed_entry.c + frontend.cだけcompileし、KMD archive1member/UMD archive1memberを置換。
archive順序・全他member SHAを維持しSYS/DLL各1link。PCO/SDK/native/transport/FW/client再build0。
新depsを各TUの.dから保存し、外部headerの歴史的同一性は未確認のまま明記する。

署名は378 SYS/DLL/CATだけ。INFは正式0.0.3.78へ更新し、CATでINF/SYS/DLL/FWを照合する。
transport372 trioと他全package leafを保持。DLL export名列が旧と同一であることも要求する。
成功したcompile/link/signのstepはhash付きreceiptで保持。--resumeは成功stepを再実行しない。
失敗/uncertain stepは自動再実行せず、logと実成功物を保存したまま担当が境界を確認する。

Windowsの既runner、既WDK/署名identityを使用する。新sourceをclone固定・cleanで実行する。
--executeなしは入力/tool全照合とplanだけ。--executeでbuild/signを開始する。
成果物は新checkoutのbuild配下だけ、端末操作・hive操作は行わない。
