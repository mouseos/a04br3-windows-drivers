# GE8300 Render capture / Patch prepare 347

run161/GPU346では同PID2628の固定GPR2でmagenta/green各480000画素一致、不一致0。R47はFW開始後のIRQ_ENABLEが1→mask後0で、R37 phase3まで通過した。初回FW同期停止と同じ状態ではない。その後R23はDRAW magic0x31444f47/generation5でC00000B5、R34はnative job PREPARED/dispatch PREPARED/serial1/generation4。ClearをprepareしたままSubmitされず、続くDrawがwait_boundary_lockedの2秒を使い切った状態に整合する。最後のReadTimeStampCounterのPC1点を初回fenceの停止へ結び付けない。

実装はRenderのClear/Draw/Copyすべてをcommand capture・ticket予約・allocation参照保持・patch list生成に統一し、residentな入力でもnative prepareしない。既存DxgkDdiPatchのcontext/ticket両copy/全allocation/residency検証後にのみprepare_pendingを呼ぶ。既存非resident Renderの経路をresidentにも用いる変更で、scheduler/IRQによる実Submit・完了通知・繰返しPatchの同token・世代・所有権・retirement条件は保持する。新しい待機、タイムアウト緩和、CPU memcpy fallback、架空fence完了は追加しない。

Microsoft一次資料：
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_render — Renderはcommand bufferからDMA bufferと出力patch listを生成する。allocation addressesは提出前に変化し得る。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_patch — Patchは実提出前の物理アドレス反映と最後の変更機会で、同DMAの複数回Patchもあり得る。

miniport/kmd_clearの2TU差分、346の他78objects・native owner・IRQ mask・UMD/FW/clientを保持。GPU347-patch-prepare-v1、INF0.0.3.47。source/build成功と実機Draw/継続Copy/安定動作は区別し、実機修正成功は未確認。同候補の反復試験はせず、次の通常batchでBT根因修正と一括適用する。
