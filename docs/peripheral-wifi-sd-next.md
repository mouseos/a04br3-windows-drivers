# 次の一括周辺更新へWiFi1020/SD8を追加する準備

このsourceは候補packetの検査・組立と、既存native offline readerの対象拡張だけ。driver、親、GPU、kernel、UEFI、更新serviceを変更/buildしない。旧6package profileへ新INFを混ぜてそのまま投入しない。

## 固定候補

- MediaTek MT6630 Wireless LAN SDIO Adapter: source9f1c03b0、INF2026.11.8.1020、既存署名packageを再利用。1013接続selector＋1014QoS統合。association/auth/IP/data/性能は未検証。
- MediaTek MT8167 MSDC1 microSD Host: source4bb60761、INF2026.11.8.8/checkpoint8、既存署名packageを再利用。unknown CDで旧世代を失効し、単なる回復で曖昧なwriteを再開しない。カード抜挿し/RW/実timeout停止は未検証。

run115/fresh116はWiFi1012とSD7のSYSを保持し、USB1009/MMA1.10/eMMC1.9/parent1101/RTC1.0/thermal1.7を適用した保存基準。**mainが進行中のGPU試験後に選ぶfreshの代わりにはしない。** SDはseven/ten native bindingの対象外なので、保存SYSだけから実選択INFやProblem0を推測しない。

親1101はSD power v1とWiFi runtimeを有効にしている。SD power interface/providerのsourceはSD7のものと同byteであり、新consumerのために親をbuild/旧版へ交換する必要はない。既存RTC/thermal/GPU/Audio/USB等のflagsを削らない。8rangeの対応POST bootを保持し、6range/親1026へ戻さない。新GPU試験でbootが進んだ場合も、そのcurrent matched bootを保持する。候補packetにboot/kernel/FDを同梱しない。

## 準備packet生成（ローカルだけ）

```sh
python3 tools/winpe/prepare-wifi-sd-peripheral-candidates.py --workspace <TAB-A04-BR3-root> --out <root内の新規output>
```

固定署名manifest/member/SYS/INF版/HWID/serviceを検査し、fresh116保存28fileとrun115適用receipt/8range基準を照合する。原本やSYSTEMを変更せずWiFi/SD packageを同byteコピーし、candidate-overlay.jsonとpins.jsonを作る。候補の整理が目的なのでdraft=true/normal_install_allowed=false。GPU試験中のfresh/media/serviceを生成・arm・applyしない。署名自体の再検証は既存回収結果を参照し、manifest/member hashを再照合する。

## 次の一括profileへ渡す具体的差分

1. GPU試験後のmain-selected fresh、GPU policy、current matched boot/親1101を同じ世代に固定する。旧fresh114/116のhiveやGPU SYS/UMD/clientを再投入しない。
2. 同じfreshのprivate trioで既存read-peripheral-preservation.ps1に **-IncludeWifiSd** を指定する。既定挙動は不変。追加対象はA04BR3\MT6630SDIOとACPI\MOU0013だけ。元trioをloadせずcloneへnative log replayし、選択INF/version/date/service/class/instanceを取得する。scriptは実機やカードをprobeしない。
3. 次の一括profile ownerはwifi/sdを新stage/DIF対象へ追加する。WiFi/SD SYSを旧untargeted保持表から外す差分はその2fileだけ。既存6roleは現在byteを保持し、他の採用差分が別に選定されたときだけ同じ一括へ含める。親/boot再交換は本候補から要求しない。
4. 旧6role固定policy/serviceはそのまま再利用できない。targets配列/loop数、stage/install order、保存対象、policy digest、各版/checkpoint/INF/SYS/CATの厳密pinを一緒に更新する。単に旧mapへ2roleを足して厳密検査を緩めない。
5. run115でdiagnostic1168の先行guardがCM取得を抑えた。次serviceでは各対象のlive CM statusをselected/sys guardより独立して記録し、それぞれのAPI rc/type/bytesを分ける。identity pinの失敗を握り潰さない。API別診断改善担当の最終sourceへ本packetを入力し、並行して同じ修正を再実装しない。
6. 全採用role/freshが決まってから更新serviceだけを1回compile/signする。WiFi/SDは既存署名byteを再利用。既存LF-only UART修正05de7966も未署名旧serviceへ含まれると誤認せず、最終serviceへ保持する。
7. planned restartは次の1回だけ。mainのみ実機操作。WiFiは標準scanと接続段階を区別し、既存OS profileがある場合だけ接続確認。SSID/鍵/profileXML/MAC等を公開ログへ出さず、ユーザーへ就寝中の準備依頼をしない。SDはカード無しならhost/binding/NO MEDIAまでとし、card R/W成功を推測しない。不在Probe/無根拠retryはしない。

このpacketだけではservice/媒体完成・起動/ネット通信/SD動作成功ではない。GPU117進行中にfreshを取り直すことや実機を所有することはない。
