# GPU353 IRQ service deadline

171-diag1/PID2868の失敗はCopy/GOP1 slot1/gen3/fence10であり、Drawではない。R52..69は直前成功Draw slot0/gen7/fence9/cookie0x6f3b67e8。CopyはDraw snapshotを生成しない。shader制約を変更しない。

R50→R51は109.3771ms、service0後に352は100ms期限でTIMEOUTへ上書きし、実submitを再評価しない。registry flushが全経過時間を占めたとの証明はない。353は同token/state8の実同期submitを先に再評価し、再びWAIT_IRQの場合だけ次serviceを100ms/100回で制限する。実submitの成功のみを受理し、IRQ完了の合成やtoken/pin解放はしない。service失敗は既negative経路。

PREPARED serviceのR39同期registry書込みを省き、R50/51をretry終了後に永続化する。R51.dはservice実経過msへ更新。FILETIMEは永続化時刻であり実call時刻ではない。既R52..69は同boot/ticket/cookieのときだけ比較する。

本番retry body抽出のhost回帰は旧110ms timeout、修正版同token再submit、再IRQのelapsed/count上限、busy、service error、同期失敗を検証する。実機成功は未検証、power9Fは未解決。SDK/UMD/FW/client/engine layout/IRQ・shader制約は変更しない。
