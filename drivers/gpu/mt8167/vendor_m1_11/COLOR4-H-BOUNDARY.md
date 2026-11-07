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

## 固定metadataとspan（Hへの採用値）

|field|値|
|---|---|
|shader format/stage|format2、VS stage0 / PS stage1|
|VS|usc88 bytes、temps0/shareds0/vtxins8/vtxouts8/input_mask3/constant_mask0|
|FS|usc80 bytes、temps5/shareds0/vtxins0/vtxouts0/input_mask1/constant_mask0|
|USC SHA256 VS|c623f3047d0dec747d81ab3b298979fdde89576403de6af66f89eb8bf65d710f|
|USC SHA256 FS|2103c3048ed23a9dd336274c708a61b6fc21438e960e9766ee2e63d3de815796|
|token bytes VS/FS|104/56（H公式FL9_1 SHDRと全byte一致）|
|constants backing|各stage512 bytesのbounded span。mask0なのでshaderは参照しない|
|span規則|blob先頭相対offset、4byte aligned、全体<=GEO_DRAW_MAX_BLOB、USC extentは8byte倍数|
|link|size=sizeof、version2、varying_count1、reserved0|
|VS link|vtxouts8、position start0/count4、f32_smooth4、他f32/f16数0|
|FS link|coeffs20、usesZ0/usesW1、entry0/0|
|varying|semanticCOLOR1/index0/components4/bit_size32/interpolationSMOOTH1、VS4/4/offset0、FS4/16/offset0|
|vertex|実provider format100/usage3のlocked-MDL別allocation、active>=96、stride32/count3/first0/offset0|
|target|既存実provider800x600/pitch3200/BGRA target同一MDL、target_indexとvertex_indexは別|

定数spanはUSC/tokenと重ねないこと。packetは8byte aligned kernel capture、
request.base.size=sizeof(geo_draw_request_v2)/version2/blob_bytes=packet残量。
wire requestのtarget/vertexは0。解決後IDはHが実allocation listから渡す。
入力layoutのsignature意味・slot/register/formatはHが実runtime値と照合する。
Eはsnapshotを推測せずこの有限code/token/metadataとの一致を要求する。

API: a04_native_color_transport_bind / _prepare / _submit。
失敗は元のNTSTATUS/GEO statusを保持し、job0。prepareはPASSSIVE、submitは既存実Dxgk
同期lockのみ。署名候補SYSにはH COLOR dispatcher/UMDがまだ含まれないので、
単体で標準COLOR描画可能とは扱わない。H最終結合時にINF/CAT版を別固定する。
