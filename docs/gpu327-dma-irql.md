# 327 native MDL scatter/gather DDI IRQL boundary

3.27正本41cc2a01（binarydf849aと対象source一致）のnative_owner_mdl.cはbind_pages/release_coldをPASSIVE_LEVEL限定で実行し、legacy DMA_OPERATIONS.GetScatterGatherList/PutScatterGatherListをIRQL遷移なしで呼んでいた。Microsoftの両DDI/IrqlDispatch ruleはDISPATCH_LEVEL必須。GetScatterGatherListExの<=DISPATCH契約と混同しない。callback自体もDISPATCH_LEVEL契約。

独立修正は2legacy call直前のKeRaiseIrql(DISPATCH_LEVEL)と直後のKeLowerIrqlだけ。bind/release APIはPASSIVEのまま、Get戻り後のcallback event待ち、PFN/SG検証、lease refcount、page cleanup/heap freeはPASSIVEで実行する。SGを架空にしたりPIN/MDL/cache方向/byte範囲を変えたりしない。既存RT/VB/textureの独立MDL lifetime、ISR/queue/flush契約は変更しない。

これは具体的DDI違反の修正であり、run131 Draw後resetの原因と断定しない。2Clear成功後にも同系経路を通る。旧flushされなかったログ不在は原因根拠にしない。単独の旧327同条件実機再試験を要求せず、Huygensの最新rootfix/sourceへ差分を評価統合する。

production helperを抽出したASan/UBSanテストは、legacy APIがDISPATCHで呼ばれること、成功/失敗とsynchronous callbackの後にPASSIVEへ戻ること、WriteToDevice/引数/戻り値保持を検証する。既存native327 archiveの1memberだけcompileし全member順/hash保存を要求、既存KMD archiveを再利用してSYS relink/同BCBF署名まで可能。UMD/client/SDK/PCO再buildなし。INF/CAT/実機profileを生成せず、署名SYSもintegration-onlyで実機投入禁止。main/Huygensが新採用版へ統合し最終package/sign/CATを固定する。

一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pput_scatter_gather_list
- https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/wdm-irqldispatch
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list_ex （Exと区別）
