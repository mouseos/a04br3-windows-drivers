# fresh196からのGPU367一括更新profile

serviceは`MT8167CombinedFollowup196`、試験はrun197、回収はcollector198。
prepare196、combined196 facade、service196 build入口、mediaの
`--combined196-profile`、read-combined196-post、preservation verifier、collector198は
既194経路を引き継ぐ。

fresh196のSHA256は
`7e6c1316961a1dc0807595552434488ff0efa43d3f43629bea8aef599a2ff8d2`、
Recovery boot IDは`18658914-6059-4b77-aa12-97f21f8e18e3`。
42原本、SYSTEM三本、旧18service全体、従来120named値と旧service194の5値を
検証する。POST DWORD0、boot0943、parent1120、thermal1.11、DIS0 transport357、
USB keyboard1010、BT2018、Wi-Fi1027など旧対象を保持する。boot/GPT書込みは不要。

GPU367は統合source `3d4a3d1097b7f6a796972e0425e37d0263b2b070`、
INF `11/10/2026,0.0.3.67`、実SYS/INF/CATとmainの署名成果物照合receiptが
一致するまでfinalizeを拒否する。receiptはmanifest SHA256とSYS SHA256を結合する。
既存UMD/FWは実baselineとbyte同一を要求する。runnerのビルド開始やplaceholderの
mapだけを成果物完成・署名成功・実機成功の根拠にしない。

`prepare-peripheral-followup196.py --audit-only`は原本保持の監査結果だけを作る。
arm用policy/header/mediaは作らず、`normal_install_allowed=false`とする。
`--finalize`とは排他。署名leafが未確定なら、mapのGPU367二参照をnullのまま
残せるが、build/media入口はこれを拒否する。

## USB1121/HCD1019を今回含めない理由

既署名tupleはsource `6741f2a1cac2cb6622c3b1d49bd2ac118c76471f`、
owner ABI3、parent1121/HCD1019。6ファイルの実hashはmanifestとmain reviewに一致する。
ただしHCDの対象は`MT8167\USBPORT20`、service `MT8167MUSBHcd`、USB class。
fresh196で確認した既USBは`MT8167\USB20`、service `mshidkmdf`、HID class、
LowerFilters `A04Usb20Keyboard`、version1010。HCD SYSはcaptureのmissing記録にある。
新PDOの状態は既13対象のnative receiptで捕捉されていない。

既194のtarget構造は同一HWID/service/classの既選択tupleを前提にする。
そのUSB slotを新HWIDのHCDへ差し替えることは、旧実機baselineの保存・照合を
省略することになる。今回のinstall maskはrenderのみで、親だけの片側更新も行わない。

同時適用には、合成USBPORT20 PDOの実在・欠落状態、親1120とkeyboard1010の
実tuple保持、新HCD serviceの初回導入、WDF/USBPORT依存、旧keyboardとの排他と
正常な退役を扱う専用のbaseline/first-install契約が必要。
manifestのABI3一致や署名成功だけではこれらの実機条件を証明できない。
強制host、source ON、GPIO実験はprofileへ追加しない。

## 検証範囲

`tests/test-combined197-policy.py --workspace <root> --map <pending-map> --out <新規出力>`
は実fresh196の保持監査、未確定/旧GPUleaf/異なるcapture/USB targetsの拒否、
既tupleによる生成処理、Python構文とcollectorのbash構文を確認する。
Windows操作・ビルド・実機試験・REAは行わない。完成した367のfinalize、
service署名、媒体生成、書込み、run197の結果は別工程である。
