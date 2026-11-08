# 単一render/display miniport 実装途中

独立branch work/ge8300-unified-present。338 current source3aaaef10を基点とし、338package/143媒体は変更しない。現時点でdesktop GPU化・DXGI output・Present実行成功を主張しない。新codeはまだminiport tableへ登録せず、caps/source数も変更しない。

## 実装済みsource

- wddm-display/unified_display.[h,cxx]：既BASIC_DISPLAY_DRIVERをopaque childとして同一adapterに保持し、POST ownership取得と実framebuffer検査の後にだけsource/child数を返す。VidPn/child/monitor/POST release/SystemDisplay各DDIをC bridgeで既実装へdispatchする。独立DriverEntry/IRQ/GPU ownerは作らない。旧KMDF provider、direct/shared diagnostic GPU経路の同時buildを拒否。
- GeoPresent：WDK8.1のsource1/destination2をGOP1のsource1/destination0へローカルallocation listで変換し、既Copy2ref予約→residency→prepare→実submit/IRQ/fenceへ接続するtransportを実装。OSのallocation listを改変せず、Patchにも元index2/1を残し、WriteOperationと両handleを照合する。両cursorは実ticketを書いた成功時に更新する。SubmitのPresent bitはcopy kindだけに限定。Flip/rotation/subrect/色変換/NULL DMAは拒否。
- 最初のtransport範囲は既実装800x600 BGRA8 RT→staging全extent/offscreen Bltだけ。これを画面primary対応として広告しない。Native shader/USCの再buildなし。

## 全統合前の具体的不足

1. actual POST primaryをOS標準SHAREDPRIMARYSURFACE allocationとして扱う契約。現standard allocatorはshadow/stagingのみ。GOP framebufferの非cachedmappingとVidMm backingの所有権を一致させ、物理連続scanout/GPU aperture backingを同じものとして検証する必要がある。単なるPrimaryAddressの書換えや任意MDLのGOP扱いは不可。
2. POST幅/高さ/pitchへnative storageと既800x600 viewport/target/copy制約を拡張し、既GOP表示を保持。現copy_storageは800x600のみ、primaryを受け付けない。出力を1と数えるだけでは不足。
3. SetVidPnSourceAddress/CommitVidPnのprimary寿命、dirty/clipped/rotated Present、実display/vblank割込・scanline契約とTDR中GOP保持。BDDはCPU rotation可能でもnative Copyがそれを実装したことにはならない。MMIO flip/vsyncを推測で広告しない。
4. UMD typed_dxgiのPresent/SetDisplayMode・primary create/identity rotationとruntime callback。現在は明示unsupported。実supported範囲が完成するまで変更しない。
5. unified miniport Add/Start/Stop/power ordering、GPU0と既DOD(MOU0007)の同時POST owner排他、childbridgeの最終table登録。橋のC++object単独は配備driverではない。

## 一次根拠

- Microsoft Learn DxgkDdiPresent: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_present
- DXGKARG_PRESENT: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_present
- SetVidPnSourceAddress: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_setvidpnsourceaddress
- WDK8.1実header d3dkmddi.h DXGK_PRESENT_SOURCE_INDEX=1 / DESTINATION_INDEX=2、DXGK_PRESENTFLAGS、DXGKARG_PRESENT。Win7+ union pAllocationInfoは予約とされ、古いpAllocationList説明との適用差は現RT8.1実callbackで確認が必要。このため未検証Presentをtableへ登録しない。
- 同repo Microsoft由来BDDのStartDeviceはDxgkCbAcquirePostDisplayOwnershipを使う。DxgkInitializeDisplayOnlyDriverを第二登録しない。

## 検証範囲

既Windows toolchainでunified_display.cxxをMSVC ARM1TU compile成功（object CD7A8B712C2363A2887D841D32D2E840208D5DDDBCE1F062F4E8540E58719FC5）。SYS link/署名/実機は未実施。shader/PCO/SDK再build無し。新Present transportは別ARM1TU compile結果をHANDOFFへ記録。compileをdesktop動作成功に読み替えない。
