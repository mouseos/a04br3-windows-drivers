# Bluetooth2018の公開source同期

本変更は搭載driverの新しい機能候補ではなく、公開Wi-Fi1033系sourceに欠けていた
既公開2018修正のexact同期である。基点8bdebbb8のcombo_client.cはREAD完了を
非再帰client WDFWAITLOCK内で呼ぶ旧実装で、INFも2017だった。
combo_client.cとINFをaa4233fd8afdc9596128ece9b71f472efc9b6b58へ完全一致で戻す。
新たなHCI command、電源処理、SCO資格、MAC/校正値を加えない。

Windows WdfRequestCompleteWithInformationは要求を完了し、上位completionを経由する。
既2018の保存済みBthMini調査は同スタックでREADを再発行する経路を示す。
旧コードのTimer→deliver→completion→READ→同じlock取得は再入できない。
2018修正はlock内でpacketをコピー・取り外した後、lock外で完了し、再取得後に
packet列を再走査する。失敗bufferもlock外で完了し、1 pass32完了上限を維持する。
既fixtureをexact同期して1回実行する。新しいfixtureやtransport模擬は追加しない。

一次Windows契約:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestcompletewithinformation
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthxddi/ni-bthxddi-ioctl_bthx_read_hci

BTHX READのType/DataLen/Informationは従来のpacked contextのまま。
Linux stock stp_chrdev_bt.cのBT_openはfunction-on→STP ready、read/writeはBT_TASK。
既XNU由来WMT/STP実装と合わせ、共有NDIS brokerの所有・command列は変更しない。
新MMIO、未知register、独立SDIO owner、MAC/NVRAM固定を導入しない。

## 失敗と後続証拠の区別

実run161/post162の最初のBrokerTargetOpen=C0000034は名前空間open失敗であり、
HCI OPEN/FW command失敗の証拠ではない。namespace/admissionに限る既2016の
30秒/300attempt待機と、実HCI OPENの失敗を再試行しない条件は現在sourceにもある。
既2015以降の失敗NTSTATUS伝播・OPEN receipt・実transport capabilityを保持する。

actual231の保存disk SYSはf20336f9...で既署名2018packageと一致、選択INFも2018。
そのC/INFの正本はaa4233fd。これを現在RAM loaded imageや現在OPEN状態の証明にはしない。
既run171-diag1には同BT SYSで標準Bluetooth radio列挙成功の独立記録がある。
providerは当時1026、actual231は1031であり、異なる起動の成功を接続・RF・電源9F
解決の証明へ広げない。今回のsource同期を搭載版への新修正として再投入する根拠はない。

Optional HFPのValidatePcm/CodecLatencyやDMA drainの未確定はそのまま残す。
未知値の常時成功化やguard緩和はしない。source同期以外に新たな確定contract不備は
この限定確認では成立していない。Windows compile、署名、実機操作は行わない。
