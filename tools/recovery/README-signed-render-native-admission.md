# signed render native plan終了契約

native planは旧terminal holdか、新しい厳格なunattended collectionを受理する。旧holdではcollection/snapshotを有効化できない。新経路ではcapture237以上、SIGNED_RENDER_UNATTENDED_COLLECTION_V1の同capture・bounded WinPE・atomic publication、厳密CONFIG、hold=falseを要求する。既collection_reboot.policy_config()を再利用し、phase1のgpu_present/gpu_globaldiag、ゼロfirst/deferred mask、snapshot契約を照合する。追加でbool/int混同を拒否する。

prepare-offline-system-service.ps1の署名render admissionだけを変更した。既accepted GPU lock、candidate/retained package、boot/GPT0、サービス名/plan SHA、7valueサービス書込み、SYSTEMの関連fingerprint比較、load/unload/privilege復元検査は維持する。helperはpin済みpolicyの実byteをSHA照合してからparseする。hive/端末操作は行わない。

hostテスト（repo rootから。出力先は毎回新規）:

```text
python -B tests/signed_render_native_admission_test.py --emit-fixtures build/native-admission-mock-r1
```

Windows mock gateは同じfixtureと実PowerShell admission関数を呼ぶ。hiveをloadせず、SYSTEMを変更しない。main専有Windows環境で実行:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File tests/signed-render-native-admission-mock.ps1 -FixtureDirectory build/native-admission-mock-r1
```

36cases（旧hold・明示disabled・新collection・snapshotのpositive5、契約/世代/phase/client/mask/config不整合negative31）に加え、Windows入口ではpolicy SHA不一致も拒否する。host fixtureで生成したpolicyはテスト専用で実配備入力ではない。実241 policyを検証する場合はhost testの `--actual-policy` を併用する。

VM操作はmain担当。Linux hostでのPython検証とWindows mock gateの実行は区別する。r2の失敗nativeoutは保持し、新source・新outでnative patch/verifyの全lifecycle0・元trio不変・無関係fingerprint不変が実確認されるまでnative修復完了とは扱わない。
