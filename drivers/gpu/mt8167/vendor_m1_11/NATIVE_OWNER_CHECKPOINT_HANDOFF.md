# Herschelへのowner checkpoint連絡

主担当からHerschelへ中継するための共有事項。GPU runtime/producer担当は維持する。
H側sourceは5a26a0dのnative_binding.h/owner_control.hを確認した。

- checkpoint82d8ba1は同じVidMm locked MDL/PFNへcoldでPTを切替える実装。
  `a04_vendor_native_attach_mdl_target_cold`、
  `a04_vendor_native_target_before_fw_start`、
  `a04_native_mdl_hold_cold/drop_cold/extent/adapter`が既存の実coreへ追加済み。
- `A04_GE8300_NATIVE_OWNER_MDL_TARGET`既定0。実800x600 active1,920,000bytes、
  rounded469pages=1,921,024bytes、offset0のみ。source/host試験済みだが、
  flag1の実WDKは未実施。既存acf523eの5TU module+kernel API LinkCheckは成功済み。
- cold PT照合→元SG/PFNへの置換→Normal CPU sentinel/cache同期を行い、
  成功後private pixel common buffersを解放する。exposed後は保持し、
  poison/不明停止時に解放しない。別private outputへの描画をbind成功としない。
- Create/Delete/control-v2全tableは未接続。新Hのaperture configを含むfactory ABI、
  service callback、実DxgkだけがIRQ所有する構成を維持する。
- `GEO_OWNER_REQUIRED`、DMA_STOPPED/CPU_VISIBLE/IRQ_DRAINED等のproofは未広告。
  aperture reserve/map、任意surface/colour、実IRQ後visibility、quiesce/reset/rundown
  を満たすまでGeoNativeOwnerCreate成功やrender capsを捏造しない。
- triangle candidateは同じ3枚の4KiB owned code/data page内へ収まる。
  mapper追加・owner ABI変更は要求しない。独立したprivate2job実証候補であり、
  HのD3D UMD/standard DDI完成を意味しない。

現在はユーザー指示に従いtriangle/texture candidateを優先し、owner変更を保全している。
H側はnative factoryの未実装symbolを成功stubで埋めず、source/link依存として残すこと。
