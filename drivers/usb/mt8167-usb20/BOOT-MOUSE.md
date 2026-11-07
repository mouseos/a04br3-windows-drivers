# MediaTek MT8167 USB20 boot HID

USB1007を基点とするUSB1008候補。実機未検証。USB1007の正本を置き換えず、
実機USB1005を保持する。物理操作・カメラ・UARTによる試験は本作業で行わない。

## 対応範囲

| 対象 | 対応 |
| --- | --- |
| 直結boot keyboard | 従来の8byte入力・5 LED出力を保持 |
| 直結boot mouse | オプトイン、3ボタン・signed 8bit相対X/Y |
| 物理mouseレポート | 3〜8byte、先頭3byteだけ使用、拡張は破棄 |
| Windows入力 | ID1 keyboard、ID2 mouse、各9byte、mouse余白5byteはconstant 0 |
| Windows mapper | HIDCLASSの別TLCから標準kbdhid/mouhidへ渡す設計 |
| 速度・endpoint | LS/FS/HS、IN interrupt、MPSはmouse3〜64/keyboard8〜64、LSは8以下、HS intervalは1〜16 |
| 接続形態 | 直結1台・alt0の1interfaceのみ。両boot interfaceがあればkeyboard優先 |
| 非対応 | ハブ、複数interfaceの同時入力、report protocolの任意HID、ホイール、追加ボタン、remote wake/選択的suspendの新規実装 |

Windows側のdescriptorは実在するMT8167 USB20 transportの入力能力を示す。
接続機器のVID/PID/serialを作り替えたり、未知reportをmouseとして解釈しない。
物理USBのboot reportにはReport IDを追加しない。IDはWindows側だけで使用する。
オプトインOFFでは従来のkeyboard-only descriptor/8byte経路を使う。

## 列挙とレポート

既存のポートreset・デバイス/構成descriptor・SET_CONFIGURATIONの後、実際の
interface番号へSET_PROTOCOL(0)を発行し、完了後だけRX1を設定する。mouseの
class/subclass/protocol・alt・IN/interrupt・MPS/interval条件を検査する。
3byte未満・8byte超・endpointのMPS超のmouse入力は拒否し、FIFO範囲外を読まない。
上位5button bitと任意拡張をWindowsへ渡さない。X/Yの符号byteはそのまま保持し、
Windows descriptorの論理範囲は-128〜127とする。

keyboardのinterrupt入力はID1と元の8byte、LEDはWindows ID1付き2byteを検査し、
物理USBへは従来のReport ID0/1byteで送る。GET_INPUT_REPORTのkeyboard応答も
物理8byteからWindows9byteへ変換する。mouseへのkeyboard LED送信は拒否する。
mouseの状態照会は実受信後のbutton状態だけを返し、X/Yを0にして移動を再生しない。
初回report前や非接続・fault状態ではnot-ready。毎回の入力はinterrupt経路で受ける。

## 寿命と電源

USB1007の無期限idle/NAK待機と、列挙・制御転送の進捗期限を保持する。
切断は古いendpoint原因より優先し、FIFO/requestを停止・flushして再接続を受ける。
D0Exit/releaseではtimerとIRQ workをjoin後に親のboard leaseを解放する既存順序を保持。
切断・fault・電源停止では滞留入力を捨て、keyboardキー解放とmouse button解放の
2レポートを送る。mouse解放のX/Yは0。queue overflowもfaultとして扱い、停止後に
buttonを残さない。独自の電源ownerやremote wake広告は追加しない。

PC接続中のdevice-role待機は正常なadmissionであり、hostへ強制変更しない。
GPIO22/MT6392 VUSB/PLL/PHYの既存guard・順序、親1026、資源、EDK2、GPUは変更しない。

## 根拠

- [USB-IF HID1.11](https://www.usb.org/sites/default/files/hid1_11.pdf): Appendix B/B.2のboot report、7.2.6のprotocol、7.2.1のcontrol report。標準部分だけを実装し拡張を推定しない。
- [Microsoft keyboard/mouse HID mapper](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/keyboard-and-mouse-hid-client-drivers) と [TLC](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/top-level-collections): keyboardとmouseを別のApplication Collection/Report IDで記述する。
- [IOCTL_HID_READ_REPORT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidport/ni-hidport-ioctl_hid_read_report) と [HID_XFER_PACKET](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidclass/ns-hidclass-_hid_xfer_packet): report buffer/ID/実byte数の契約。
- 機種stockのkernel-4.4 `drivers/hid/usbhid/usbmouse.c`: IN interruptを受けてbuttonとsigned X/Yを入力系へ渡し、close/disconnectでURBを止める。Linux側が読むwheel/追加buttonは本候補では広告しない。
- 既存UEFI `Common/Mu/HidPkg/UsbMouseHidDxe/UsbMouseHid.c`: boot subclass/protocol照合とSET_PROTOCOLの参照のみ。EDK2共通コードは変更しない。
- UniXNU-a04br3のA04BR3Charger系はcharger監視であり、USB host/コネクタ5Vの成立やmouse対応の証拠として転用しない。

Linuxコードの転載ではなく、仕様と既存APIに基づく独立実装。既存ファイルの
クレジット・ライセンスを維持し、新規コードはBSD-2-Clause-Patentとする。

## ビルド・検証

`A04_MUSB_BOOT_MOUSE_EXPERIMENT=1` を既存 `tools/ci/build-mt8167-usb20.ps1`
が扱い、manifestへboot_mouseを記録する。host-core/HID-controlフラグは独立して従来どおり。
`tools/ci/build-mt8167-usb-boot-mouse-incremental.ps1` は固定1007 cacheをhashで検査し、
KB_ENUM/KB_SESSION layoutの依存分を含め6objを再compileし、9objをhash照合して再利用する。
親SYS・SDK・GPUは再構築せず、SYS署名→CAT生成→CAT署名→SYS/INF membership検証を行う。
既存1006用の増分scriptは1006専用のまま変更しない。

- `make test-mt8167-usb-boot-mouse`: mock MUSB/EP0によるmouse列挙、実SET_PROTOCOL/endpoint設定、LS/FS/HS、3〜8byte、NAK/idle/HS4096ms/時計wrap、再接続、不正descriptor/report、STALL。独立HID item walkerでTLC/ID/bit offset/relative flag/範囲を検証。
- 既存USB portable 8targetをmouse OFF/ON双方で実行し、keyboard/LED/親owner/IRQを回帰検証。
- `python3 tests/usb/boot_hid_wdf_test.py --scratch <作業ルート内dir>`: 実WDF callbackをbounded host shimでcompileし、read/controlのID振分け、移動を繰り返さないsnapshot、切断・D0停止・overflowの両入力解放を検証。
- `python3 tests/usb/d0_receipt_test.py --scratch <作業ルート内dir>`: admissionとD0実statusの記録を保持。

portableとshimはASAN/UBSANで実行する。Windows WDK ABIのARM32 compile/link・署名は
別検証であり、Windows HIDCLASS/mouhid/kbdhidの実行、VBUS/PHY/IRQ、実入力、長時間安定は
主担当による将来の実機試験まで未確認とする。親を含む既存batchのwdmguid.h不足は
別の未解決であり、本候補のために親stubを変更しない。
