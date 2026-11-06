# native clearのcold owner接続契約 v1

対象は単一RT・MSAAなし・Z/S backingなしのnative TA/3D。Windows D3D UMD/完全WDDM render miniportとは別。完全payloadを確定するまでTA/3Dの実行ゲートは開かない。owner、DMA allocation、cold mapping、shadow queueの準備は並行実装できる。

## 分担と境界

runtime担当は既存hardware owner、poison、DMA adapterとuncached Device-memory access、cold mapping、queue capacity、FWCCB/completion/cleanup lifetimeを所有する。packet担当はREAでclear用TA/3D全recordとUSC/PDS/VDM/PPP/EOTの生成を確定する。現段階のnative_record_fieldsは完全payload factoryではない。stub成功やzero templateをfactoryにしない。

## FW bank追加範囲（Windows側で選定した固定契約）

既存render_resources.cの64KiB bankとMIPS mapを再利用し、FW baseは0xc0080000。現builderのusedは0x4680。追加前にlayout.used<=0x4800、bank_bytes>=0x4c40を検査する。別allocationを増設する契約ではない。render_buildがbank全体をゼロにするので、その後に追加builderのCPU shadowを書き込む。

| bank offset | bytes | 内容／FW address |
| --- | --- | --- |
| 0x4800 | 0x400 | a04_m111_native_bank、64-byte aligned、FW 0xc0084800 |
| 0x4800 | 0x10 | parent target |
| 0x4840 / 0x4880 | 各0x40 | freelist 0/1 |
| 0x48c0 / 0x4a48 | 各0x188 | HWRT slot 0/1 |
| 0x4990 / 0x4998 | 各8 | slot0 TA/3D cleanup |
| 0x4b18 / 0x4b20 | 各8 | slot1 TA/3D cleanup |
| 0x4c00 | 0x40 | 専用UFO領域。slot0 TA/3Dは+0/+4、slot1は+8/+12。cold値0 |

native bankの実offsetは必ずoffsetofから求め、上表とのABI assertionを置く。cleanupはHWRT内であり別allocateしない。既存sync +0x23c0と共有しない。TA contextは既存context+0、3Dは+0xa0。現builderではcontrol +0x340/+0x1380、ring +0x380/+0x13c0、各4096 bytes。主担当checkoutが変わった場合はlayoutの返却値を使う。

## GPU backing（確定容量と選択可能なpolicyを区別）

allocationは4KiBへ切上げる。64/16-byte suballocation alignmentはGPU VAについても満たす。CPU pointer、adapter DMA address、GPU VA、MIPS FW addressを別の型・所有者として保持する。

| 資源 | 有効bytes | alignment／初期化 |
| --- | --- | --- |
| region header 2組 | 各align64(ceil(W/16)*ceil(H/16)*5) | 64、vendor flags=3。必須初期内容は未確定 |
| TPC + VHeap | align64(side*side*8)+0x600、side=ceilPow2(max(tilesX,tilesY)) | 64、全域zeroがvendorで確定 |
| MLIST 2組 | 固定PB 16KiB〜16MiBでは各4096、計8192 | 16、vendor flags=3。必須初期内容は未確定 |
| dummy region + stencil | 128+2048=0x880 | region64/stencil16、初期内容未確定。TA+0x78=region VA、3D+0x128=stencil VA\|1 |
| PM pages local/global | 各pages*4096、pages>=4かつ4の倍数、現builder上限4096 pages/list | 4KiB、全物理ページを先に確保。GPU/FW/他listと非重複 |
| freelist table 2組 | 各pages*4、mirror有ならpages*8 | 16。CPUで全pageのadapter DMA>>12を構築し、mirrorも同内容 |

800x600: region実0x4a80→allocation0x5000、TPC/VHeap実0x8600→0x9000。
800x1280: region実0x9c80→0xa000、TPC/VHeap実0x20600→0x21000。
共通: MLIST0x2000、auxiliary0x1000。

初期bring-upのpolicy例はlocal PM=0x28000（40 pages）、global PM=0x100000（256 pages）、固定grow=0。これはUM local単一RT容量とglobal apphint既定値に基づく選択で、描画成功を保証する最小量ではない。各freelist tableはmirror込みでも4KiB allocationで足りる。800x1280の上記data backing計0x158000 bytes（code、VDM/PPP、output、page tablesは別）。OOM時に無根拠に小さいPBで再送しない。

## GPU VAとpage tables

native_resources自体は非zeroの40-bit VAを受けるが、実mappingではBRN65273の既存General窓を使う。Linux rgxheapconfig.hと既存Windows scratchは0x65c0000000。追加data用のWindows側予約案は[0x65c0200000,0x65c0400000)（2MiB）。既存scratch [0x65c0000000,0x65c0010000)とは非重複で、同じ1GiB PC entry内。これはvendor固定ABIではなくruntime ownerの選定範囲なので、既存allocation inventoryとの照合後に予約する。上記0x158000-byte policyは1 leafへ収まる。

既存PC/PDは再利用し、data arenaには追加4KiB PTを1枚用意できる。arenaを増やす場合は2MiB leafごとに4KiB PTを追加しmap_pages_multi_4k等を使う。PM物理ページはadapter logical addressで32-bit、4KiB aligned、重複なし。FW bankをGPU VAとして流用しない。

PDS exec base=0xa800000000、USC exec base=0xba00000000は既存FW initと一致させる。既存各64KiBのTQ code bankとの共有・上書きを避ける。native codeの必要量とbase-relative fieldはpacket解析中で未固定。outputは既存screen GPU VA 0x8000000000等を同じFWMEMCONTEXTのcatalogueへcold attachする。

GPU mapping/cache policyをallocation flagsから機械的に同一視しない。既存uncached DMA＋SLC_BYPASS方針を使う場合も、PM保護属性と全GPU masterのアクセス可否をruntime側で照合する。TPC zero、table生成、全data/table copyを完了してbarrierを発行し、FW起動前にMIPS/GPU mappingを公開する。既存BRN65273の2予約VA→zero page mappingを維持する。hardware_owned後のcold API呼出しは禁止。

## Queue公開と解放

- CPU shadow rings 4096*2、prepare_shadow_pair workspace8192、payload TA0x88/3D0x150を別所有。payloadが完全に確定した場合のみpairへ渡す。
- 1 frameはTA group224 bytes、3D group472 bytes。wrap用paddingを各group前に確保。CCB両方とKCCBの2 kick分を公開前に予約。CPU lockと既存hardware ownerを保持する。
- TAのPR fence valueはprevious、TA UPDATEはprevious+1、3D FENCE_PRは同値、3D UPDATEは専用completion値。wrap-around拒否を維持。両ringのDevice copyとbarrier後に既存KCCB公開手順を使う。
- cleanup submitted/executed countersの管理はownerの責任。builderはkickのcleanup addressを付けるだけで、submitted counterを増やさない。同版KMのqueue commit手順に合わせて両DMを管理する。
- 片方kick公開後の失敗は単なるCPU rollbackでは回復しない。poisonして全backingを保持する。TA/3D completionとcleanupを別々に確認し、停止・無出力を完了扱いしない。
- 成功時もhardware owner・queue drain・cleanupと既存FW停止条件を満たすまでDMA、tables、FW bank、sync、code/outputを解放・再利用しない。

## 解析の未確定境界

TA +0x3c/+0x44/+0x4c/+0x54/+0x80等の意味/初期化、3D clear/残りregister、USC/PDS/VDM/PPP/EOTが未確定。TPC以外の未確定初期内容を「全zeroが正解」と宣言しない。CPU allocationをzeroで確保する場合も安全なhost policyと記録し、GPU初期状態の一次証拠にはしない。

packet担当はStartFrame 0x15e9c8、SetupBGObject 0x11372cのclear分岐、DoKickTA 0x1612a0とRGXKickTA 0x126354の間をREAで追い、clear専用の完全recordと依存コードを次に渡す。owner/backing担当は本契約でcold準備とqueue公開部を進められるが、dummy complete payloadを公開して試験しない。

## owner実装後のHWRT register factory

`native_hwrt_profile`はRGXAddRenderTargetの単一RT・1x1 samples分岐を800x600/800x1280について実装する。ownerの実`target`/`mlists`と寸法を渡し、返した2組を既存`a04_vendor_native_bind_bank_cold`へ渡す。private m1.11翻訳単位内で使用し、modern providerのenumへm1.11型を露出しない。GPUへのpublishは行わない。

重要: `region_header_size`へ入るのはbyte容量ではなくtile数（1900/4000）。`tpc_size`はraw TPC bytes（0x8000/0x20000）、`tpc_stride=0`、`mtile_stride`はtile数。merge registerはvendor定数0x3e89309c/0x3fddb3d9/0x3fbb67b2からIEEE754 binary32 round-nearest/evenで算出した固定bitsで、実装内でFPUを使用しない。sample tableは08,08の1sample位置を反映した0x88。cold bind可能なmetadataであり、完全描画packetではない。

## 採用したsingle-emit EOT code factory

`native_eot_program`は即値loadを使わず、共有reg sh0/sh1からPBEへ単一emitするsource-generated USCと、同版UMの1-DMA PDS constructorを組む。必要な有効範囲はUSC16 bytes（temp0/shared2）、PDS40 bytes（32-byte data segment＋8-byte code、data先頭からcode+32）。USC entry alignment4、PDS data/code alignment16。DMA allocation/mappingは各4KiBへ切上げれば足りる。この量はEOT部分のみで、背景USC/VDM/PPPの総codespaceではない。

USCは既存ホストMesa PCOでGE8300 22.40.54.30向けに生成した。source baseは0d94b3f397fb9cf8470bb5600ada8af79ff5361a、再生成レシピはtools/native_eot_reference.c。使用した既存archiveのSHA256・確認したsource差分・生成手順は指定log領域のmanifestに保存する。生成物は48a080c280408001880000fff2ffffff。既存TQ EOTの命令bodyと一致し、group lengthと末尾16-byte alignmentが異なる。vendorの即値load encoderから算出した42/44/46-byte候補は未採用であり、mapper仕様に使わない。

factoryへ渡すemit_word0/1はPBE setupで確定した最終2wordで、record内2個の64-bit CSRとは別。PDSはtask64、emit_word0/1、control0x90000002、alignment padding、DOUTW0xf0040101、DOUTU0xf4000002。PDS taskは上記USC VAから構築する。pixel event registerにはdata/codeサイズとshared2の正しいbindingが必要で、そのregister builderは未完成。完全TA/3D公開は依然禁止。

code GPU VAは既存USC heap [0xba00000000,0xba40000000)、PDS heap [0xa800000000,0xa840000000)の中で所有者がcold予約する。既存TQ/clear各64KiBと重複しなければ、USC0xba00010000、PDS0xa800010000を各1page予約する案が使える。ただし最新owner inventoryの確認と同一FWMEMCONTEXTへのcold mappingが前提。背景側を含む全資源の量が確定したとは扱わない。

## 背景＋EOT codespace factory（cold mapping可能な範囲）

`a04_m111_build_clear_codespace`は背景packed32出力とEOTのUSC/PDS CPU shadowを生成する。USC有効32 bytes、PDS有効0xa8 bytes、各4KiB allocationで収まる。USC alignment4、PDS alignment16。VA全spanを各1GiB execution heap内で検査する。既存TQ bankと非重複ならUSC 0xba00010000 / PDS 0xa800010000の各1pageをcold mapできる。この容量は背景＋EOTのみで、今後のVDM/PPP/TA用programを含む総量ではない。追加FWbank領域は要求しない。

| host選定offset | 内容 |
| --- | --- |
| USC +0 / +0x10 | 背景 / EOT、各16 bytes |
| PDS +0 / +0x20 | 背景pixel data32 / DOUTU code4 |
| PDS +0x40 / +0x50 | 色shared-loader data16 / DOUTW END code4 |
| PDS +0x80 / +0xa0 | EOT data32 / code8 |

背景USCは`388a8087800800000020f3ffffffffff`、temp0/shared2。独立PCO recipe `tools/native_background_reference.c`はshared0からpacked32をoutput0へ書く。PDS shared-loaderはconstant64 slot0から色と未使用shared1=0を送り、control32 slot2=0x90000002。cc0/end1のDOUTW 0xf4020001はMesaの`pvr_pds_inst_encode_dout`で独立生成して照合した。pad zeroはhost packing policy。RGBA/format変換は行わず、入力は既にpacked済みのoutput register bit pattern。

record側background triple、pixel event binding、PBE final emit wordsは本factoryの出力に含めない。CPU生成・ASan/UBSan・ARM32 COFF compile確認は、WDK compile/linkやGPU実行確認とは別。完全TA/3Dと必須data初期内容は依然未確定でpublish不可。


`a04_m111_build_clear_code_bindings`はこのcodespace専用のheap相対background tripleとevent info/code/dataを返す。PDS VA 0xa800010000の場合、background={0x0001005000010000,0x0001004000000000,0x0080000000010001}、event info=0x202、code=0x100a0、data=0x10080、background pixel data=32 bytes。PR/通常backgroundのどちらへ接続するかは未確定のためrecordへ自動patchしない。

重要訂正: `FUN_00202600`のmetadata +0x24=N*2+3をPDS temp数5と解釈しない。`InitPixelEventCodeBlocks` 0x1c1a14がcode allocationのword数として消費する一次証拠ev_37321734325fed9a2e64d0359f96e085a1bc0a5215e61ea3e6bfd8a0c5dd2760で確認。今回のEOT命令列はconstant参照のみでPDS temp0、shared2、data8 wordsなのでEVENT_PIXEL_PDS_INFO=0x202。以前の解析候補0x242は不採用。background sizeinfoはSetupBGObjectのshared/data単位変換と0x204504の最低temp1を適用。コードbindingの確定は完全TA/3D payloadの確定ではない。


## packed32 linear RTのPBE factory

`native_clear_surface`は単一RT、single sample、linear layout3、packed8入力、output0、identity lane swizzle、norm/gamma/rotationなし、全extent clipの最終emit2wordとnative CSR2qwordを返す。同版GLES format helper 0x22c9c0（ev_a85befce74dbbd23327a59b658c855eab7b0ab00fce16487ae09aece873c2763）、format row0x22 at 0x26ac44（ev_76cf618c0315ebe655426b949e05d391fb66c064eb5dfe3b9ac336f9e1aab919）、layout table at0x265080（ev_f9776c4cf4ad0b451fd2c972655f47fa8e798d233dbc170979d57026a239cad7）を既存producer 0x20ff6cと照合。rowは1 plane、packmode0、norm0、swizzle0/1/2/3。API形式のRGBA float変換は行わない。

output VA0x8000000000、pitch800の場合、emit={0,0x80020}、CSR0=0x0200063c68800000、CSR1=(799<<32)|(height-1)。800x600と800x1280を試験し、surface→EOT codespace連結をASan/UBSan、ARM32 COFFで検証。既存native22 wide-stateと同じCSR配置、modern 3qword形式は使わない。callerはemitをcodespaceへ渡し、CSR pairをRT0へ配置する。完全record・backing・publicationを本factoryは提供しない。


## no-streamoutのVDM/PPP終端suffix

`a04_m111_build_terminate_stream`は同版OutputTerminateState 0x1c1facのstreamout objectなし・context+0x7e30 bit5なしの分岐を実装。一次証拠ev_d2f9a3486f50a6cddb02212742471cd5a74f401fb3d6259aa2a8b7c95e74f417、count0条件はev_58565dc9429b268065a2c583e3d74008ac11e518cbcba640b928c30ec53a7cf2。General dataに追加4KiB pageをcold mapし、VDMをpage+0（12 bytes）、PPPをpage+64（12 bytes）へ置く。host有効shadowは76 bytes。VDM STREAM_TERMINATEが最終page末尾64 bytesに入らない配置。PDS/USC/FWbankの増量は不要。

VDM={PPP address high|0x300,PPP address low&~3,0xc0000000}、PPP={0x02000000,screen_xy_max,0}。screen_xy_maxは同版native surface+0x150の完全wordを入力する。HWRT ppp_screenとは別registerであり、modern XMLの32-pixel単位へ変換しない。許容bitはX最大bits18..26、Y最大bits0..8。このsuffixだけで空のTA streamが描画可能であることは未証明。先頭state／clear trigger、TA register残部、data必須初期化の確定は引き続き必要。

Windows選定例として、data arenaが[0x65c0200000,0x65c0400000)で現allocationが0x158000 bytesなら、直後0x65c0358000へこの1pageを追加できる（owner inventory照合必須）。この場合PPP VA=0x65c0358040、data backing合計0x159000 bytes。物理ページとPT mapping、barrier、GPU owner前のcopy完了はruntime担当。screen_xy_maxの最終取得／寸法profile照合は未完了なので推測値をpublishしない。


clip入力の追加確定: `a04_m111_build_clear_terminate_stream(out,page_gpu,width,height)`はAddRTと同版getterを接続し、single sampleのceil(width/16)-1 / ceil(height/16)-1からclip wordを構築する。getter 0x1258b8、証拠ev_2a52e0ae5f4979e58962d3a101e91d124fc30380ece2c3640ad842414a2c4178はdataset+0x34/+0x38をそのまま返す。AddRT 0x124a34の既存証拠ev_f33018709a2c5a26acbe7e73ed517452c2a058a13e38b56f1fdac9c58fbc5752はceil(/16)-1を当該2fieldへ保存。StartFrame 0x15e9c8がX<<18|Yへ変換。800x600=0x00c40025、800x1280=0x00c4004f。直前の「寸法profile照合は未完了」はこの追加証拠で解決済み。空TA streamの成立・先頭state・完全recordは未確定のまま。


## TA PPP-state update programの追加（有効量更新）

同版InitPPPStateCodeBlocks 0x1c1b54（ev_5eda239ac555d3583d60b3d6f906f9109163e784ecbdbd250d02d23476938340）はUSC8 bytesを0x2628d2からコピーし、temp0のtaskとDOUTU-only PDSを生成してcontext+0x7208へ保持する。USC一次bytesは44a08005000000ff（ev_8df7dad9414e7836dc3e7f2cdb90b9f6589046fe632b35faa2cd968b134e3221）。StartFrameがこのPDS allocation baseをTA+0x40へ設定する。field旧名pds_code40は単独code addressではなくdata+code一体programのbase。

native_clear_codespaceへUSC+0x20の8 bytes、PDS+0xc0のdata32 bytes／+0xe0のcode4 bytesを追加。**最新有効量はUSC40 bytes、PDS0xe4 bytes**。各4KiB allocation・既存VA案は変更なし。bindings.ta_ppp_pds_baseはPDS VA0xa800010000で0x100c0。以前の32/0xa8は背景＋EOTのみの量として保持する。完全producerへ至るために必要なTA依存programであり、成功を装うhost stubではない。


## 全record初期化とclear-only経路の追跡

GLES ComputeFrameBufferCompleteness 0x13b680（ev_6bebd477c4efb285c7df4f57da94141286f2be25838e3c93a378efeefe93c778）はrender surfaceをcalloc(1,0x710)で作る。TA [0x538,0x5c0)、3D [0x5c0,0x710)を含むCPU初期値zeroは一次証拠で確認できた。ただし後続storeの網羅とfieldの未使用性をこのcallocだけで保証しない。

DoClear 0x125ca0（ev_1626a79a9018691d78ef6dd7e19fbfe85bf5826ba9e1e121e66bb1ff5d904b53）、fast-clear elision 0x16043c（ev_887ae4a56b753963ae0bd9599971feb219e482d1de296cc323e050c47d9dd020）は、色clear maskを消した場合にclear primitivesを送らず終了する分岐を持つ。その分岐は0x1621d4→0x200edcのfast clear register設定を必要とする。単に背景shader＋terminatorへ色を渡すだけの完全clear成立とは別なので、全profile確定前にpublishしない。TA+0x80（surface+0x5b8）はDoClearのmulti-sample/multi-targetループからindexが書かれるfieldであり、無条件paddingではない。single sample profileの初期値0はcalloc経路と照合する。

同版KM source configはPVR_LINUX_PHYSMEM_ZERO_ALL_PAGESを定義し、physmem_osmem_linux.cのfresh PMR allocationはbZero=TRUEへ上書きする。region/MLIST/auxのfresh physical pages zeroと一致するcold policyを取れるが、UM suballocatorで再利用される範囲まで毎回zeroと断定しない。

FW consumerを確認するため同梱rgx.fw.22.40.54.30（SHA256 5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1）をREAで開いたが、現在providerはMIPS16/microMIPS ASEをarchitecture_unsupportedとして拒否。別disassemblerによる迂回は行っていない。診断は指定logのwindows-rt-m111-fw-micromips-rea-blocker-20261107.json。

残る完全producer確定の別経路としてtools/gpu/README-recovery-native-clear.mdと元EGL/GLES pbuffer harness、DRM ioctl observerを準備。実機利用は主担当のRecovery許可後のみ。現状ARM64 object compileまででrunnable link未実施、runtime packetは未取得。


## 元vendor成功captureから確定したfast-clear色

元vendor runのTA/PR/3Dは0x88/0/0x150、800x600の全480000画素RGBA={17,34,51,255}一致。
これはWindows 3D成功ではない。captureはGPU VAの再送用テンプレートではない。
3D+4の0xff332211は同版caller0x1621d4がsurface+0x5c4を0x200edcへ渡す経路と一致する。
REA ev_66ed74a8cb7f8bae745867bb7fe94d75038c28abd178d53d5f58c0d3d458bd4a。
`a04_m111_patch_3d_fast_clear_rgba8`は確定した8bit laneのR|G<<8|B<<16|A<<24を+4へ配置する。
FPUを使わず、入力は既に量子化済みのRGBA8。+0のcontrolや他形式をこの関数で生成しない。

完全producerの阻害は、TA+0x10のVDM本体とPPP参照、3D+0x38/+0x40の疎資源内容・役割、
GPU-only dummy region/stencilの初期状態、残りregisterを含む生成順の網羅である。
取得済みPDS/USCは各programを個別に照合し、Windowsが所有するheap相対VAへ明示relocateする。
未取得をzeroにせず、完全producer提供までTA/3D publishを開けない。
