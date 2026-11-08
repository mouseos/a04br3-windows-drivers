# 341: run145 VIDEO_SCHEDULER_INTERNAL_ERROR 119/3 の準備境界修正

340はfreeze。338/run143の2Clear標準GPU CopyResource各480000一致成功は別成果として維持。
338後段resetと340firstCopy Patch失敗を同じ原因とは扱わない。

実run145 post146: R22=0xC00000BB、R23=0xC00000BB/kind0x31504f47/slot1/gen1。
R28 Flags0、DMA範囲0..16、R29 contextあり/private範囲0..16。
実選択340、pixelPID2536、初Copy RenderCb S_OK後Map/Lock待ち、UART119/P1=3。
P2..4はscheduler内部pointerで独自解釈しない。officialDXGKDDI_PATCHはPASSIVE、
anyerrorreturnで119/3。新画素成功は未証明。

340のsource欠陥:
初Clear完了後、stagingの初MAPがcold_for_bind→stop/retire→build→attach_linearを行う。
その時点のgraphはcoldで、copy_storageはboot前にしか作られない。後のPatchで
prepare_pending→prepare_copyがcopy_transfer未作成/onlinefalseからNOT_SUPPORTEDを返す。
run145の実R23/R22と一致する具体的な経路。全エラー原因の一般化はしない。

修正:
- RTとstagingが両residentになったnative bind時にcold TQ資産を準備。起動/任意PTE更新なし。
- KMD MAPの正規PFN lease/geo_bind/GeoConfirmVidMmBind完了後、関連するpending jobの
  全allocationがboundならprepare_pendingを1回行う。別順MAPでも最後の依存MAPが担当。
- Copy native準備は本当にcold（hardwareownedfalse/startattemptedfalse）のgraphのみ初起動。
  onlinegraphは340の長寿命再arm継承。Copyごとのstop/rebuildは復活させない。
- 全residencyが既存なら従来どおりRenderで準備。
- Patch/repatchは同context/slot/generation/openhandles/kind/refsの準備済ticketを検証のみ。
  Patch内のnative allocation/bootstrap/prepare_pendingを除去。未知ticket/flags等は拒否のまま。
  catch-allSUCCESS、偽completion、参照解放、Stop証明の緩和はない。
- 準備失敗はMAP/Renderの実失敗として伝播して参照保持。未知エラーを成功にしない。

実関数テスト: Patch36case（両MAP順、8回ずつrepatch、未準備/別context/別handle/別generation/
poison/owner準備失敗の拒否）、copy64transaction+578negativegates+初coldboot1回。
ASan/UBSan通過、WDK/owner/IRQ/FW/cacheはmock。ARM/sign/実機結果は別receipt。

根拠: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_patch
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_patch
生HTML/textと実R22/R23は成果物rootへ保持。再Patchは公式に複数回呼出し可能。
source単一branch fix/gpu341-resident-copy。実機main、最新fresh146以後のみ、340無変更再実行なし。
