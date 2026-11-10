# 固定POST出力の未指定変換

WindowsのVidPN rotation/scalingのNOTSPECIFIEDはminiportが具体値を選ぶ契約。
既存のIsVidPnPathFieldsValidはこの値を許すが、unifiedのA04PathFitsは
IDENTITY以外を拒否していた。固定800x1280のunified経路では未指定だけを
IDENTITYへ解決し、支持判定とcommit/update時の実保存値を一致させる。

UNPINNED、UNINITIALIZED、実回転、拡大縮小、異なる寸法の許可は広げない。
DOD経路と正当なPOST解放による停止条件も変更しない。

実369でハードウェアD3Dデバイス作成は成功したが、同adapterの出力列挙は
DXGI_ERROR_NOT_FOUNDだった。この変更がその原因を解消したかは未確認。
実際のpath値と同bootのPOST解放記録を照合し、正式増分ビルドと実機結果で
判断する。通常デスクトップ描画の成功をsource変更だけから主張しない。


ビルドは`build-ge8300-bdd-vidpn-compile.py`で変更BDDの1TUだけをコンパイルし、
`relink-sign-ge8300-vidpn371.py`で実369のarchiveへそのobjectだけを戻す。
archive memberの順序と他memberの全byteを検査する。renderだけを0.0.3.71へ
更新し、transport369・UMD・FW・既clientは保持する。別DWM候補370と混同しない。

リンク入力は実369のnative archiveと、新たに固定したWDK81の5library。
`ge8300-vidpn371-kernel-inputs.json`の全file hash/sizeをlink直前直後に検査する。
displibは保存369 hashと一致するが、他4libraryの歴史的同一性は記録不足で
未確認。imports一致を実装byte一致の代わりにはしない。receiptにはこの範囲、
input-lock SHA、元BDD compile receipt、実link/sign引数を保存する。
SDK・PCO・native・UMDを再構築しない。署名成功と実機出力成功を区別する。
