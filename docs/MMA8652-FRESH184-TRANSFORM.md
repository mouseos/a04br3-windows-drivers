# MMA8652 fresh184：実配置1.10と180度補正の同定

ユーザー最終申告「センサー今度は上下逆」は未解決として扱う。補正済みsourceの配置を物理方向の正しさと同一視しない。一方、現在版には既に180度補正があるため、同じ申告を根拠に反転を重ねない。

fresh184 manifest ce675a15cbef38ba91696d9f7f863e364de0fc44ee9f4a15824714c59ecd9775と対応native receipt private-windows-rt-combined-run183-registry-post/receipt.jsonのmanifest/hive hashesを照合。actual ACPI\MOU0010\0のselected mma8652.inf/0.0.1.10/11-6-2026、Service=mshidkmdf、LowerFilters=A04Mma8652を確認。配置SYS998329ec0541a7738c893c3185b591439e433ec55489aee657670a5b89f52388/21440Bは既BCBF署名1.10 packageとbyte一致。

署名源泉9ad5ef0afff9712399f86e429787464e6bf710cdのmanifest全11fileとgit blob、actual runtime C/H/INFを照合。INFのみ既知CRLFを適用。fixture/doc整合source8b614ddfをaudit branch基点とするが、driver runtimeを新親や媒体へ取り込む操作はしない。I2C1/PIO/IRQ/IOCTL/報告安定化/感度/校正/descriptor/型番/製品識別文字列を変更しない。

Linux CT3 commit7c478212のDTS direction7、hwmsen sign/map、mma8x5x式からraw→board=(-rawY,-rawX,-rawZ)。現1.10はboard→HID=(-boardY,+boardX,boardZ)、従ってraw→HID=(+rawX,-rawY,-rawZ)。旧1.8/統合1.9は(-rawX,+rawY,-rawZ)で、現在へさらに面内180度を加えると旧出力へ戻る。READ_REPORT/GET_INPUT_REPORTは同じa04_mma_hid_inputを通る。

9ad5ef0aのcomment/ORIENTATION末尾はcaptured1.8への上下逆申告を受けて1.10を作ったと明記。fresh184はその1.10を保持している。SYSTEM/captureには物理姿勢や期待上辺がなく、1.10適用後の新たな姿勢申告の時系列は確定できない。XNU f5d43bf1の_axisEventは入力XYZを保持する共通処理であり、Windowsパネル上辺の根拠ではない。portrait寸法や旧GPU/DOD制約で符号を決めない。

既actual C経路（7-byte I2C raw→board→HID）をASan/UBSanで一回実行し、四方向±X/±Y、Z両方向、12-bit端点/混合ベクトル計8ケースが1.10の数学契約に一致。物理姿勢の成功ではない。tools/sensors/audit-mma8652-fresh184.pyがroot/out内の新規directoryだけに結果を出し、manifest/receipt/SYS/source/Linux/XNUとベクトルをまとめて検査する。成功結果を固定し同内容の追加再実行はしない。

未修正sourceではないため新runtime修正・WDK再compile・同一package再署名は行わず、既1.10を保持。物理問題は未解決。次に符号変更を正当化するには、mainの必要一括batchで得るactual1.10/後継版と対応した姿勢・期待上辺・現出力の組が必要。追加の物理操作/個別試験は要求しない。実機/185/13role/GPU360/clock/FW/共通EDK/既SDK/cacheは非変更。
