# H / E 最終native bindingの実装契約

H固定source: f492756aa51e6a19527b5d2ca65526a4d1c640dc。
Eのresolver実配線: 501083b、停止/IRQ候補を含む検証済みcheckpoint: 780b7c7。
共有repository: https://github.com/mouseos/a04br3-windows-drivers.git
E branch: work/ge8300-native-render-packet。
H branch: work/ge8300-wddm-render-sidecar-20261107。

## 分担（未実装をHへ委譲しない）

E: GeoNativeOwnerCreateV3/Delete本体、native allocation/queue/prepare/publish、
control-v2全table、実IRQ/visibility、停止/reset/epoch/rundownと失敗時資源保持。
H: 最終factory呼出し、実VidMm allocation/MDL resolver、frontendのadapter寿命、
UMD/PCO/NIR/draw stateおよびscheduler通知/miniportの実Dxgk IRQ登録。
GPU IRQはDxgkだけが所有。KMDF baseline providerを同時接続しない。

## 固定factory ABI（H native_binding.hが正本）

```c
NTSTATUS GeoNativeOwnerCreateV3(PDEVICE_OBJECT,
    const DXGKRNL_INTERFACE *, const DXGK_DEVICE_INFO *,
    const struct geo_frontend_memory *, struct geo_native_binding *);
NTSTATUS GeoNativeOwnerDelete(struct geo_native_binding *);
```

binding/control/owner_opsはversion2、frontend memoryはversion1。
Create成功後にcontrol.acquireでleaseを取得し、実IRQ通知初期化後restart。
partial Create失敗で保持が必要ならbinding.native_contextを残す。
Delete/release失敗ではそのcontext、callback、adapterを解放しない。
Eの現checkpointにはこのfactory symbol/table全体はまだ無い。
成功stub、unresolved無視、cold資源成功をGEO start成功へ置換してはならない。

## H resolverから実bindingまで（501083bに実装済み）

```c
NTSTATUS a04_native_allocation_bind_frontend_cold(
    struct a04_native_owner_allocation *owner,
    const struct geo_frontend_memory *memory,
    PMDL original_vidmm_mdl, ULONG active_bytes,
    uint64_t aperture_byte_offset, uint32_t packed_argb);
```

profileは800x600/pitch3200/active1920000/page_bytes1921024。
resolve成功のsize/version1/originGEO_MDL_VIDMM_CACHED_SHADOW/独立lease.mdlを検査。
lease.mdl（元VidMm MDLではない）を実OS SG/PFN/cache/target bindへ渡す。
native bindがlease.acquireで別参照を取得し、resolveの所有参照1個は成否ともrelease。
HのUnmapはmap参照だけをdrop。native参照が残れば独立pinを保持する。
元VidMm MDLのunlock/freeは禁止。unknown MDLやcache flagだけでは成功させない。
成功なのにrelease口が無いresolver結果はE ownerに保持してDeleteを拒否する。

Hに確認する寿命契約:
`memory_api.context`/callbackとadapter本体は、map参照が落ちてもnative参照・
failed ownerが存在する全期間、E Delete成功までresidentで保持すること。
確認/修正はこの固定ABIへ反映したcommitで返す。EはH待ちだけで作業を止めない。

## control-v2の必要な接続

- acquire: 排他的な実D0/IRQ resource所有、referenced lease/epoch。未資格capsを返さない。
- synchronize: 実DxgkCbSynchronizeExecutionに委譲。偽IRQL/別IRQ/常時TRUE不可。
- ops.create/bind/prepare/discard/submit: surface/job/tokenを元allocationへ結合。
  現実装のsubmitは実IRQ lock下のbounded publishのみ、acceptedとcompletedを区別。
- interrupt_event: 実ISRだけからnative dispatchを呼ぶ。未claimはNOT_FOUND。
  claimのみはPENDING（GEO event epochはlease epoch、job/generation/visibility=0）。
  実source＋後続GP fence＋cache proofの一致時だけGPU_DONE|CPU_VISIBLEを通知する。
- service: 単一PASSIVE workerでnative finish/cache→実IRQ同期内fresh GP arm。
  polling側からscheduler completionを通知しない。
- quiesce/reset/set_power/restart: FW off/IRQ counter drain/実HW停止、callback/work rundown、
  token retirement、epochを結合。mask/idle/FWackだけでGEO_PROOF_REQUIREDを返さない。
- release/Delete: live surface/job/binding/IRQ/work/interface参照が残れば保持してerror。
  exposed common-buffer/MDL/SGの解放は実停止とretirement後のみ。

Eの780b7c7は実ISR sampleのFW counter照合・OFF/queue空のsource close、
GPU0 scoped reset hold/readbackを追加したが、外部AXI write-response完了の契約が未確定。
NoDMA/全stop proofを広告しない。詳細はNATIVE_CORE_STOP_REVIEW.md。

## draw/shader semanticの現在範囲

owner公開操作の実装対象はpacked clear。固定triangle/texture producerはWindows実機で
solid→texture2job成功した独立経路で、動的VS/FS/vertex/constant入力を受けない。
PCOが生成した任意binaryを固定producerへ貼り付けるABIは存在しない。
Hの動的draw側は、そのsemanticと検証可能なresource/compiler出力契約を先に提示する。
Eがsource/同版一次根拠でPDS/USC/VDM/PPP配置・参照閉包・再配置を実装してから結合する。
固定capture VA再送、未取得zero補完、clearをdraw成功へ偽装することは禁止。

## build / 最終link

E helper: tools/ci/build-ge8300-native-owner-core.ps1
`-EnableCandidate -ExpectedSourceCommit <fixed> -WithMdlTarget -WithOwnerAllocation -LinkCheck`
780b7c7は実WDK ARM37TU（36TU検証再利用）＋kernel API LinkCheck成功。
H f492756はH報告で17 Clang TU＋Microsoft ARM SEH mdl_pin成功。
H SEH TUは正式Microsoft ARM compiler、他TUは既存LLVM/WDK契約。
最終同一SYSでのfactory/full-control linkは未実施。既存copy/DOD public ABIは変えない。
モジュールlink成功は実GPU IRQ/WDDM/D3D成功ではない。


## 2026-11-07 factory実結合の追記

7e09f6f はCreateV3/Deleteとcontrol-v2全callbackを実allocation/queue/IRQへ接続。
実WDK ARM32 43TU＋kernel API LinkCheck成功、runtime未検証。
親2502364＋機種UEFI2f6f73a3のSPM正規割当が必須（同版正常free callchain相当）。
新 `wddm-render/lifecycle.c` はH63bbe0b同blobを基に、release成功後に
actual control.synchronizeのIRQ lockでnotifications_ready/lease.cookie/service_pendingを
消去し、その後InterfaceDereferenceする。Delete前のconsumer pointer rundownに必要。
H canonicalへこの小変更を反映すること。H checkoutは直接変更していない。

最新TL flat profileとvertex byte_count>=4..64KiB、GPU input float4/stride16/48bytesを受領。
Eはvertex union PT＋実GetDrawOps prepareを継続。clear opsが実在することをdraw完成としない。
追加確認: 既存固定triangle producerは黒背景を初期化する診断profileである。
標準Renderへそのまま使えば既存target内容を壊す。drawでは背景load/preserveが必要であり、
黒clearとRenderの混同をしない。canonical flat入力USC/metadataは公式SM4→PCO生成24/64bytesに
限定検査可能だが、任意ISAを通さず実PDS/PPP/shared countを構築する。
直接H送信toolはserialization errorのため、main中継依頼（再送ループ無し）。


vertex create/bind実装の契約:
- format100/usage3、width=pitch=active bytes、height1、4..65536 bytes。16倍数制限なし。
- target469pages+vertex最大16pages、aperture4MiB内の任意aligned非重複区間。
  commit_limit485pages。target/vertexのPFN/CPU aliasと全native/FW resource aliasを拒否。
- target-firstは実PTをreadbackしてunion map、vertex-firstはcold保持しtarget attach時に
  private pixel mappingを除去して完全union PTを作る。vertexだけのFW startは禁止。
- vertexは独立Shadow MDLをresolve→native acquire→resolve release、実SGは
  WriteToDevice=TRUE。targetと同じ参照を偽装せず、正常停止まで両方保持。
- targetのactive bytesはbind/discard/rebootstrap時に変更しない。paddingだけA5 guard。
- GetDrawOps/prepare_drawは依然未公開。vertex登録をdraw成功としない。


## flat draw実producer（上記「未公開」を更新）

`GeoNativeOwnerGetDrawOpsV1`→実`prepare_draw`→`a04_vendor_native_prepare_draw_cold`を追加。
factory buildのA04_GE8300_NATIVE_OWNER_DRAW=1でsource-linkする（通常provider既定OFF）。
有限profile: physical800x600/pitch3200、float4/stride16の1triangle、H63の実PCO
VS24/temp0/input4/output4とBGRA PS64/temp5/shared4/constantmask1のみ。
metadataと全ISA bytesを固定source生成結果へ照合し、任意memory shaderは受理しない。
Hの半画素補正/TL変換を受領し、E PPPの+400/+300/.5 scale/offsetは変更しない。
public小RTはHが同じ本物MDLのphysical800x600に正規化する。index/staging CPU-onlyは
E native allocationへ登録しない。両owner ID/元MDL/patch/job参照はHが保持する。

診断用黒clear背景は使わず、source `pvr_uscgen_loadop`と同じX/Y special-registerを
使う非正規化nearest loaderをPCO生成。target自身のlinear800x600 descriptorを
正規化GPU VAから組み、通常/PR背景を両方loadへ結合。source208bytes/temp5/shared8、
2process生成一致。背景のGPU実動作はまだ未検証でありrun40固定fixture成功とは別。
GPU実fetchは同一aperture中の別の実vertex MDLを参照。PS constantsはimmutable blobから
実General backingへcopy/readback。USC/PDS/PPP/VDM/recordsも全てcopy/readback後にFW boot。
実prepareはGPU publishせず、既存bounded submit/actual IRQ/visibility経路を再利用する。

任意triangleには固定pixel oracleが無い。draw finishは実UFO/cleanup/CCB→SLC/GP fence→
同target MDLのCPU cache→padding guardを検査し、matching_pixelsを480000と偽らない。
その後のfresh GP実IRQでのみCPU_VISIBLE/GPU_DONEを返す。raster正否の実機試験は別。
未実装profileは正直にUNSUPPORTED、常時拒否stubではなくこの有限profileの実builderがある。


## Clear完了後の初回vertex bind / metadata複数化

Hの標準順序Clear→初回DrawではvertexがClearのGPU完了後に初めてbindされる。
`bind`はPREPARED/SUBMITTEDを拒否し、COMPLETEなら実stop→IRQ close→platform-off→
retire graphを通し、元target Shadow MDL/SG/CPU画像とIDを保持したままcold graphを
再構築してvertex union PTを付ける。COMPLETEを単に許すlive remapではない。
次Drawは同じtargetをLOAD。完了だけでNoDMAとは扱わない。

またCreateDeviceの暗黙backbuffer＋明示RTのmetadataを別々の参照IDで保持する。
metadata作成を単一current bindingの制約と混同しない。実active bindは
one-target469pages＋one-input最大16pagesで、commit_limit485pagesは不変。
VidMmが未residentの別RT metadataを作ることを理由にBUSYにしない。
既存roleの実bindがまだ残る場合はBUSYであり、別のMDLへ黙ってredirectしない。
prepared discardはactual Dxgk lockで当該tokenのsubmitを先に閉じてから正常stopする。

H731のflat SM2 tokensはhostで実PCO実行し、Eのallowlist VS24/PS64と全bytes/metadataが
一致した。source生成ログは指定ログ保存先 `windows-rt-native-h731-flat-pco-check.log`。


## WDDM preemption / resetの保証範囲

1 SubmitCommandはnative TA/3D pair1個。DMA-buffer boundaryはこのpairの実完了境界であり、
triangle途中/instruction boundary/単独TA↔3D途中のpreemptは提供しない。H preempt_pendingで
次publishを止め、現在jobがUFO/CCB/cleanup→SLC/GP→MDL cache→fresh実IRQで完了した後だけ
DMA_PREEMPTEDを通知する。未publish prepared tokenは保持し、cancelは実discard経路。
単なるFW IRQ/idle/pollをGPU_DONE/PREEMPTEDへ変換しない。

resetは全所有GPUの正常FW idle/OFF→RGXStop/reset保持→parent domain-off→IRQ/worker rundownを
要し、全TA/3D contextを退役し元MDL/SGを保持、外側epochを更新する。論理Node0/Engine0
単一なら対象範囲は同node全体であり、別TA/3D engineの独立resetとは広告しない。
現経路はFW hung/ACK不能/poison時に正常offを成功と偽らず保持するため、無応答からの
per-engine TDR recovery成功を保証しない。PreemptionCapsだけの書換えでWDDM1.2適合としない。

Clear/Renderは非同期。新bind/prepareは実PREPARED/SUBMITTEDが残る間、PASSIVEで最大2秒の
actual boundary待ちを行いmutexを解放して実worker/IRQを進める。rundown refは保持し、
closing/poison/timeoutならerrorで退役しない。Clear完了後だけcold rebootstrapする。
READY中の実IRQもempty FWCCBを検査してrearmするが、job完了は返さない。

旧d13 build helperのowner_create_delete_implemented=falseは旧stageの固定表記だった。
本sourceはWithFactory時のimplementationフラグを実入力に合わせて訂正する。
geo_full_control_ready=falseはmatched hardware qualification未完を意味し、常時拒否stubでない。
既存immutable manifestは改竄せず、新build manifest/supplementで区別する。

## 2026-11-07: typed SM4 profile と linked ABI2

- `2fd0f47a`: 実ownerの既存prepare_drawはformat2の実official SM4token列と
  VS24/PS64の全USC・metadataを照合する。既存ABI1 request/control-v2のまま
  呼べる。bounded instruction framingを追加し、未知token列/USCは拒否する。
- 一次入力はH63bbe0bのofficial-token-inputs.json
  SHA256 ede14dec409def27d79cb3f3e8021e8195dec38ce3e3193a4a2377dda8d167d4。
  VS DXBC52fbd84a2223899d0a1fb931679d49e8227d8632c48a4fd33ca912eab46a39f5、
  PS5035681297565a29167e2721f3e4ffcf81ca4c913614908cf40723c8785366fc。
  framing-only RET fixtureは実compiler tokenに交換した。実cold prepare modelも通過。
- ABI2は別request。`frontend/draw_link_abi.h` と `NATIVE_VARYING_ABI2.md` を参照。
  ABI1をversion2として読み替えない。1COLOR4/F32のsemantic snapshotを固定した。
- `a04_m111_build_varying_pds`: 実linked Z/W/COLOR係数DOUTIとPPP scalar。
  `a04_m111_build_linked_triangle_ppp`: 全800x600 PPPへ上記を接続。
  `a04_m111_build_linked_vertex_pds`: position4/color4のstride32 fetchと実temp割当。
- Hが次に渡す入力は、最終pair-linked POSITION4/COLOR4 VS/FSの実USC bytesと
  common temps/shareds/vtxins、VS vtxouts/ranges/counts、FS coeffs/ranges/uses.z/w、
  entry_offsetを記録した固定artifact。GPUVAや手書きPPPは不要。
- Eはその実生成コードを検査してowner prepare-v2と完全producerへ接続する。
  現ABI2 builder単体で実GPU成功/FL9_1 qualificationとは扱わない。
  Eの親1014停止契約、実MDL lease、actual IRQ/visibility/rundownは維持する。


### Typed SM4 viewport（有限800x600）

format2はCPUで座標を変更しない。PPPでxscale/offset=400/400、
yscale/offset=-300/300、zscale/offset=1/0を使用する。
D3D clip Zは0..w。format1の旧TL変換済みprofileは従来300/300,.5/.5を保持。
Mesa pvr_arch_cmd_buffer.c:7810-7843のheight/2とmaxDepth-minDepthを根拠とし、
D3DのYを負height viewportとして扱う。cullNONEは維持。


### prepare-v2実接続（有限source COLOR pair）

実API `GeoNativeOwnerGetDrawOpsV2` と `geo_native_draw_ops_v2.prepare_draw` を追加。
request_v2はbase.size=sizeof(request_v2)/version2、stride32、3vertices=96bytes。
全field契約・実source USC hash/サイズ/配置はNATIVE_VARYING_ABI2.md末尾。
E参照artifactはROOT/port/xnu/out/windows-rt-native-color-pair-source-v2/。
H最終SM4pair compilerの生成物とは別で、source NIRを実PCOへ通した参照pair。
USC VS88/FS80、0/5temps、0/0shareds、VS8in/out、FS20coeff/usesW1。
同一USCとofficial color SM4token列を検査して実cold ownerへ接続する。
Hは自身の実pair outputを比較してからV2へ接続すること。
任意USC admission、caps拡大、偽completion、準備時publishは追加しない。
同じMDL/SG、既存FW power/platform stop/rundown、poison保持を使用。
first typed flatは30b71470のV1 opsで先に進められる。


### 次のviewport producer（同800x600 backing内）

format2は整数の非負TopLeftX/Y、正Width/Height、矩形全体が800x600内、
MinDepth0/MaxDepth1を実PPPへ変換できる。半整数scale/offsetも正確に生成する。
カーネル側は整数命令のみ使用する。format1旧TLはfull800x600のまま。
Hは既存typed_draw.cのfull viewport制限をこの範囲に限って緩和し、
request.state.viewportへ実runtime値をcopyできる。前処理でvertexを変換しない。
これは任意サイズHWRT/target allocationやdepth bufferの実装とは区別する。
未対応fractional viewport/矩形外/depth rangeはエラーで返す。

### 親profile台帳更新

主担当の現在値はboot53d6/親1010。Eの停止/電源候補は
固定287241c/1014 WithSpm＋SD1、ACPI6e852b1を引き続き使用。
1016はwithdrawn署名で再使用不可、1018thermalはSD0専用候補、1020はmiccapture予約。
thermal同時profileにする場合はSDpower/audio DMA forwardingを含む全effective flagsを
統合した新versionが必要。1018を単純採用してEのSD1条件を失わないこと。
これは台帳の記録であり、現在の実機へ新tupleを適用した記録ではない。

### 最終KMD link時のvalidator所有

H KMD archiveが既存`geo_draw_validate_capture`を提供していても結合できるよう、
EのV2 entryは`frontend/draw_link_protocol.c`の独立objectに分離。
共通のframing/state実装はTU-private `draw_validation_private.h`。
V1 symbolを差し替えたりduplicate/unresolvedを無視するlink flagは使わない。
E factory helperのLinkCheckはGetDrawOpsV1/V2双方をexportして実参照を検査する。
