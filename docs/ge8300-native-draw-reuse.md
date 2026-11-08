# 後段native Draw/texture再利用（341採用sourceとは別branch）

Mencius native所有: native_owner_factory.c / windows_native_job.c /
windows_native_execute.c / native_owner_dispatch.{c,h} / windows_native_cold.h。
Huygensはdesktop frontend/UMD/miniport Present table/unified_display17担当。
Huygens donor/共有frontendは本branchで編集しない。mainへHANDOFFで所有権を固定する。

既sourceはDrawごとstop→retire→cold build→shader/state書込→boot。
既rearm_clearには診断sentinel全RT書込みと16frame制限があり、そのままDrawへ転用不可。
新経路はその同じtypedCCB/UFO/cleanup証明を利用するが、production DrawではRTにsentinelを
書かず実画素/LOAD/blend内容を保持する。HWRT/freelist/cleanupcounterをリセットしない。
既native_pair.cの同versionKM全group予約と実完了後反復offset/PRfence更新を根拠とする。

- 既same-root/same-allocationに、要求v1..v5を既CPUproducerで再生成する。
  GPUに見えているUSC/PDS/stateimageと全TA3Dバイト一致を要求。前frameのPRfence値4Bのみ正規更新。
  不一致は無変更NOT_SUPPORTEDとして既cold経路へ。任意live code/data/PTE rewriteなし。
- 直前の本render completion+freshIRQとCPUownership、online同FW、CCB空/実UFO/cleanupcounter一致を検証。
  正規resident VB/textureの新CPU内容は全MDLをGPUownershipへ渡し、同immutable shader/stateで再利用。
- 既candidateのattempted/verified/publishedとvisibilitytokenは新jobのため正規更新。
  dispatch COMPLETE→READYは実Dxgk IRQlockで行い、freshjob/generationと既submit/ISR/workerを通す。
- 途中errorはpoison/refsretain。Stop/TDR proofなしに資源を解放しない。偽success/fakeIRQなし。

必要な実rearm/typedCCB producerの3frame sequenceをASan/UBSanで確認。
GPU/IRQ/cache/OSはmock、RTsentinel書込み0を確認。silicon/texture/desktopPresent成功ではない。

通常Presentは現render-onlyminiportが明示拒否。Huygens単一adapterDOD17bridgeの
primarylayoutはまだ標準VidMm/GOP/scanout leaseに繋がっていない。
現nativeは800x600/4MB aperture、panel800x1280×2面は1024PTEを超える。
caps/寸法を緩めるだけで成功にしない。本変更はPresent table/driver capsを変更しない。
