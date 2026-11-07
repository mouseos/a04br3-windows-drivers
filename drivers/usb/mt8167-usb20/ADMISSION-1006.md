# MediaTek MT8167 USB20 admission解析

run83 native checkpointをソース定義に従って評価した結果、確認できた停止条件は
「ID41 highでdevice-roleと判定し、host leaseを取得せず待機」である。一般的な
電源/PMIC失敗ではない。USB1006はD0実戻り値と内部activation待機を正しく
区別するleaf修正であり、host給電の根本解消・USB列挙成功とは扱わない。

- InitParent: status0/detail0=32/detail1=1はARM interface size/version。
- InitAdmissionWait: statusC000009E/detail0=1は旧leafによる待機の電源失敗表現。
- InitD0: 旧leafは上の内部statusを保存するが、実callbackはSUCCESSを返している。
- InitPower/Pio/BoardPrepare/Arm/Runは未記録。欠落だけでは一般に未到達を断定しないが、
  今回の既知理由1の制御経路は、それらの処理より前に終了する。

親のusb_owner_acquireはparent lock/online/leased/poisonを確認後、probe_sourceを
最初に呼ぶ。ParentUsbProbeSourceはread-onlyでGPIO0x220を読み、bit9(ID41) highを
理由1、bit2(GPIO34 detect) lowを理由2として返す。この時点でGPIO22/PLL/VUSB/PHY/
VBUSを変更しない。Linux一次ソースkernel-4.4/drivers/misc/mediatek/usb20/mt8167/
usb20_host.cのis_usb_detとmt_usb_is_host、およびa04br3.dtsのID41/VBUS22/detect34に
対応する。既存vendor/XNU由来board_host/gpio22コードもparent lease前にPHYを触らない。
ID highの実測経路は判明したが、ケーブル/外部回路/継承pin設定のどれがその入力を
作ったかはこのcheckpointだけでは確定しない。未知の寄与をレジスタ書込みで補わない。

変更は既知理由1/2のactivationをPENDINGとして返し、D0実戻り値のSUCCESSと分離する。
無lease待機はhost-readyではない。未知値/負値、Pio/PHY/IRQ/Runの実障害は失敗を維持。
本修正でdevice-role判定をbypassしない。新規物理操作・カメラ・実機操作は要求しない。

親1026のsource 2d6231727e8ed72b6d7dca4148debc101ac5aeefを保持。RTCborrow/capture/
GPU/SPM/SD/thermal等のflags・resource・親USB owner/interfaceは不変。実配置USB1005の
HID query/workitem/checkpoint修正を取り込んだleafだけを1006として生成する。親SYSを
旧branchの親へ巻き戻さない。既存USB1005 obj cacheからwdf_keyboardだけを再compileし、
変更のない13objをcopy/hash照合する。driver名は実ハードMediaTek MT8167 USB20。

検証は実KbD0 callbackをcompileして、receipt statusが戻り値と一致し、既知waitで
board-ownedが立たず、実障害が失敗のままになることを確認する。旧USB1005 callbackは
同じfixtureでreceipt不一致を再現する。既存board/parent owner fixtureで待機時に
MMIO副作用・lease・poisonがなく、source制約を拒否することを確認する。
実機host admission/給電/列挙/入力は未確認のままで、将来の別観測が必要。
