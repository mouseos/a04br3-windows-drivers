# 要求内MDL再利用

`A04_MSDC_REQUEST_MDL_REUSE=1`は同期A04RunIo内だけで512-byte bounce bufferのMDLを再利用する。既定は0。固定boot profileの署名入口は`tools/ci/build-msdc-quiet-boot.ps1 -ReuseRequestMdl`。この入口名は旧quiet候補から継承しているが、manifestで本機能を区別する。

MDLは最初の実転送で遅延確保・構築し、全sectorのcache flush、DMA完了・停止処理を終えてから、同期要求の出口で1回解放する。通常の4KiB要求でIoAllocateMdl/MmBuildMdlForNonPagedPool/IoFreeMdlを8組から1組へ減らす。cancelledを初回転送前に検出した要求は確保しない。確保失敗、clock failure、任意sector失敗、取消の出口も同じ1箇所を通る。初期化/GPT診断の単sector呼出しは従来どおり各呼出しで確保・解放する。

512-byte command/DMA、各sectorのKeFlushIoBuffers、data copy、timeout、DMA STOP、DAT0/CMD13、GPT write guard、queue/worker/reset/AdapterControlを変更しない。新しいpersistent mapping、MDL、DMA adapter、register writeを追加しない。既存bounce mappingのcacheability/physical DMA ownershipやStop/Restartの未検証条件を解決する変更ではない。MDLはDMA bus mappingそのものを作成するものではない。

`tests/msdc_request_mdl_test.py --out <project-root内dir>`は固定搭載source e2db926fと候補OFF/ONの実request/DMA wrapperをhost compileし、cache flush方向・MMIO・データ・status一致、MDL構築/解放回数、各sector位置での失敗/取消、確保失敗・clock failureを検証する。DMA hardwareとOS APIはmockであり、実機boot・電源・速度試験を代替しない。

表示名のMediaTek MT8167 MSDC0はcontroller、Samsung AJTD4Rは保存済みCID-derived PNMとsysfs診断で一致した搭載媒体名。未確認のマーケティング型番に変換しない。INFだけに媒体表示を加え、実行時SCSI identityの動的引継ぎを保持する。

MDL一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioallocatemdl
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmbuildmdlfornonpagedpool
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keflushiobuffers
