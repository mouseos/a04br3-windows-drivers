# 既知PMRのreadonly alias捕捉

通常の捕捉動作は変更しない。prepare-recovery-native-capture.py の
`--suite --readonly-alias` が生成する `--run-suite-alias` だけで有効にする。
4frame workloadはcold clear / warm clear / solid triangle / 1x1 texture triangle。

根拠sourceは公開kernel-4.14の
`drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693/` 配下：

- include/pvrsrv_memallocflags.h: CPU_READABLE=bit4、GPU_WRITEABLE=bit1。
  KERNEL_CPU_MAPPABLE=bit14はkernel VA用であり追加しない。
- generated/mm_bridge/common_mm_bridge.h: NewRamBackedPMRのpacked56byte入力、
  flags32はoffset36。60byte validation variantは今回はalias対象外。
- services/server/env/linux/pvr_bridge_k.c: PVRSRV_MMapはlive connectionの
  vm_pgoffをPMR handleとしてlookupし、PMRMMapPMRが参照を保持する。
- services/server/common/pmr.c: PMRMMapPMRからflavor mmap/generic mmapへ。
- services/server/env/linux/pmr_os.c: OSMMapPMRGenericはPMR offset0から
  physical backingをmapし、CPU cache属性をPMRから選択する。

observerは成功した自身のNewRamBackedPMR journalと前後一致するchunk indices、
fd epoch、live GPU mappingを必須とする。4KiB単一dense chunk、CPU_READABLE、
GPU_WRITEABLEなし、既存CPU VMAなしのPMRのみ対象。importや未知topologyは対象外。
元allocation、MapPages、kickの入力を変更しない。捕捉済みVA/handleを再送しない。

kick前にlive_fdとlive_handle*4096でPROT_READ|MAP_SHAREDを作り、
既存のaligned volatile32 snapshot処理で読む。GPU VAをCPU pointerへ変換しない。
直後に自作aliasだけをmunmapし、成功失敗をkind7/tag12に記録する。
munmap失敗はbrokenにする。unknown/失敗/未取得は完全性の成功にならない。
SIGBUS等のprocess異常終了時はOSがVMA/PMR参照を解放し、試行は失敗とする。

上限：8試行/kick、32試行/process、同時alias最大32KiB。
既存snapshot上限4MiB/kick・16MiB/process・256KiB/spanは増やさない。
harness30秒、REA scenario45秒、staging/回収/解放を含む予定上限5分。
超過時は対象serialのこの試行processだけを終了・確認し、GPU retryしない。
Windows/ESP/boot/実partitionを書かず、/tmp private namespaceのみ。
実機操作は主担当の排他handoff後に限る。

この方法が新たに得るのは元UMの依存bytesでありWindows triangle成功ではない。
PMR aliasが成功してもPPP/PDSからUSCまで参照閉包を別途検証する。
