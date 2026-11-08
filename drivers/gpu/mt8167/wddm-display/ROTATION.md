# MT8167 CPU display DOD 1.7

基点はCPU表示・brightness有効のDOD1.6 (`6a99ec7d`)。GE8300 render driver、UEFI、タッチ、センサーは変更しない。

D3DKMDTの回転は反時計回り。論理画素 `(x,y)` と物理寸法 `(pw,ph)` に対し、0/90/180/270度の出力はそれぞれ `(x,y)`、`(y,ph-1-x)`、`(pw-1-x,ph-1-y)`、`(pw-1-y,x)`。既存Microsoft由来GetRowStart/GetPitchesを使用する。90/270では論理幅・高さを交換し、物理pitchは交換しない。source modeは800×1280と1280×800を列挙し、具体的なsource/pathの組は固定POST寸法へ収まる場合だけ受理する。

PresentのRotateフラグが0なら再回転しない。その場合の入力は物理向きの寸法として検査する。常に与えられたPitchを検査し、全source spanをMDLで保護する。dirty rectとmoveのDestRectを同じ最終source画像からコピーする既存KMDOD契約を保持する。mode/path切替後は次presentを全面更新し、成功時だけ要求を消費する。中央配置は物理領域のoffset、回転はその内側の画素変換として別々に計算する。

hardware cursorはadvertiseしない。software cursorはOS合成済みsource画素と同じ変換を受ける。BU21025はnative panel絶対座標を一度だけ生成し、表示への対応はWindowsのdisplay rotationに任せる。DODからtouch/sensorの校正や座標を変更しない。XNUのTouchState.cppは入力共有領域と通知の実装であり、この機種の表示回転仕様の根拠にはしない。

根拠:
- [PresentDisplayOnly](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_present_displayonly)
- [Rotate flag](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_d3dkmt_present_display_only_flags)
- [D3DKMDT rotation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmdt/ne-d3dkmdt-_d3dkmdt_vidpn_present_path_rotation)
- CT3 Linux `drivers/video/console/fbcon_cw.c`, `fbcon_ccw.c`, `fbcon_ud.c`: 非正方形の寸法交換と端からの矩形変換。ただしCW名称をWDDMのCCWへ直結しない。

再現:
```sh
python3 tests/dod_rotation4_test.py --output build/dod-rotation-host
```
ホスト試験は実bltfuncs.cxxのWindows宣言部分だけを置換して実行する。SEH/MDL、Windows上のVidPn交渉、実touch/pointerはこの試験では確認しない。

Windows ARMビルド・署名:
```powershell
.\tools\ci\build-mt8167-display-rotation4.ps1 -EnableCandidate -KitRoot <existing-wdk81> -VcToolsRoot <msvc-tools> -UcrtIncludeRoot <sdk-include-version-directory> -Inf2CatPath <existing-Inf2Cat.exe> -SignToolPath <existing-signtool.exe> -OutputDirectory "$PWD\build\rotation4"
```
既存小closureだけを読み、専用出力へDOD11TUをcompile/linkする。全GPU実験flagは0、brightnessは1。SYS/CAT署名・署名者・CAT membership・ARMNTを検証し、objectsも回収可能にする。cloneした固定sourceと既存Windows SDK/WDK/MSVC、既存BCBF署名identityが必要。SDK/PCOの自動install/buildはしない。

実機4方向表示、OSのmode切替・touch/pointer、長期動作はmainの次周辺一括採用時の確認項目。ビルド候補を実機動作成功とは扱わない。

EnumCofuncではrotationがpivotの場合だけそのsupportを保持する。旧コードの `EnumPivotType != D3DKMDT_EPT_ROTATION` を `==` に修正し、その他の場合に4方向supportを更新する。path更新時はblackoutに成功してから新しいrotation/scalingを反映する。

同じcheckout位置・同じcompiler/header/flagsのobject receiptを回収してある場合は、ビルド引数へ `-ReuseReceipt <object-receipt.json> -ReuseReceiptSha256 <sha256>` を追加できる。receiptそのもののhash、全project/kit/VC/UCRT header群、compiler、feature flags、各source/object hashを照合し、sourceが変わったTUだけ再compileする。入力が異なるreceiptは拒否する。過去sourceのobjectと最終sourceの関係はsigned manifestのobject_provenanceに記録する。
