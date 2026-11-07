# E/H共通のWDDM MDL resolver契約

Hはfrontend_memory.h/.c、mdl_pin.c、miniportのfactory呼出し、BuildPagingBuffer接続を所有する。EはCreate/Delete/native allocation/control-v2/停止/IRQ rundownを所有する。source版の唯一の境界をこのheaderとし、片方の未実装をもう片方へ戻さない。

## factory input

旧4引数libraryとの誤結合を避け、**GeoNativeOwnerCreateV3**へsymbolを更新した。

```c
GeoNativeOwnerCreateV3(PDO, dxgk, device_info, const struct geo_frontend_memory *, out_binding);
```

出力geo_native_bindingはversion2、control-v2/ops-v2（aperture offset付きbind）は維持。入力memory tableはsize厳密照合/version1。miniport AddDeviceがtable/contextを初期化し、同じFDOのadapter寿命をStop/Deleteまで保持する。factoryはtableをcopyし、H contextをNativeOwnerDelete成功まで借用する。別PDO検索/QueryInterface/共有IRQは追加しない。

## Eのbindで必要な具体的な呼出順

1. Hのops.bindへ入った元VidMm MDLとactive bytesを `memory.resolve(memory.context,source_mdl,active_bytes,&lease)` へ渡す。
2. size/version1/origin=GEO_MDL_VIDMM_CACHED_SHADOWを検査。lease.mdlは**独立に実lockした同じPFNのShadow MDL**。lease.page_bytesはrounded full extent。元VidMm MDL metadataを長期保持する代わりに、このmdlをa04_native_mdl_bindへ渡す。
3. native a04_native_mdl_leaseへlease.context/acquire/releaseを設定する。a04_native_mdl_bindがacquireを呼び、自分の参照を取得する。
4. resolve自体も**所有参照を1つ返す**ため、native bind成功/失敗のどちらでもresolveの参照をlease.release(ctx,mdl)で1回dropする。resolve後の検査失敗でもdropする。これによりresolve→acquireの間のUAFを避ける。
5. cold binding/実GPU公開後はE側のbindingが参照を保持。未公開なら正式cold detach、公開済みなら実停止/IRQ rundown等の証拠が揃うまで放さない。H callbackをGPU stop/quiescenceの代わりにしない。

Hはowner.bindが成功を返しても、実native参照が残っていなければbindを失敗にしてpoison/pinする。H unmapはmap参照だけをdropする。native参照が残る間、物理page lockとadapterは残る。二重release/誤MDL/誤IRQLはmemory faultとして保持し、解放成功へ丸めない。未解放record/faultがあるStop/Delete/Removeは成功にしない。

## Normal originと物理寿命の根拠

H CreateAllocationはExistingSysMem/ExistingKernelSysMem/primaryを使わず、実VidMm cached/CpuVisible system backingを要求する。登録するのは実BuildPagingBuffer MAP_APERTUREの、live driver allocationに対応するphysical system-memory MDLだけ。ユーザーMDL、DmaAdapter common-buffer、任意PFN列を受ける入口はない。Cached bitだけを未知のMDLへ付けて認可するAPIではない。

Mapの中でsourceのsystem VAから新しいIoAllocateMdlを作り、**Microsoft ARM compilerでcompileするSEH内**でMmProbeAndLockPagesを呼ぶ。元MDLをrelockせず、新しいMDLで独立lockし、全PFNを元VidMm MDLと比較する。失敗NTSTATUSは実例外値を伝搬する。元MDLがnonpaged-memory由来でも元MDL自体へのProbe/Unlockは行わない。部分/IO-space MDLは未対応として拒否。

nativeへ渡すShadow MDLはHが所有し、元VidMm metadataや元system VAが後で失われても物理ページを保持する。最後のH/native参照だけが対応するShadowのMmUnlockPages/IoFreeMdlを行い、元VidMm MDLやOS allocationをunlock/freeしない。元MDLのcounterだけをpin証明にしない。GPU/SLC/GP/cache可視性とquiescenceは別のE責務。

根拠: Microsoft [DXGKARG_BUILDPAGINGBUFFER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_buildpagingbuffer) のMapApertureSegment.pMdlはphysical system-memory pagesを記述する。[DXGK_ALLOCATIONINFOFLAGS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_allocationinfoflags) はCached backing storeの契約を定義する。[MmProbeAndLockPages](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmprobeandlockpages) は異なるMDLによる同じphysical pageの複数lockとtry/except必須、最後のunlockまでの保持を明記する。[MmGetSystemAddressForMdlSafe](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmgetsystemaddressformdlsafe) の元mapping所有権をHが勝手に解放しない。

## build

Clang ARMのSEH非対応をunsafe dereferenceへ置換しない。build-wdk81.ps1へ、hash固定した正式Microsoft ARM cl.exeを`-MicrosoftArmCompiler`/`-MicrosoftArmCompilerSha256`、対応headerを`-MicrosoftCrtInclude`で渡す。mdl_pin.cだけMicrosoft compiler、他TUは従来Clang。生成pin objectのARMNTを検査する。既存42c moduleは実WDK成功/回収hash一致、新MDL sourceの実WDK/実mappingは未確認で区別する。
