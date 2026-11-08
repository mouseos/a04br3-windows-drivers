# MT6630 FromDS再送の受信owner

基点は接続/QoS統合1020 source9f1c03b0。association・OS supplicant・CID7・実PTK event・controlled port・CCMP PN・TX_DONE/creditは既実装で、再実装しない。実RF接続/暗号通信/性能は未検証。

1020のnative_frame_parseはFromDSのRetry bitを無条件拒否していた。このため最初の送信を受信し損ね、その後の正当な再送だけがHIFへ届いた場合も捨てる。一方、stock gen3 nic/nic_rx.c:659 nicRxIsDuplicateFrameはRetryだけでは捨てず、同一Sequence Controlの既受信時だけ重複と判定する。非HT ERPのQoSはaucTid2ACI（nic/que_mgt.c:56）でAC別、non-QoSは独立cache。BA/HT/VHT/reorder/A-MSDUは本候補でも非対応。

FromDS decodeは既存24/26/30byte header/FromDS/fragment0/実HIF descriptor/rate/相手APを検査したまま、Retryをcaller-owned duplicate境界へ渡す。Runtimeは既存security/PN/port後に単一AP・association receive_epochのsequence ownerを使う。cacheは実RX slotへcopyできた後だけcommit。packet filter・queue満杯・鍵/PN拒否からcacheを成功更新しない。epoch変更は古いcacheを失効させ、Seq4095→0は単純な大小比較で拒否しない。同じSeqでもRetry0はstock同様に新しい観測。group RetryはACK前提不成立で拒否する。

NDIS runtime context内の5cache（BE/BK/VI/VO/non-QoS）だけで、独立parent/MMIO/IRQ/再送workerを追加しない。外部peerを新規作成せず、OS profile/鍵を取得・出力しない。実TXは従来どおりRetry指定を拒否し、standalone indication helperもstateful ownerがないのでRetryを拒否する。受信decoder単体成功をNDISへの配送成功とは呼ばない。

production HIF decoder→同retry ownerのfirst-retry/重複、queue未commit、epoch・AC/nonQoS分離、wrap/fragment/group拒否、既存QoS24/26/30/TID/CCMP bounds/入力不変をASan/UBSanで確認する。driver source version/checkpoint1021で識別。runtime contextとinline include依存に変更があるTUを増分compileし、無関係なSDK/parent/音声1033/GPU sourceは変更しない。実ネット接続・DHCP/IP・実双方向通信はmainの既存profile条件で後段確認が必要。
