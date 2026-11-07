# COLOR4限定候補とH境界

初回e837/run53は不変。この拡張をその媒体へ入れない。H frontendをEが変更しない。

## 実shader契約

公式SM4はHのfixed-shaders-69385809のCOLOR VS/PS。実token→有限decoder→NIR→
pco_link_nir/rev_link→PCOのsourceはtools/gpu/shaders/native-color-*とbuild-native-color-pair.py。
artifactはport/xnu/out/windows-rt-native-color-pair-sm4-v3。
VS USC88/temp0/shared0/input8/output8、FS USC80/temp5/shared0/coeff20/usesW1。
全USC/tokenはe837 profileへ一致。新しいISA/PPP word/VAを許可しない。
最終metadataはfrontend/draw_link_abi.hのgeo_draw_pair_linkへsnapshot。
position0/4、COLOR VS4/4、FScoeff4/16、smooth4、f16全0、entry0。

## wire→実bind→prepare→submit

native_color_transport.hが固定wire契約。magic GCD2/version2、2elementは
slot0/register0/offset0/float4とslot0/register1/offset16/float4、per-vertex/step0。
3頂点・stride32・96bytes。target/vertex native IDはwireで0。

Hで必要な実装:

1. immutable-VBへの96byte書込と実VidMm allocation/locked MDL lease。
2. COLOR pair compiler結果・実signature/input-layout照合。snapshotしたrequest_v2と
   blobをa04_color_packetへ格納し、allocation listのtarget/vertex indexを設定。
3. fault-safeな全packet copy後、indexから実provider IDを解決。別MDLやprivateRTへ置換しない。
4. GeoNativeOwnerGetDrawOpsV2と既存control-v2 acquireのlease.opsで
   a04_native_color_transport_bind。provider context一致必須。lease/interface寿命を保持。
5. PASSIVEでtransport_prepare。成功は実ownerのcold shader/resource/ring構築だけ。
6. 既存schedulerの実Dxgk同期lock内でtransport_submit。native ownerの本物submitへ委譲。
   fence0も有効。成功はacceptedであり完了ではない。
7. actual IRQ→service→visibility、discard、quiesce、poison+DMA保持は既存control-v2を使用。

transport自身はIRQ接続、D0取得、mapper、完了生成を持たない。未準備はエラー。
HはV1 packetをreinterpretせずGCD2を明示dispatchし、独立capture/size上限を守る。
Eのcallable transport/prepare-v2は実装済みだが、H frontendへのdispatchはこの境界で別実装。
全FL9_1適合やCOLOR実GPU成功は未確認。署名module候補を完成品SYS/UMDと混同しない。
