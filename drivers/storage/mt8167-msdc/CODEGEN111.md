# MSDC 1.11 DMA polling core code generation

1.10 commit47f6e233のruntime sourceを保持し、純C dma.cだけ-O2 -fno-vectorize -fno-slp-vectorizeでbuildする独立候補。miniport、MMIO wrapper、SRB mapping、queue ownerのcompile設定は従来O0のまま。LTOなし、target/ABI変更なし。既署名1.10/HCD1017/147media/親USB/GPU非変更。

現1.10のbuild-msdc-request-perf110.ps1ではdma.cに-O指定がなくclang既定O0だった。間接callbackを繰り返すpoll coreでloop/helperのCPU overheadが残る。Linux CT3 source7c478212のkernel-4.4/Makefile:611-615ではkernelをOsまたはO2でbuildするが、Windowsでの正しさをLinuxだけから推定しない。XNU NativeMsdc f2fc17f6のbatch/IRQ ownerとWindows同期DMAは別契約であり、IRQ/SG/multiblock移植は今回の範囲外。

-O2化対象は引数/uint32/uint64演算と副作用callbackを持つ純CのDMA coreだけ。vectorizer2種を無効化してSIMD最適化を要求しない。clock->now、observe、io->read32/write32は外部callbackで、全callbackの値・順序・数・終了結果を比較する。poll期限、wrap差分、各段階start、1024回のQPC監視、4096回のobserve、iteration上限、write CMD13/DAT0、STOP失敗結果を保持。MMC clock/voltage/tuning/flush/write retry不変更。DMA停止証明が改善したとは主張しない。

ASan/UBSanで同じ実dma.cをO0/O2別objectへcompileし、918ケースの全MMIO/clock/observe列・内部fake状態・結果を比較。CMD17/24/13、FIFO/command/data/STOP/DAT0/STATUS各遅延、CRC/timeout/card error、初期busy/IRQ/DMA、clock有無/短期限/uint64 wrap、invalid address/alignment/clockを含む。12件のSTOP失敗を明示assert、invalid6件はhardware callback0をassertする。hardware/Windows kernelの実行ではなくhost modelである。

runnerの既存WDK/signing closureでminiport1TU＋DMA O2 1TU、比較DMA O0/miniport flagsOFFをcompile。portable5objectは既存hash/source/include照合で再利用。source/callback契約は同一、ARMNT/実RT8.1 exports/signature/CAT照合を成果側に固定する。実速度/CPU使用率/boot-I/O安定はmainの後続試験待ち、147に途中投入しない。

再現: python3 tests/msdc_dma_codegen_test.py --out ROOT内成果先。署名buildはtools/ci/build-msdc-dma-codegen111.ps1。前回と同じreuse/source-c747別履歴bundle・既署名1.9 baseline・WDK小closureが必要。SDK/WDK全setupなし。共通親/USB/GPU/EDK非変更。
