# native triangle / texture 2job candidate

Windowsで検証済みの基準はnative clear4framesであり、このtriangle候補は未実機。
元vendor capture06の4frame成功と混同しない。WDDM/D3D capabilityは追加しない。

## 完成したCPU producerとruntime接続

`a04_m111_build_triangle_suite`は800x600 RGBA8、opaque black背景、
(-1,-1),(1,-1),(-1,1)のsolid triangle→同geometryの1x1 textureを生成する。
General/USC/PDSのGPUアドレスはcold ownerの実DMA/PT mappingから渡す。
旧Android VA/PMR handleはbuilderへ渡さず、全pointer/heap offset/task/descriptorを合成する。

- prefix PPP48、draw PPP104/108、vertex PDS32+12はcapture06全量と一致。
- prefix USC32はGLES InitClearCodeBlocksが静的DAT_00256e89からcopyする
  immutable programと新取得USC32が完全一致する。同版boardの固定programとして使用。
- main VSはRG32 input→UVSW xy01、40bytes、temp0/input2/output4。
- solid FSはshared0 packedRGBA→pixel0、16bytes、temp0/shared2。
- texture FSはshared0..3 sampler・4..7 image、normalized2D(.5,.5)→
  U8888 pack→pixel0、64bytes、temp5/shared8。
- main VS/FSはsource NIRをPCOで生成。patched host Mesa base
  0d94b3f397fb9cf8470bb5600ada8af79ff5361aの実source差分・全link input/object hashを
  build manifestへ保存する。2process生成結果一致とsource IRのSMP2D/pack/endを検査。
- fragment taskはsourceのUSC temp量子化で再encode。共通single-DOUTDで
  solid uniformをshared0、texture descriptorをshared0..7、texture時の未使用
  uniformをshared8..9へ転送。programのshared範囲はPPP予約shared16内。
- PDS varying size0なのでvarying programはsource仕様上実行しない。
- region clip/terminateは同版800x600 captureの実scalar49/37を維持。
  別generationのXMLにある32px単位へ換算して変更しない。

`A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE=1`の場合だけ、cold stageで全resourceを
readback照合し、既存publisherでsolidを実publishする。完全TA/3D/UFO/cleanup/
SLC flush/GP fenceと全480000pixel/guard検証後だけtextureへrearmする。
全code/resourceはFW start前にcopy済み。warm中のcode差替えはない。
失敗時は既存poison+DMA保持/CPU admission fallbackを使用する。
FW cleanup counterのhost incrementは追加していない。

## backingと公開API

追加DMA allocation/mapper/FWbank変更は不要。既存各4KiB page内の使用量：

| heap | 使用bytes | 内容 |
| --- | ---: | --- |
| General suffix | 0x4b0 | VDM2本、PPP、scissor/dbias、頂点、2点prefix入力、image/sampler/texel/uniform |
| USC | 0x140 | validated clear background/EOT/TA-PPP、prefix、main VS、solid/texture FS |
| PDS | 0x244 | validated clear programs、vertex fetch、fragment task、shared DMA |

残りpaddingはhost-owned packing policy。未取得region/MLIST/auxの観測を
zeroで捏造したものではない。HWRT/PM/aux cold policyは実機clear基準と同一。
出力は同じprivate480000pixelと既存sentinel/guard、実DMA/PT lifetimeを使用する。
HのVidMm/control-v2へ接続したという意味ではない。

## 主担当用WDK recipe

既存copy-service/lease/clock/2D flagsと固定FWをそのまま使い、以下のみ明示する。

```powershell
$env:A04_GE8300_VENDOR_NATIVE_BACKING='1'
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_JOB='1'
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE='1'
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_FRAMES='2'
$env:A04_GE8300_VENDOR_NATIVE_TRIANGLE_SUITE='1'
# 既存fixed-source BCBF署名/verify-ge8300-package/SFTP回収手順
& .\tools\ci\build-ge8300-provider.ps1
```

DriverVerは0.0.2.5。実搭載版より上であること、署名後SYS hash/INF/CATを
回収manifestと実配置で照合する。実WDK compile/link/signingは主担当で未実施。
単独clear/cold-onlyの追加実機試験は不要。host reviewとWDK後に2jobを一度で試す。

UARTは`NATIVE TRIANGLE_TEXTURE BEGIN` / `STATUS`、phase/match/mismatch/guard/
completed_framesを出す。成功条件はSTATUS0 PHASE4 MATCH480000 MISMATCH0 GUARD0
COMPLETED_FRAMES2。第一frameも全画素一致しなければ第二frameへ進まない。
期待画素は独立なpixel-centre半平面式。各frameは240000pixelが色付き、
残り240000pixelはopaque black。単なる全画面clearは不一致となる。

## 検証境界

host：capture全量比較、全active referenceのresource範囲照合、全heap/output移動、
invalid inputでout不変、source shader metadata/IR/再現性、2frame lifetime/fault/
poison/guardモデル、baseline1/16frameとMDL target回帰に合格。
5 producer TUはlocal ARM32 COFF compileに合格。これらはGPU実行ではない。
Windows triangle/texture、一般D3D/WDDM owner、実IRQ visibility/quiesceは未検証/未完成。


## 失敗時のbounded scan診断

GPU readbackは従来の全画素/guard scanだけ。追加GPU job・DMA read・mapperはない。
public DOD/copy interface、既存native resultの5word layoutは変更しない。
別のsource-linked private accessorで現在失敗frameのscan結果だけをUARTへ渡す。

失敗時のみ最大7行：SCAN(FRAME0=solid/1=texture、PIXELS、WORDS、
COLORED/BLACK/OTHER)、FIRST_MISMATCH(VALID、INDEX、EXPECTED、OBSERVED、
GUARD)、有効なSAMPLE最大5行。slot0..4は(0,0),(799,0),(0,599),(799,599),
(400,300)。indexはrow-major DWORD index、値はlittle-endian packedRGBA。
COLOREDは現在frameの正しいforeground値そのもの、BLACKはff000000、
OTHERはそれ以外。counts合計は読み終えたactive pixel数でありguardは含めない。
guardの最初の不一致もFIRST_MISMATCHへ記録し、GUARD=1で区別する。

read途中失敗ではPIXELS/WORDSとsample maskの実取得分だけを報告する。
未取得sampleをzeroとしない。まだ画素不一致を観測していなければVALID=0。
scan前の失敗、成功時、次jobへのrearm前失敗で前frameのsampleを再利用しない。
既存status/phase/guard/poisonと合わせて判定する。DriverVer2.5は未deployのため維持。
