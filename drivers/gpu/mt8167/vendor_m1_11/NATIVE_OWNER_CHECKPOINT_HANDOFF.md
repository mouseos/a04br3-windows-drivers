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

## H5a26a0d aperture bindへの実接続追加

provider側に以下を実装した（実機/runner未操作、GEO tableは未公開）。

- `a04_vendor_native_target_aperture(n,&window)`：実ownerの確保済み
  PD/PTから、GPU base0x8200000000、window4MiB、現在のone-surface commit limit
  469pages、最高DMA0xffffffffを返す。CPU BAR/identity mappingの推測ではない。
- `a04_vendor_native_attach_mdl_target_at_cold(n,mdl_binding,aperture_byte_offset,packed_argb)`：
  Hのbindが受け取るaperture_byte_offsetそのものへ469pageを配置する。
  offsetはpage aligned、末端は4MiB以内。他のPTEは全てinvalidにする。
  実OS SG/PFN一致、MDL extentとNormal lease契約は従来どおり必須。
- target変更後に元private outputを解放し、PBE/EOT/clear code/record/CCB shadowを
  実target VAとpackedDWORDにより再生成・Device readback確認する。
  A8R8G8B8のメモリはB,G,R,A bytesなのでpacked ARGBをそのままpixel DWORDとして扱う。
  alpha0を含めてCPU projectionを確認。GPUで任意色が成功したという主張ではない。
- 現profileは800x600/pitch3200/full-MDL/first-coldのみ。
  offset500pagesのPT leaf跨ぎ、offset555pagesのwindow末端、
  非整列/UINT64 overflow/範囲外で無変更、既存exposed保持/cold detachをhost確認。
- 実WDK helperは`-WithMdlTarget -LinkCheck`で新entryもexportしてlink対象へ残す。
  helperの実WDK実行は主担当管理、ここでは未実施。

H側で次に必要な接点（実装担当を重複させず主担当から中継）：

1. VidMm allocationの**実Normal-RAM originとMDL/pages参照**をretain/dropする
   frontend口をnative factoryへ渡す。現a04_native_mdl_leaseはこの実参照を必要とする。
   MDL_PAGES_LOCKEDだけやcache flagだけでoriginを捏造しない。
   推奨はgeo_native_bindingのsource-linked factory入力にfrontend contextと
   retain/dropを追加、または同じ意味のfrontend accessorを提供すること。
   public DOD/copy interface変更は不要。GEO bindのbytesはactive1920000、
   actual MDL countはrounded1921024であることを保持する。
2. `GeoNativeOwnerCreate`がfullcontrolを返す前に、actual Dxgk IRQだけを経由する
   completion/visibilityと、platform/fabric停止を結合する。
   `A04BR3RogueStop.h`自体がexternal fabric drainの証明ではないと明記している。
   power_off_ack/IRQ mask/Stop成功だけからGEO_PROOF_DMA_STOPPEDを返さない。
   その証拠がない間、exposed DMA/MDLは保持し、capsやquiesce成功を返さない。

このowner追加はGPU2.5最終候補860f697とは別のhost作業。
主担当の2.5 buildは860f697固定のまま。owner commitを無断で混ぜて再buildしない。
