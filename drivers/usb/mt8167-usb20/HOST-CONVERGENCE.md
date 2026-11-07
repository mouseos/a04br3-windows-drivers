# MediaTek MT8167 USB20 host開始・継続条件

USB1008を保持したUSB1009候補。実機未検証。今回の修正はMACの誤成功判定と
静かなhost喪失を処理するもので、現在のPC接続によるdevice-role待機を解除しない。

## 既存証拠で区別できること

run97のUSB1005は親/資源/HID query成功、InitAdmissionWait理由1でhost lease取得前に
待機していた。旧InitD0のC000009Eは内部activationの表現で、実D0失敗の確証ではない。
1006のstatus分離、1007のidle修正、1008のboot mouseを本候補でも保持する。

stock usb20_host.cのID high/device-roleとdetect34 active-lowは現在のadmissionと
一致する。PC接続中の待機は適切であり、極性反転・ID強制・GPIO22再実行を根拠なく
対策にしない。継承したpinmux/IESがその時点で適切かを確定する同一起動の完全な
snapshotは既存run97 checkpointにない。この不足を推測で埋めない。

過去のEFI Shell入力・Linux mouse列挙の履歴は、その条件で基板給電経路が利用できた
根拠になる。現在のケーブル状態・同一binary・コネクタ電圧の測定ではない。
MT6392のVUSBはstock regulatorで3.3Vの内部rail。GPIO22 DOUT、GPIO34 DIN、
PHYのforced VBUSVALID、MAC stateを5V測定として報告しない。UniXNUのcharger監視も
USB host/コネクタ5V成立の証明に使わない。BQ24160のOTG_LOCKをboostと解釈しない。

## sourceで確認した不足と修正

既存Linux移植の `a04br3-mt8167-usb-role.c` はSESSION/PHYの再ラッチ後、
BDEVICE解除を5ms間隔・1秒上限で確認してからcoreを再開する。Windowsの
A04UsbHostRunはPOWERのbit0/5がreadbackすれば成功し、BDEVICE継続やSESSION欠落、
RESET/SUSPENDM/RESUME/SOFTCONN残留を成功扱いできた。これは静的コードとfake-MMIOで
再現した実装不足であり、run97の停止原因と確定したものではない。

USB1009は同じadmission/親lease取得後に、DEVCTLのBDEVICE=0かつSESSION=1を
最大1秒待つ。解除は要求せず観測する。VBUS/PHY/mux/PMICの既存書込み値と順序を
変更しない。待機中の追加処理はDEVCTL読出しとdelayのみ。HOSTMODEは接続前の条件に
せず、未接続の正常hostを拒否しない。POWERのRO HSMODEは許可し、RESET等の残留は
失敗とする。失敗後は既存のIRQ disarm/join→SESSION/VBUS停止→lease解放を使う。

さらに稼働中のpollで、親のhealthが成功した後だけMACのSESSION/BDEVICEを読む。
静かにSESSIONを失った場合やBDEVICEへ戻った場合は既存PortFaultで停止する。
通常のポートRESETやHOSTMODE未成立はこのhealth条件に含めない。

待機理由1/2では追加MACアクセスを行わず、leaseなし/PHYなし/VBUSなしを保つ。
親のv1 interface/guard、GPIO/MT6392/PLL、親1026、資源、EDK2、GPUを変更しない。

## 戻り値とnative checkpoint

- Runの0はMAC状態の確認まで。給電・接続・列挙成功を意味しない。
- Runの-1は従来の引数/VBUS API失敗、-2はBDEVICE/SESSIONの期限超過、-3はPOWER残留状態。
- InitRunのdetail0はRun result。detail1はRun直後のDEVCTLとPOWERを下位/上位8bitに格納。
  これは逐次の後続readbackであり、期限超過時の最終sampleと同一・atomicとは保証しない。
- 新規stage18 InitHostHealthはC0000185、detail0に判定で実際に読んだDEVCTL、detail1にport stage。
  FFFFFFFFはcallbackの妥当性が失われ読出せなかった値。電気計測として扱わない。
- schema1/32byte/同device lifetime開始時刻/一stage一記録を保持。stage countは19。

## 検証と候補生成

`make test-mt8167-usb-host-convergence` は旧Runの誤成功を再現するfixtureを使い、
即時・5ms刻みの遅延・1000ms末端での成功、期限超過、SESSION欠落、POWER不正、
未接続HOSTMODEなしの許容、callerによるVBUS/lease cleanupを検証する。
`tests/usb/host_poll_test.py --scratch <作業ルート内dir>` は実KbPollをcompileし、
MAC喪失時fault、正常RESET許容、親health優先、device-role待機中0MMIOを検証する。
既存keyboard/mouse/owner/IRQ/control、WDF HIDとD0 receiptの回帰試験を保持する。

`tools/ci/build-mt8167-usb-host-convergence-incremental.ps1` は固定1008 compile cacheの
source/manifest/unsigned SYSを照合し、board_hostとwdf_keyboardだけを再compileする。
15obj中13objをhash照合してコピー再利用する（gpio22とhost_core_contractは非リンク）。
親/SDK/GPUを再構築せず、既存署名identityでSYS/CATとmembershipを検証する。

rootの既存Linux/stockソースへの参照は作業記録のsource-evidence.jsonにhash固定し、
生ログをこのrepoへ入れない。1007/1008の署名正本は変更しない。mainのGPU試験媒体へ
混載せず、実機・UART・書込み・再起動・物理操作依頼は本作業で行わない。
