# MMA8652の座標変換と上下逆の修正

## 原因と修正

Linuxの取り付け補正とWindowsのパネル補正は別の変換である。
`mma8652.c` はraw値をLinux direction 7の基板座標へ変換し、
`sensor_hid.c` は基板座標をWindowsのnative portraitパネル座標へ変換する。

| 段階 | X | Y | Z |
| --- | --- | --- | --- |
| MMA8652 raw | rx | ry | rz |
| Linux direction 7 / `mma8652_read` | -ry | -rx | -rz |
| 0.0.1.7 HID (`0658432`) | -board Y = rx | board X = -ry | board Z = -rz |
| 0.0.1.8 HID (`b788381`、本branchでも維持) | board Y = -rx | -board X = ry | board Z = -rz |

0.0.1.7の補正は修正後に対して面内の両軸が逆で、180度異なる。
「90度違う→表示上端右→補正後上下逆」というユーザー履歴は、
既存ログでは0.0.1.7への上下逆申告として記録されている。
これを受けた符号修正は既に `b788381` に存在する。
本branchはその修正を独立して適用し、raw→基板→HID全経路の回帰試験を追加する。
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
面内四方向、Z両方向、12-bit端点を確認する。0.0.1.7のHID変換へ戻すと面内試験が失敗する。
実機・runner・REAは使用しない。Windows WDK packageの再build・署名と実機確認は主担当が行う。

## 統合

本branchの符号・INF版は既存 `b788381` と同じ0.0.1.8である。
既に同commitを統合したtreeでは符号修正を重複適用せず、追加コメント・試験・本資料を取り込む。
既存0.0.1.8と同じ座標出力のため、この追試用差分だけでDriverVerを上げる必要はない。
