# native clearのhost接続とWDK recipe

28eee6eの初回800x600 producerを実Windows cold ownerへ接続する。元vendor capture04の成功はWindows 3D成功とは別。実機/runner操作はこの変更で行っていない。

## build時の選択

既存copy-service構成に次を追加する。

```powershell
$env:A04_GE8300_VENDOR_NATIVE_BACKING='1'
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_JOB='1'
$env:A04_GE8300_VENDOR_COPY_SERVICE='1'
# 選択中packageのinventoryから設定する。保持ファイル名だけで決めない。
$env:A04_GE8300_INSTALLED_DRIVER_VER='11/07/2026,0.0.2.2'
$env:A04_GE8300_INSTALLED_SYS_SHA256='3a5b1a0553dae7e09852432f838c3de255487a4b63d4c95f0a7be960eedeeaef'
./tools/ci/build-ge8300-provider.ps1
```

上記baselineはrun29で初回起動traceまで到達した04d0e57署名package。実機で選択されたINF情報と回収SYSの照合は適用前に別途必須とする。新source INFは11/07/2026,0.0.2.2へ更新した。buildはbaselineより新しいversionと同日以降の日付を要求する。将来baselineが上がれば、再度source INFを更新してcommitする。同版packageを再配布しない。

既存のVendorStage/UncachedStage/RenderCold/CopyDiagnostic/SharedLease/CopyService依存は維持する。NativeClearJob未指定では従来1280 backing、指定時だけHWRTを含め800x600で準備する。output backingは従来4,096,000 bytesのまま、先頭480,000 pixelsをclear対象、残部544,000 pixelsをguardとして扱う。

署名後には、SYS/catalogの署名がValidな環境で以下を行う。

```powershell
./tools/ci/verify-ge8300-package.ps1 -Package $package `
  -InstalledDriverVer $env:A04_GE8300_INSTALLED_DRIVER_VER `
  -InstalledSysSha256 $env:A04_GE8300_INSTALLED_SYS_SHA256
```

provider-package-manifest.jsonは最終署名済SYS/INF/catalog/FWのSHA256、DriverVer、source commit、baselineを固定する。build-config.jsonのSYS hashは署名前と明記し、署名後hashと混同しない。適用後のselected INF DriverVerとSYS hashが最終manifestに一致して初めてnative candidate適用済みとする。run29ではnative provider初回起動に到達したが、cold preparationがC000009Aで失敗し、TA/3Dは未実行。実GPUの3D成功証拠はまだない。

## cold準備とgate

`windows_native_job.c`は実ownerのdata/code/output/HWRT/aux/UFO addressからcomplete jobを生成し、既存native_pairを使って224/472-byte groupを作る。code、VDM/PPP/table、TA/3D ringをDevice-memory word helpersでcopy/readbackする。client control、KCCB、UFO、cleanup counterを公開・更新しない。HWRT bank全体とPM tables/mirror、region/MLIST/TPCのcold内容もreadback照合する。

NativeClearJob時は通常のTA/3D priority commandをskipし、staged ringが上書きされるのを防ぐ。GP/TQとcopy-serviceは維持する。正常cold準備のUARTはNATIVE JOB STAGED=1 TA3D_GATE=CLOSED。これは描画成功ではない。

`windows_native_execute.c`には実publisher本体を用意した。command_busyと既存lifetime所有を前提に2 KCCB slotを予約し、TA/3D、UFO、cleanup、SLC flush→GP fence、480,000画素とguardをboundedに検査する。one-shotであり、publish後timeout/未知FW要求/画素不一致はowner+commandをpoisonし全DMAを保持する。ui32SubmittedCommands/ui32ExecutedCommandsは同版rgx_fwif_shared.hのFW所有counterで、hostから増加させない。

**NativeClearJob=1だけでは実行gateは開かない。** 別の`A04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE`は既定0。1を明示したbuildだけ、既存D0 PostInterrupts callbackのvendor_lifetime内、GP fence成功直後でpublic opaque wrapperを1回呼ぶ。PASSIVE、pinned copy owner/device referenceの一致、display lease/ownerなし、service未公開・未closedを必須とする。display-copy完了を偽装したlease tokenは作らない。初期FW/native mapping所有は既存copy ownerと親render/FW leaseで保持する。

実機用の1回candidateは上の構成に以下を追加する。別途cold-only bootを挟む必要はない。

```powershell
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_ACTIVATE='1'
# 既存startup profileでも1だが、activationでは明示確認を必須にする。
$env:A04_GE8300_VENDOR_START_DIAGNOSTIC='1'
$env:A04_GE8300_SHARED_LEASE='1'
```

build-configと署名後manifestは`native_clear_activate=1`と`armed-one-shot-private-480000`を記録する。0なら`closed`。INF DriverVerは0.0.2.2へ更新済み。署名済SYS/INF/DriverVerが適用先と一致してから、この1回試験candidateと扱う。

UARTはcold準備成功かつjob staged時だけ`TA3D_GATE=ARMED_ONE_SHOT`、未準備時は`CLOSED_PREPARATION`、実行直前`NATIVE CLEAR BEGIN PRIVATE=480000`、終了時`NATIVE CLEAR STATUS/PHASE/MATCH/MISMATCH/GUARD`を出す。成功条件はstatus成功、phase4、match480000、mismatch0、guard0。実行前の所有条件不成立はSKIP=ADMISSIONであり成功ではない。preparation失敗かつchild完全cleanup時は、copy parentを維持してnative callerを呼ばない。失敗childが残存するときはFW start前に停止してDMAを保持する。

試験失敗はpublic wrapperでもpoison/quarantineを維持し、GPU service onlineを許可せずadmissionを永久closedにする。既存D0 PostInterruptsはSTATUS_SUCCESSでPnPへ返し、GPU未取得のDODには通常のCPU pathを残す。FW所有DMAは解放せず、未証明の`vendor_cpu_fallback=TRUE`を強制しない。UART reset経路を変更しない。Engine/service/IOCTLから任意packetやrepeatを送るentryは追加しない。

## dummy region/stencilの最終cold policy

same-version UM RGXCreateGlobalPB0x12454cとRGXCreateRenderContext0x1276fcはflags3でallocate/map後に戻る。DevmemSubAllocate0x112a80のflags3経路はCPU zero/poison処理をしない。同版Linux configはPVR_LINUX_PHYSMEM_ZERO_ALL_PAGESを定義し、fresh PMRはbZero=TRUE→__GFP_ZEROへ接続される。よって最初のcold candidateではfresh backingをzeroにするpolicyを採用する。

Windows ownerが実際にzeroを書いたregion128/stencil2048 bytesをFW開始前にreadbackして検査する。これはWindows側初期化policyの検証で、GPU-only Android runtime memoryを読めたという意味ではない。suballocator再利用やrepeat frameへは一般化しない。capture未観測は引き続き未観測として残す。

## 検証の範囲

host OS stubはallocator/IRQL等だけ。実producer、mapper、Device-word helpers、pair builderを使うcold ownerテストを実施する。FWモデルでは正常完了、timeout、画素不一致、cleanup未完了、one-shotとDMA保持を検証する。モデルはGPU実行の証拠ではない。WDK compile/link、署名、実Windows適用と描画は主担当runner/実機で未実施。source/host review後、明示activation=1のWDK build/署名と1回のprivate clear試験へ進む。WDK/実機を実施する権限は主担当が所有する。

## run29 preparation修正候補

04d0e57はGeneral 0x13c000 bytes、output 4,096,000 bytesを各1回のcommon-bufferで確保し、General全量+3pagesのCPU shadowも要求していた。C000009Aはowner/CPU pool/common-buffer/map-register capacityのどこからでも返る。run29はBYTES=0/STAGED=0なので、cleanup済みという観測だけから失敗した個別allocationを断定できない。stagedを確認せずcallerへ進み、C0000184を実行失敗としてpoisonしたgate欠陥はsourceとtraceから確定する。

新ownerは容量と正規化GPU VAを維持し、General/outputを最大64KiBの独立した実common-bufferへ分割する。800x600 Generalは20 blocks（最後48KiB）、outputは63 blocks（最後32KiB）。CPU pointerはblockを越えて合成せず、各PTEとPM PFN/mirrorは実adapter logical addressから作る。General zero/table stagingは16KiB、output table/sentinel stagingは20KiB。map-register capacityは最大の単一common-buffer要求16pagesに対して検査する。総数1000pagesを単一転送の必要数とは扱わない。これは総容量削減ではなく、連続確保依存の除去であり、実Windowsでの確保成功は未確認。

`NATIVE PREP STATUS/STEP/INDEX/REQUEST/MAPS/DATA_BYTES/DATA_BLOCKS/PIXEL_BLOCKS`はcleanup後も残す。STEPはpublic headerのOWNER=1、CAPACITY=2、DATA=3、PT=4、SHADOW=5、MAP=6、BANK=7、CODE=8、OUTPUT=9、JOB=10、DONE=11。INDEXはblock/code番号、output tableは65..67、output shadowはFFFFFFFF。REQUESTはその操作の要求bytes。JOBはcandidate pool要求。失敗段階と正のallocation数を同一bootで観測し、同条件の無根拠retryを避ける。

hostでは64KiB超のDMA/CPU pool要求を拒否し、block間DMAを16MiB離したallocatorを使用する。全PTE/PM PFN/mirror、block境界転送、全DMA確保箇所の失敗cleanup/alias quarantine、未prepared caller不実行、retained childのFW開始阻止、publish後timeout/画素不一致/cleanup未完了のpoison保持を確認する。host結果はWDK compile/linkや実GPU描画の代わりにはしない。

次のWDKは既存copy flagsを保ちNATIVE_BACKING/JOB/CLEAR_ACTIVATE=1、baseline0.0.2.1/SYS13c8、新INF0.0.2.2で1回だけ固定sourceから生成する。署名後verifyと実選択package照合を済ませ、主担当のprivate clear試験へ渡す。実機UARTのpreparation結果が未確定の間はWindows 3D成功と報告しない。

## run31成功後のbounded warm候補

run31はf1af516でPREP0/STEP0B/MAPS40/DATA_BYTES13C000/DATA_BLOCKS14/PIXEL_BLOCKS3F、JOBSTAGED1、CLEAR STATUS0/PHASE4/MATCH75300/MISMATCH0/GUARD0を実測した（数値はUARTの16進表記）。fresh32実SYS3a5b1a0553dae7e09852432f838c3de255487a4b63d4c95f0a7be960eedeeaefが署名候補と一致した。これは固定3D clear成功であり一般D3D対応ではない。

元vendor capture05 ev_7266a0cd35c075c4598512415d1d4037ec0695ab2eb8926eb5ef84b821ff7c4bは同一contextでcold/warm clear、solid/texture triangleの4jobs、各480000pixels mismatch0を観測した。HWRTはA/B/A/B、TA prior-fenceは0/0/2/2。Windows側は既存の独立TA-update→3D-fence/update契約を維持する。cold bank/HWRT/freelistは一切再初期化せず、完全完了後だけ同じ所有HWRT Aを直列再利用する候補であり、元UMのdouble-buffer運用と同じschedulerを再現したという意味ではない。このWindows warm経路は実機未検証。

`a04_vendor_native_run_clear_frames`は1..16の固定上限を持つ。実CCB read/writeが前回終端で一致し、TA/3D UFOとcleanup submitted/executedが完了数に一致し、前回flush+GP fenceが完了した場合のみ次pairを生成する。native_pairの同版reserve/paddingにより3D ring wrapを扱い、TA prior-fence/UPDATE/3D UPDATEとjob IDを次の値へ進める。cleanup counterをホストでincrementしない。新packetのscalar/code/VAはcold成功profileのままで、shaderやHWRT初期値を推測で差替えない。

各frame前にprivate output全容量をA5へ書きreadbackして確認し、各frame後に480000pixelsと残guard全量を再検査する。前回の同色結果が次回を成功させることはない。任意段階の失敗はpoison+DMA保持。callerは従来のD0 lifetime/PASSIVE/GP/pinned copy owner/lease無しを維持し、service公開前に一度だけbounded runを呼ぶ。

次のbuildは現baseline0.0.2.2/SYS3a5bに対してINF **11/07/2026,0.0.2.3**。全copy flagsを維持し、NATIVE_BACKING/JOB/CLEAR_ACTIVATE=1に加えて次を指定する。

```powershell
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_FRAMES='4'
```

未指定は1のまま。activation無しで複数frameや1..16外の指定はbuildで拒否する。build-config/署名後manifestへnative_clear_framesを保存する。UARTはBEGIN FRAMESと最終COMPLETED_FRAMESを出す。4frame試験の成功条件はSTATUS0/PHASE4/MATCH75300/MISMATCH0/GUARD0/COMPLETED_FRAMES4。new SYSを既存packageと同版INFで配布しない。

hostでは実producer/mapper/publisherを使い1frameと16frames（3D ring wrap）、2frame目timeout、前回cleanup破損、画素不一致、one-run制限と保持を検証した。実providerの準備/admission/caller blockをactivation0/1とframes1/4でcompile/runした。WDKとWindows warm clearは主担当の次試験であり未確認。triangle/textureのWindows producerはまだ未完成で、今回のwarm flagからは投入できない。

## triangle VDM外側streamの確定実装

`native_triangle_vdm`はcapture05のsolid/texture双方で共通の84bytes=21wordsをsourceのROGUE VDM field定義から生成する。PPP prefix/draw/terminateの40bit VA、PDS実行heap内data/code offsetを引数として再生成し、捕捉VAやraw task templateを埋め込まない。捕捉した両jobと全21wordsが一致した。CPU constructorのみであり、Windows publish gateやWDK source列には接続しない。

prefix PPP48bytes、draw PPP104/108bytes、terminate PPP12bytes、PDS/USC/vertex/texture backingの完全性は別の契約。prefix PPPはcapture05のGPU VA0x65c0001000にrange/PMRはあるがCPU mapping無しで未取得。VDMに含まれる最初の2-index LINE_LIST taskのshader役割も未確定。これらをzeroや仮shaderで埋めた完全packetは作らない。取得済みdraw PPPのpresence bitsとPDS shader/texture/uniform pointersを追い、prefix生成元のGLES init functionをREAで確認してから完全producerへ進める。
