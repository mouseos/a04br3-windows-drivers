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
- KMDはHuygens e24a5001を正本として合流。ticket/両openhandle/context/kind/count/residencyを
  nativeprepareより前に検証する。state1の再Patchは同owner tokenを保持し無副作用。
- nativecopyはcoldで正規資産準備→初boot→firstarmを実行。liveは前実完了後の同graph再arm。
- Mencius途中2b8e3474のBuildPagingBufferへ準備を移す案は不採用として履歴保持。
  pagingcommandのfatal失敗へ移すだけの修正にはしない。
- 全residencyが既存なら従来どおりRenderで準備。
- Patch/repatchはHuygens事前検証とprepare_pending(state1idempotence)を採用。
  未知ticket/flags等は拒否、catch-allSUCCESS・偽completion・参照解放・Stop証明緩和なし。
- 未知の準備失敗は実失敗として伝播して参照保持。未知エラーを成功にしない。

実関数テストはHuygens Patchテストとproduction caller sequenceを正式receiptへ記録。
途中不採用36caseは履歴のみ。copy64transaction+578negativegates+初coldboot1回も実行済み。
ASan/UBSan通過、WDK/owner/IRQ/FW/cacheはmock。ARM/sign/実機結果は別receipt。

根拠: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_patch
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_patch
生HTML/textと実R22/R23は成果物rootへ保持。再Patchは公式に複数回呼出し可能。
source単一branch fix/gpu341-resident-copy。実機main、最新fresh146以後のみ、340無変更再実行なし。
