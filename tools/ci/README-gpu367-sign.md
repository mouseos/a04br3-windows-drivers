# GPU367 sign-only

署名のみ。compile/link、runner/実機、cold条件、StageOnly/DIFを操作しない。
親sourceは3d4a3d1097b7f6a796972e0425e37d0263b2b070。metadata recipe c30とは別branchであり、mainが必要なsourceをまとめる。

Windows上でmainが、各変数を既存の実ファイル位置へ設定して実行する。Outputはこのcheckout内build/out配下の新規ディレクトリ。既存署名済み入力やplanは上書きしない。

```powershell
python -B tools/ci/sign-ge8300-integrated367.py --baseline-package "$Baseline366Package" --linked-directory "$Linked197Directory" --integrated-manifest "$Metadata197Directory\integrated-build-manifest.json" --integrated-manifest-sha256 5bd6410045fa5feb213bb9b1a39a4750357410b5093c2735397f6ed3bdf06c65 --main-review "$Metadata197Directory\main-artifact-review.json" --main-review-sha256 67e03bdda4091103ed4bbed872e537bb51dd58c81aa0afc48541d4e524f9d657 --output "$PWD\build\gpu367-signed" --sign-tool "$ExistingSignTool" --inf2cat "$ExistingInf2Cat" --sign
```

baseline366はoutputs366.zipのpackage366を展開したディレクトリ。manifest deca993f...と宣言された全14 leafを固定pin照合。SYS482f4325...／UMDebba6b7a...、metadata5bd...／main review67e...も完全hash照合する。--signを外すとpin照合・plan保存だけで、署名toolは実行しない。

baselineの全output名を保ち、更新するlinked binaryはSYSとUMDだけ。root INFは意味を変えずCR蓄積を単一CRLFへ正規化し、DriverVerのみ11/10/2026,0.0.3.67へ変更。新render CATを生成する。FWと旧display-transport三点はbyte同一保持し、新linked transportは採用・署名しない。

既存BCBFでSYS/UMD/CATを署名し、SignTool trust/CAT所属とPowerShellの証明書identityを検証する。全出力hashを記録し、INF記載のSYS/UMD/FW/INFと旧transportのCAT所属を確認する。補助exe/lib/cerはbaseline hash保持であり、新CATに載ると偽らない。署名のPE変更はchecksum/security directory/certificateだけであることも照合する。

出力package367/signed-manifest.jsonは新schema GPU367_SIGN_ONLY_FROM_LINKED197_V1。package_source_commit/source_commitはcompiled3d、recipe_source_commitは署名helperの実HEAD。runtime_build_id=GPU359-start-gap-v1とe072 identity sourcebaseは3dソースとunsigned SYSに実在することを確認済み。実source provenanceとは分離する。旧compiled_unitsはコピーしない。afterfailure=true、historical include trace=falseを保持する。

SYS/UMDのunsigned入力hashとsigned出力hashはbinary_signing_transformsへ分ける。全生成ファイルのhash一覧はoutput-file-hashes.json。実署名は未実行。署名後のmain独立status signed-artifact-verified-not-hardwareは本helperでは発行せず、mainがmanifest/SYS/UMDを独立照合して作成する。
