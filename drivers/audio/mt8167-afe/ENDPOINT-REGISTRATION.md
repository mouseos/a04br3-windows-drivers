# Engine endpoint登録の寿命修正候補

run42のAudio1006はNativeStart schema1/56B/Stage14/result0、DMA adapterあり、
Version3/ScatterGather FALSE/address32、CM Started/Problem0。StartDevice改善を
確認したが、AudioEndpointBuilder/Audiosrv STATE4でもMMDevice endpoint0、
HRESULT 80070490、client exit1で、音声出力は未確認。

## 修正

PcRegisterSubdeviceのNameはデバイス寿命まで有効でなければならない。
旧WaveRT/Topology登録はブロックローカルWCHAR配列を渡しており、登録関数終了後
にPortClsが参照する名前が無効になる。両方をdriver-static writable配列へ変更した。
INFのMt8167WaveRT/Mt8167Topology参照文字列を維持する。
これは確定したDDI契約違反の修正であり、endpoint0の唯一の原因と断定しない。

Engine INFだけ1007へ上げ、NativeStart revisionも1007とする。親は1008の
同一binary/flagsを再利用可能。親/USB/WiFi/DOD/GPU、DMA設定、HFP、固定tone
profileのINF版は変更しない。新Engine SYS/INF/CATのWDK build/再署名が必要。

## 照合して維持する箇所

INFのAUDIO/RENDER/REALTIME Wave interfaceとAUDIO/TOPOLOGY interface、
PortCls登録名は一致。wave bridge out1→topology in0→speaker out1の向き、
ANALOG rangeとCOMMUNICATION_NONEのbridgeは公式speaker topologyと一致する。
EP/0 Associationはspeaker GUIDで、OEMFormatは48kHz/16bit/stereoの
serialized VT_BLOB。根拠なくEP indexやphysical connection方向を変えない。
CategoryCount=0は公式speaker topologyにも存在するため、これだけを欠陥扱いしない。
未実装jack detection/event-driven/追加formatを成功広告しない。

## 一次資料

- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-pcregistersubdevice
  Nameのdevice lifetime要件。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-endpoint-builder-algorithm
  AUDIO arrivalからunconnected bridge pinを調べPCM hostまで経路を追跡する。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/installing-device-interfaces-for-an-audio-adapter
  INF/interface referenceの一致契約。
- https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/speakertoptable.h
  built-in speakerのbridge direction/category/接続とdefault categories。

## 検証範囲

既存registration testへ、登録関数から戻った後のborrowed name使用を追加。
AddressSanitizerのstack-use-after-return検査で旧実装の寿命違反を検出し、
修正版を確認する。host確認はPortCls実登録/endpoint実生成の代替ではない。
実WDK buildと実MMDevice endpoint出現、実音声出力は別々の未確認項目。
