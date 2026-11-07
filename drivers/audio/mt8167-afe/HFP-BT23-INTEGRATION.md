# HFP common / Nash23ee 結合checkpoint

BT依存を旧63c32からNash `23eeea70805f15c12fa3f9ed8346d2ae5c057649`へ更新。
sideband.c/hfp_endpoint.c/ks_endpoints.cpp/hfp_topology.cpp/hfp_pnp.cppの5TUと
hfp_volume.hを含む全依存headersを同じcommitからarchiveする。旧core objectは混ぜない。
provider v2 structはbf98963と同一。controller/独立DMA drainが未資格という条件は維持。

Audio owner/common3TUに対して、`build-hfp-owner-modules.ps1 -PairBluetoothCore`で
現BT5TUを同じflagsでcompileして8objectsへ結合できる。`-IncludeBaseAudioContracts`を
明示するとdma_adapter/wavert_registrationの2TUもcompileし合計10objects。
SYS/INF/CAT/署名/親再buildや版予約は行わない。source/依存/flags/object数をmanifest化。

既存Engine1006の実StartDevice改善を独立HFP sourceにも接続した。AFE合成PDOだけに
BUS_INTERFACE_STANDARDを実親stackへforwardし、contiguous DMAをVersion3/address32/
ScatterGather FALSEで取得する。v1がARM一律非対応とは主張しない。
Engine1007のPortCls Name寿命修正もHFP sourceへ反映した。2subdeviceの名前をstatic保存し、
登録関数return後のstack文字列をPortClsが参照しない。run47のSYS/媒体は変更しない。

旧full HFP helperのINFは親1007/audio1005のcheckpointであり、現親1008/1010より古い。
新たな版予約なしに同版/旧版の別親SYSを作らないよう、baseline以上の新版でなければ
SYS build前に拒否する。これらのINF版を勝手に1009/1012/1014へ変更しない。

## 未接続であり成功広告しない契約

- 実radioとHFP targetの物理一致。未確認GET_DEVICEOBJECT IOCTL/名前一致で代用しない。
- vendor FC72/GORM bytesとCVSD8k/mono/S16の実controller設定一致。
- 独立bus-drain証拠。CUR停止/disable readback/IRQ rundownをその代用にしない。
- codec delay/FIFO latency。未確認値を0成功にしない。

親controller登録、VerifyDmaQuiescentが未資格の間、HFP factory公開/SCO capabilityは抑止する。
REAは他担当専有のため本作業では使用しない。必要な一次調査はNash handoffに従いmainへ
中継する。current BT profileはWLANと排他であり、run47のEngine/WLANへ混ぜない。
