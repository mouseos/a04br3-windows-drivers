# MMA8652 1.10の座標契約と未確認事項

## 現在の候補と採用状態

現1.10はraw→board=(-ry,-rx,-rz)、board→HID=(-boardY,boardX,boardZ)、
合成HID=(rx,-ry,-rz)。run115/fresh116の選択1.10とSYS998329ec...は署名候補と一致する。
これは1.8/1.9のHID出力からXYだけ180度を反転した候補であり、Z/校正/基板軸は保持する。
最新ユーザー申告「今度は上下逆」は先行HANDOFFでは搭載1.8への申告として扱われ、1.10に対応済み。
この申告が1.10採用後にも再現したことの証拠は現保存資料にはない。追加の推測反転はしない。

今回修正したのは旧1.8期待値のまま残っていたtest-hidの2fixtureと本資料だけ。
テストの合格は実sourceの1.10行列・report契約の確認で、物理上辺の確認ではない。
runtime/INFと既署名1.10は変更せず、新SYS再build/署名/版更新は不要。

Linux direction7は基板mountの一次根拠。XNU IOHIDEvent共通実装は渡されたXYZを保存するだけで、
本機のWindowsパネル上下を確定しない。800x1280のnative portrait寸法だけでも180度の上辺は
識別できない。署名DOD1.6 source6a99ec7dの180/270回転未対応は別の表示契約であり、
sensor符号を変えて代替しない。表示変更はmain/Huygens owner側で扱う。

## 過去の1.8修正記録（履歴）


## 原因と修正

Linuxの取り付け補正とWindowsのパネル補正は別の変換である。
`mma8652.c` はraw値をLinux direction 7の基板座標へ変換し、
`sensor_hid.c` は基板座標をWindowsのnative portraitパネル座標へ変換する。

| 段階 | X | Y | Z |
| --- | --- | --- | --- |
| MMA8652 raw | rx | ry | rz |
| Linux direction 7 / `mma8652_read` | -ry | -rx | -rz |
| 0.0.1.7 HID (`0658432`) | -board Y = rx | board X = -ry | board Z = -rz |
| 0.0.1.8 HID (`b788381`、当時のbranchで採用) | board Y = -rx | -board X = ry | board Z = -rz |

0.0.1.7の補正は修正後に対して面内の両軸が逆で、180度異なる。
「90度違う→表示上端右→補正後上下逆」というユーザー履歴は、
既存ログでは0.0.1.7への上下逆申告として記録されている。
これを受けた符号修正は既に `b788381` に存在する。
当時のbranchはその修正を独立適用した。現1.10は上記の別契約を持つ。
Z、raw読出し、I2C、PnP、HID descriptorや電源policyは変更しない。

履歴上、0.0.1.8は適用成功と継続測定が確認されているが、向きの確認は未実施。
このため今回さらに面内符号を反転させる根拠はない。
0.0.1.8でも上下逆という新たな実機結果がある場合には、適用版と姿勢を区別して
判断する必要がある。ホスト試験の成功を実機の向きの確認とは扱わない。

## 一次ソースの比較

- Linux CT3 commit `7c4782127d4ea11965be00ce1343056be9bfb769`：
  [a04br3.dts](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/a04br3.dts)
  の `cust_accel@0` はdirection 7。
  [hwmsen_helper.c](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/misc/mediatek/hwmon/hwmsen/hwmsen_helper.c)
  のmap[7]はsign=(-1,-1,-1)、map=(1,0,2)。
  [mma8x5x.c](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/misc/mediatek/accelerometer/mma8x5x/mma8x5x.c)
  の読出しは `acc[map[i]]=sign[i]*data[i]`。
  Windowsの基板座標変換はこれと一致する。
- XNU正本commit `f5d43bf1677955e7e367545b9bbbd6449776cdda`：
  [IOHIDEvent.cpp](https://github.com/mouseos/xnu_kernel_benesse_a04br3/blob/f5d43bf1677955e7e367545b9bbbd6449776cdda/iokit/Kernel/arm/hid/IOHIDEvent.cpp)
  の `accelerometerEvent`→`_motionEvent`→`_axisEvent` は渡されたXYZをそのまま保持する。
  この共通イベント実装はMMA8652固有のパネル補正を行わず、Windowsの補正符号の
  独立した実機根拠にはならない。参照した正本treeにはMMA8652専用ソースは見つからない。
  パネル補正方向の根拠は上記ユーザー申告であり、XNUで動作確認済みとはしない。

## 実PnP経路と試験

`wdf_mma8652.c` の定期READ_REPORT完了とGET_INPUT_REPORTは両方とも
`mma8652_read`の基板sampleを同じ`a04_mma_hid_input`へ渡す。
補正は試験専用の関数ではなく、この実際のHID境界で適用される。

`make -C drivers/sensors/mma8652 test test-hid arm-coff` で確認する。
`mma8652_orientation_test.c` は模擬I2Cの7-byte raw frameから実読出し・実HID変換を実行し、
面内四方向、Z両方向、12-bit端点を確認する。当時の1.8 fixtureでは0.0.1.7のHID変換へ戻すと面内試験が失敗した。
現1.10は1.7と同じ変換なので、その過去の否定試験を現合否条件には使わない。
実機・runner・REAは使用しない。Windows WDK packageの再build・署名と実機確認は主担当が行う。

## 統合

この履歴の対象branchの符号・INF版は既存 `b788381` と同じ0.0.1.8だった。
既に同commitを統合したtreeでは符号修正を重複適用せず、追加コメント・試験・本資料を取り込む。
既存0.0.1.8と同じ座標出力のため、この追試用差分だけでDriverVerを上げる必要はない。

## 1.10の候補

fresh106の搭載SYSは署名済みupright1.8とbyte一致した。最新の上下逆申告に
対応する候補として、1.10では1.8/統合1.9のHID境界にあるX/Yだけを反転し、
(-board Y, board X, board Z)を返す。既存1.7と同じ変換だが、旧1.7の低い
DriverVerと別署名identityは現在のexact更新契約に合わないため、1.10を
ソースから新規生成し、既存BCBF signerでSYS/CATを署名する。
Z・基板軸・校正・descriptor・IOCTL・I2C・電源処理は変更しない。
これは現在姿勢での実機成功を示さない。今回fixtureは1.10の数式契約へ更新したが、
物理姿勢の合否判定には使わない。
