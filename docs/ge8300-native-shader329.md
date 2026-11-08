# ABI4: 実生成ALU shaderのnative受理

327/328から独立。327/325/326の署名packageと実機媒体は変更しない。

旧nativeはpassthrough/constant/color/SAMPLEの固定token+USC列と固定metadataしか受理しなかった。新ABI4は、実source→NIR→PCOが生成したUSCを変更せず受理する。別shaderへの差替え、無条件の任意USC受理、固定USCへのfallbackはしない。

## 検証契約

producerはMesaの`pco_ir.c`と同順のpassを実行し、post-RA/pco_end後・group化前のphysical IRをpointer-freeな整数recordへexportする。その後は元のpco_group_instrs/pco_encode_irを使用する。PCO/NIR本体やSDKは再buildしない。

nativeはrecordのopcode/operand/modifier/register/終了を検査し、同じ一次source `pco_map.h` / `pco_isa.h` のfield encoderで全byteを再計算してUSCと照合する。先頭/末尾byteやhashだけの認証ではない。データ値・命令の並びが異なる正当なプログラムも受理する。生成されたUSCそのものをowned USC pageへコピーする。

metadataはpost-RAの実physical refsとrepeat幅からtemps/vtxinsを導出する。runtime CBのmask/範囲に基づいてshared領域を設定し、nativeも独立に各accessが宣言内にあることを確認する。PDS DOUTUのtemp allocationは4DWORD単位へ丸め、PS uniform DMAは実shared数（未使用時も安全な4DWORD、最大16）を使う。

opcode/modifier metadataは専用symbol名へ分離し、NDEBUG有無でsizeofが変わる既存PCO libraryのpco_op_infoへbindしない。kernelとUMDのwitness encoderは同一のsource tableを使用する。

kernel encoderは固定stack scopeだけを使う。Mesa mapのIR list/arena bookkeepingをlocal scratchに置換し、instruction/field encodingを変更しない。許可したmappingはscratch instructionを最大2個作る。ralloc/free/再帰/浮動小数演算/GPUアクセス/UMD callbackは呼ばない。

任意USCがSM4 tokenと意味的に等価であることをkernelが再compileして証明する契約ではない。UMDがshader意味を保持する責任を持ち、nativeはmemoryless executableとregister/resource境界を独立に検査する。USC改変は元witnessと不一致なら拒否する。攻撃者が新しいwitnessも生成しても、許可された安全な算術と出力以外は実行できない。

## 実装範囲と拒否

- straight-line FADD/FMUL/FMAD/MBYP/PCK/MOVI32/MOVWM/MOVS1、VS UVSW WRITE/EMIT_ENDTASK。
- scalar/vector float32、VS POSITION4 input/output、PS BGRA8 output0。
- VSのCB無し（即値は可）、PS CB0の先頭4vectorまで。temps最大128、USC各stage最大192byte、post-RA record最大128。
- TEMPのread-before-write、全register範囲、source bank/slot制約、ROM numeric const範囲、許可modifier、最後のendtask/END、出力存在を検査。
- memory/texture/atomic/branch/predication/spill/indexed register、任意SPECIAL/GLOBAL/SLOT、half precision、余剰IR/未知ISAは拒否。
- Texture327 ABI3は元の実resource/descriptor契約で保持。ABI4へ架空texture資格やvarying/GS/depth/blend対応を追加しない。一般shader全機能の完成とは扱わない。

USCはVS0x80..0x13f、PS0x140..0x1ff、既存RT-load0x200以降へ分離する。検証済み192byte上限が各範囲の重なりを防ぐ。512byte constant captureからPS最大64byteだけをnative dataへコピーし、EOT/RT descriptor領域と重ねない。元のtarget/VB2件を参照し、shader IRからGPU VAやresource IDを受け取らない。

## private ABI / lifetime

adapter private contract5でshader_draw_version4を厳密交渉する。旧peerはsize/version不一致で拒否。packed-clear owner/control/MDL resolve1とtexture ABI3/profile4は変更しない。ABI4 headerは2本のwitness spanとISA識別を持ち、KMDは元と同じMmCopyMemoryによるimmutable captureを保管する。

Render reserve→paging bind→Patch native prepare→Submit→IRQ/fence→retireの所有権と成功条件は変更しない。shader/constant/IRはRenderCb前にコピーされ、UMD storageを解放してもpending DMAの参照先にならない。PASSIVE_LEVEL、同一target/VB MDL、停止・quarantine・generation・失敗時job=0等の元条件を保持する。第二IRQ/HWownerや早期成功を追加しない。

## 一次source

- `third_party/mesa-pco/mesa/src/imagination/pco/pco_ir.c`（pass順）
- `.../pco_group_instrs.c`（length/DA/最終2groupの8byte alignment）
- `third_party/mesa-pco/generated/src/imagination/pco/pco_map.h`, `pco_isa.h`, `pco_info.c`（命令mapping/encoding/metadata）
- `.../pco_const_imms.c`（numeric ROM）
- Mesa `pvr_arch_cmd_buffer.c`（VDM vtxins、PDS data/USC temp/shared配分）と `rogue/ppp.xml`。

実機試験はmain/Mencius所有。hostの実compile/native構築と実GPU動作成功を区別する。
