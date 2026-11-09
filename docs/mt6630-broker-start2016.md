# MT6630 client2016 / provider1025：PnP開始順序と実HCI準備

実run161/post162はactualclient2015・選択System class0012・ReachedMask0x27ff、AddDevice→D0Entry各status0、BrokerTargetOpen=C0000034（OBJECT_NAME_NOT_FOUND）、D0Return同値。HciOpen/Capabilities/Receiptは未到達。これは実C0000034であり、HCI/FW初期化の失敗とはまだ言えない。

CDO名はprovider/clientとも\Device\MT6630Comboで一致する。署名providerのNDISRegisterDeviceEx importと同literalも存在し、単純な綴り違いやCDO未実装とは断定しない。NdisRegisterDeviceExはDriverEntryで作成し、実adapterはhardware_start後に別途publishする。client2015はD0Entryで一度だけ開いて失敗を返すため、まだprovider driver/CDOが開始されていないPnP順序から回復できない。

Microsoft一次資料「Specifying Driver Load Order」はPnPでDependencies/LoadOrderGroupを開始依存として利用できず、実依存には親子関係が必要と説明する。したがってINFのDependencies=Mt6630Runtimeだけを根拠にCDO存在を仮定できない。NDIS一次資料はDriverEntry登録後にMiniportInitializeEx、CDOはNdisRegisterDeviceExで作成する契約。WdfFdoAddStaticChildは親FDO作成後に実子を追加する正規APIである。

## 実装

2016はroot software busの開始とHCI child/radio準備を分離する。初回の未報告childに限り、実CDOの未公開/name/path/not-readyを100ms timer、30秒interrupt-timeか300attemptのいずれか早い上限で待機する。PnP開始threadをKeDelayやsleepで占有しない。rootD0成功はsoftware列挙busの稼働でありHCI/radio成功ではない。待機中はStaticChild/HciReady未到達、BthMini child公開0、HCI READ/WRITE/caps成功0。期限切れ/アクセス拒否/unknownABI/実HCI失敗は明示的fail、NoRestartでPnPへ返し、盲目restartしない。既報告childのresumeにこのdeferを適用しない。

provider1025はprivate kernel IOCTL0x94a/32Bのsoftware admissionを既単一NDIS workerへ追加する。実generation/power/initialized/failed/uncertainだけを観測し、SDIO/MMIO/WMT/電源操作0・新lease0。CDOだけ先にできadapter未publishなら既dispatchのNOT_READY、publish後でも実ready0なら待つ。query成功/ready0はsoftware観測でありHCI成功ではない。旧IOCTL/32B以外のABI/NDIS runtime構造は変えない。

CDO open handleを待機中は保持し、software admissionだけを再照会する。実ready後に既HCI OPENを一回だけ実行し、実function-on/STP/全HCIcommand/identity readbackが成功した後に初めてWdfFdoAddStaticChildする。HCI OPENの失敗は再実行しない。旧providerのunknownIOCTLをreadyと扱うfallbackも無い。既optional HFP/controller、real bypass predicate、parent単一owner、command bytes、FW、NVRAM、WiFi通信路、SCO hold/drain契約は保持。

Stop/Surpriseは待機を失効し、timer/queuesをjoin。namespace/admission段階ではHCI CLOSEを合成せずtargetだけを閉じる。実HCI OPEN済みだけ既CLOSEを実行する。既sideband hold中の停止拒否と既quarantineを維持する。

固定stage17 ProviderWait=STATUS_PENDING、18 BrokerAdmission=実query status、19 HciReady=実OPEN/子公開後、20 ProviderWaitResult=終端status。stage8 StaticChildは実公開時だけ。D0Return0をHCI成功に読まない。新helperに同stage名を追加する。既64B OPEN receiptは本当にOPENした時だけ生成し、software待機の代用に使わない。

## 検証と出自

source-only primary docsはroot成果物primary/に保存。host ASan/UBSanはproduction provider_wait.hのnamespace/admission限定、アクセス拒否/unknownABI/HCI失敗を再試行しない、既子resume非defer、逆時計/時刻期限/attempt上限を確認する。これはPnP/実HCI成功の証拠ではない。ARM/WDF compile/signと実機成功は別。

新host42555796562のC:\mt6630-start2015-cache-ae2e0708\closureに保存した最終47objectsを基点にclient combo_client1TU、provider combo_broker1TUだけ変更、残client3/provider42をsource/header/objecthash照合して再利用する。共通SDK/WDK再setup0。親1117/161/163/346/347/実機を変更しない。次batchはmain判断、既ROOT/SYSTEM/0001更新のみで新登録0。

今回の実C0000034がprovider開始順だけで発生したこと、根因解消/Code10解消/BthMini/radio/RF/SCO成功は実機未証明。両literal一致、初回targetOpen失敗、PnP Dependencies非保証という確定範囲を根拠にした候補である。

一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/specifying-driver-load-order
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nf-ndis-ndisregisterdeviceex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/network/initializing-a-miniport-driver
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdffdo/nf-wdffdo-wdffdoaddstaticchild
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdevicesetfailed
