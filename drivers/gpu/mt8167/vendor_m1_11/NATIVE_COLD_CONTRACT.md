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

## Windows cold backingの実接続

`windows_native_cold`は、親render所有者に子leaseを持ち、32bit adapter DMAの連続data領域と追加PTを確保する。PM local40/global256ページを実DMA addressから構成し、独立したfreelist表とmirrorを作る。PCの親directory一致、未使用leaf、既存FW/render/copy資源との非重複を確認してcold PD entryを公開する。PMページには同版RGXGrowFreeListのPMMETA_PROTECTを使う。使用量は800x1280で0x158000 bytes、別にPT 4KiB。

`A04_GE8300_VENDOR_NATIVE_BACKING=1`は既定OFFで、providerの実cold準備へ接続する。完全なTA/3D packetを作成・送信するflagではない。region/MLIST/auxiliaryのゼロはhost allocation policyに限定し、TPC/VHeap以外の必須初期内容を推測しない。HWRT bank bindは独立して導出したregister値と所有GPU VAを検証し、MIPS bank+0x4800と専用UFOだけを初期化する。bank bindとTA/3D実行許可は別である。hardware_owned後は解放せず、cold解放でも親の他mappingを復元snapshotで上書きしない。

## owner実装後のHWRT register factory

`native_hwrt_profile`はRGXAddRenderTargetの単一RT・1x1 samples分岐を800x600/800x1280について実装する。ownerの実`target`/`mlists`と寸法を渡し、返した2組を既存`a04_vendor_native_bind_bank_cold`へ渡す。private m1.11翻訳単位内で使用し、modern providerのenumへm1.11型を露出しない。GPUへのpublishは行わない。

重要: `region_header_size`へ入るのはbyte容量ではなくtile数（1900/4000）。`tpc_size`はraw TPC bytes（0x8000/0x20000）、`tpc_stride=0`、`mtile_stride`はtile数。merge registerはvendor定数0x3e89309c/0x3fddb3d9/0x3fbb67b2からIEEE754 binary32 round-nearest/evenで算出した固定bitsで、実装内でFPUを使用しない。sample tableは08,08の1sample位置を反映した0x88。cold bind可能なmetadataであり、完全描画packetではない。

## 採用したsingle-emit EOT code factory

`native_eot_program`は即値loadを使わず、共有reg sh0/sh1からPBEへ単一emitするsource-generated USCと、同版UMの1-DMA PDS constructorを組む。必要な有効範囲はUSC16 bytes（temp0/shared2）、PDS40 bytes（32-byte data segment＋8-byte code、data先頭からcode+32）。USC entry alignment4、PDS data/code alignment16。DMA allocation/mappingは各4KiBへ切上げれば足りる。この量はEOT部分のみで、背景USC/VDM/PPPの総codespaceではない。

USCは既存ホストMesa PCOでGE8300 22.40.54.30向けに生成した。source baseは0d94b3f397fb9cf8470bb5600ada8af79ff5361a、再生成レシピはtools/native_eot_reference.c。使用した既存archiveのSHA256・確認したsource差分・生成手順は指定log領域のmanifestに保存する。生成物は48a080c280408001880000fff2ffffff。既存TQ EOTの命令bodyと一致し、group lengthと末尾16-byte alignmentが異なる。vendorの即値load encoderから算出した42/44/46-byte候補は未採用であり、mapper仕様に使わない。

factoryへ渡すemit_word0/1はPBE setupで確定した最終2wordで、record内2個の64-bit CSRとは別。PDSはtask64、emit_word0/1、control0x90000002、alignment padding、DOUTW0xf0040101、DOUTU0xf4000002。PDS taskは上記USC VAから構築する。pixel event registerにはdata/codeサイズとshared2の正しいbindingが必要で、そのregister builderは未完成。完全TA/3D公開は依然禁止。

code GPU VAは既存USC heap [0xba00000000,0xba40000000)、PDS heap [0xa800000000,0xa840000000)の中で所有者がcold予約する。既存TQ/clear各64KiBと重複しなければ、USC0xba00010000、PDS0xa800010000を各1page予約する案が使える。ただし最新owner inventoryの確認と同一FWMEMCONTEXTへのcold mappingが前提。背景側を含む全資源の量が確定したとは扱わない。
