# 実割込み処理と準備済みDMA送信

run165のGPU348は、最初のClearを受理してPASSIVE準備を終えた後、実送信の帰還がC0000184となった。続いてWindowsのメモリ管理停止0x10E/B/C00000A3を観測した。今回の正確な拒否条件は採取しておらず、startup IRQがその原因だったとは断定しない。

ソースには、FW起動から送信までの間に実ISRが割込みをclaimしてmaskすると、同じwork itemの実行中にはqueued serviceが進まず、native submitが拒否する競合がある。349はこの経路を処理する。

- ownerは有効な同epoch・実claim serial・masked source・READY/COMPLETEに限り、未送信の継続結果を返す。cold mask、fault、drain、未知状態は対象外。
- consumerはstate8、同じprepared token、fence、世代、ページ参照を保持する。PASSIVEで実receiptをserviceし、同じIRQ lockで実送信を再開する。native prepareは繰り返さない。
- 実sourceを処理できない場合は100msまたは100回を上限に失敗させ、pinを保持する。完了通知、割込み、画素結果を作らない。
- preemptのackは従来どおり実DMA完了後。異なるtokenでの再開を拒否する。
- R50は実IRQ継続の観測、R51はserviceの帰還。どちらもGPU完了や描画成功ではない。

`tests/submit_queue.c`はproduction engineへIRQ継続を注入し、未送信中の参照保持、後続ジョブの追越し禁止、preempt境界と実完了の区別を確認する。ホスト試験はWDK/IRQL/実GPUの証明ではない。

既存の署名済み348を基点に `tools/ci/build-ge8300-submit-irq349.py` で4TUだけ再compileする。miniport、KMD clear、engine、native owner factoryが対象で、残る76objects、UMD、FW、client、SDK/compilerを再構築しない。全ソースからの通常ビルドの出自は既存fullsource gateで確認する。実機での成功は未確認。

投入前の再レビューで、inline service後に古いservice_pending bit1が残る経路を修正した。bit1はservice/rearm前に原子的に消費し、その後の新しい実IRQが立てたbitを消さない。r1は未実機のまま投入を撤回し、r2でKMD clearの1TUだけ差分再構築する。他79objects/UMD/FW/clientは保持、INF3.49は維持し、正式SYS hashとsource commitでr1と区別する。
