# RT8.1標準USB接続の実装境界

USB1011はUSB1010の直接boot keyboard/mouse経路に、root hub要求処理と
WDK8.1 URBアダプタを追加した開発版。標準USBホストとしては未接続。
INFのClassを変更してHCDとして認識させる実装にはしていない。

## 根拠とAPI判断

対象実バイナリはWindows RT8.1 ARMNT。usbport.sys SHA256
`c668b894e7733206f987d85296f5e2d5fb4cf5b06c12ee43ea5afef85d4c31fe`。
USBPORT_RegisterUSBPortDriverとUSBPORT_GetHciMnのexportがある。
REA Evidence `ev_342ddbe98e9f165334c78c784e7c3e8e377acf2eb3c361acabc78145ec43a221`
で登録関数がversionに応じて異なるサイズをコピーし、callback群を登録元へ
書き戻すことを静的に観測した。version 0x136では0x224バイトをコピーする。
これはcallbackの引数、実行IRQL、完了・取消し契約を確定したことを意味しない。

実usbehci.sysの登録処理の既存REA Evidence
`ev_78a67be54a48cb57faa230c88bc39023a94ee051620d10c677b54021602a5290`
はversion 0x136で多数の関数ポインタを登録している。EHCIのレジスタ処理を
MUSBへ流用できる根拠ではない。UCX01000.SYSは通常exportを持たず、
WDFクラスライブラリのbind経路を使う。exportがないことだけを理由に
UCXが利用不能とも判断しない。WDK81キャッシュに公開UCX/HCD登録ヘッダは
見つからず、型を捏造して登録するコードは置かない。

公開WDK8.1 `shared/usb.h` のURB_CONTROL_TRANSFER[_EX]、
CONTROL_DESCRIPTOR_REQUEST、CONTROL_VENDOR_OR_CLASS_REQUEST、
CONTROL_GET_STATUS_REQUESTを実型のまま使う。
Linuxの根拠は既存公開ソース
https://github.com/s1204IT/CT3_GPL_LGPL_Src
commit `7c4782127d4ea11965be00ce1343056be9bfb769` の
`kernel-4.4/drivers/misc/mediatek/usb20/musb_virthub.c` と
`musb_host.c`、`musb_regs.h`。単一port、状態/変化ビット、class request、
EP0のPIOイベントを参照した。Linuxコードのコピーはしていない。
Linuxの「overcurrent報告なし」や電源要求のNOPを安全条件へ転用していない。

## 今回接続した処理

`root_hub.c` はhub descriptor、hub/port status、変化ビットのclear、
interrupt bitmapを処理する。POWER/RESETはgeneration付き非同期actionとして
返し、board ownerの完了証明があるまで成功しない。POWER ONは給電資格、
ID host、過電流なし、VBUS確認、endpoint停止のすべてを要求する。
POWER OFFも物理off確認とendpoint停止を要求する。RESETは50ms以上の
assertと有効speedを確認した後だけportをenableする。
切断・過電流・停止・removeではownerがactionをinvalidateする。
電源good時間はownerが提供する値をdescriptorへ使い、ボード値を仮定しない。

`wdk_urb.c` はURBからsetup/bufferを検証して既存`a04_control_submit`へ接続し、
既存queueによるpending/active取消しと切断をUSBD/NTSTATUSへ変換する。
MDLはownerのmapping関数へ必要バイト数を渡す。ownerがサイズと寿命を検証する。
実行中の取消しはendpoint停止・IRQ drain後のqueue完了まで通知しない。
短いIN転送はSHORT_TRANSFER_OKを適用する。EXのTimeoutはownerへ渡すが、
この層では時計やタイマーを持たず、ownerが期限管理する。
root hubも同じURB decoderから要求処理と非同期完了へ接続する。
不明な機能、isochronous、bulk/interrupt、select configuration等は拒否する。

## 未達とowner側の次工程

標準HCD登録、root hub PDO、USB_BUS_INTERFACE_USBDI/HUBの公開、
device handle/address管理、configure/pipe管理、汎用bulk/interrupt、
実ISRとの接続、deadline timer、PnP/power teardownは未実装。
USBPORT callback群を実バイナリのcallee/callerから引数・IRQL・寿命まで確定するか、
WDK81互換UCX class extension契約を取得してから標準HCDへ接続する必要がある。
新アダプタはSYSへリンクされるが、既存WDF HID経路からは呼ばれない。
標準USBスタック対応済み、物理5V確認済み、実機動作済みとは扱わない。
実機はmain専有のため、このworkerから配備しない。

## 再現

`make test-mt8167-usb-root-hub test-mt8167-usb-host-control`。
`tools/ci/build-mt8167-usb-urb-incremental.ps1` にUSB1009 leaf cache、
WDK81 KitRoot、新しいin-tree OutputDirectoryを指定する。
キャッシュmanifestと未変更TUのgit blobを照合し、変更6TU+新規2TUだけをARMコンパイル。
実WDK型を使うx64試験をWindowsホストで実行後、ARMリンク、SYS/CAT署名、
CAT membership/署名者を検証する。parentとSDKの再構築は行わない。
manifestのstandard_usb_host_stackとhardware_verifiedはfalseを維持する。
