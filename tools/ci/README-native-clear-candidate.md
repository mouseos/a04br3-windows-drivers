# native clearのhost接続とWDK recipe

28eee6eの初回800x600 producerを実Windows cold ownerへ接続する。元vendor capture04の成功はWindows 3D成功とは別。実機/runner操作はこの変更で行っていない。

## build時の選択

既存copy-service構成に次を追加する。

```powershell
$env:A04_GE8300_VENDOR_NATIVE_BACKING='1'
$env:A04_GE8300_VENDOR_NATIVE_CLEAR_JOB='1'
$env:A04_GE8300_VENDOR_COPY_SERVICE='1'
# 選択中packageのinventoryから設定する。保持ファイル名だけで決めない。
$env:A04_GE8300_INSTALLED_DRIVER_VER='11/06/2026,0.0.1.3'
$env:A04_GE8300_INSTALLED_SYS_SHA256='9c3c7d846565c0e5e73c2afe1d4d9900089f74bbe92d97ff8d1627729321265e'
./tools/ci/build-ge8300-provider.ps1
```

上記baselineのSYS hashはrun27 fresh backupと保持853abc2 packageで一致した値。INF DriverVerはその保持packageの値であり、実機で選択されたINF情報との照合を適用前に別途必須とする。新source INFは11/07/2026,0.0.2.0へ更新した。buildはbaselineより新しいversionと同日以降の日付を要求する。将来baselineが上がれば、再度source INFを更新してcommitする。同版packageを再配布しない。

既存のVendorStage/UncachedStage/RenderCold/CopyDiagnostic/SharedLease/CopyService依存は維持する。NativeClearJob未指定では従来1280 backing、指定時だけHWRTを含め800x600で準備する。output backingは従来4,096,000 bytesのまま、先頭480,000 pixelsをclear対象、残部544,000 pixelsをguardとして扱う。

署名後には、SYS/catalogの署名がValidな環境で以下を行う。

```powershell
./tools/ci/verify-ge8300-package.ps1 -Package $package `
  -InstalledDriverVer $env:A04_GE8300_INSTALLED_DRIVER_VER `
  -InstalledSysSha256 $env:A04_GE8300_INSTALLED_SYS_SHA256
```

provider-package-manifest.jsonは最終署名済SYS/INF/catalog/FWのSHA256、DriverVer、source commit、baselineを固定する。build-config.jsonのSYS hashは署名前と明記し、署名後hashと混同しない。適用後のselected INF DriverVerとSYS hashが最終manifestに一致して初めてnative candidate適用済みとする。現在の実機GPU成功は旧copy providerで、native未適用。

## cold準備とgate

`windows_native_job.c`は実ownerのdata/code/output/HWRT/aux/UFO addressからcomplete jobを生成し、既存native_pairを使って224/472-byte groupを作る。code、VDM/PPP/table、TA/3D ringをDevice-memory word helpersでcopy/readbackする。client control、KCCB、UFO、cleanup counterを公開・更新しない。HWRT bank全体とPM tables/mirror、region/MLIST/TPCのcold内容もreadback照合する。

NativeClearJob時は通常のTA/3D priority commandをskipし、staged ringが上書きされるのを防ぐ。GP/TQとcopy-serviceは維持する。正常cold準備のUARTはNATIVE JOB STAGED=1 TA3D_GATE=CLOSED。これは描画成功ではない。

`windows_native_execute.c`には未接続の実publisher本体を用意した。command_busyと既存lifetime所有を前提に2 KCCB slotを予約し、TA/3D、UFO、cleanup、SLC flush→GP fence、480,000画素とguardをboundedに検査する。one-shotであり、publish後timeout/未知FW要求/画素不一致はowner+commandをpoisonし全DMAを保持する。ui32SubmittedCommands/ui32ExecutedCommandsは同版rgx_fwif_shared.hのFW所有counterで、hostから増加させない。

**provider、D0、IOCTL、Engine/serviceからpublisherを呼ぶ経路は追加していない。NativeClearJob=1だけでは実行gateは開かない。** 実機向けactivationは別途、lifetime/lease・現在のqueue・対象package・実行上限を確認した変更が必要。

## dummy region/stencilの最終cold policy

same-version UM RGXCreateGlobalPB0x12454cとRGXCreateRenderContext0x1276fcはflags3でallocate/map後に戻る。DevmemSubAllocate0x112a80のflags3経路はCPU zero/poison処理をしない。同版Linux configはPVR_LINUX_PHYSMEM_ZERO_ALL_PAGESを定義し、fresh PMRはbZero=TRUE→__GFP_ZEROへ接続される。よって最初のcold candidateではfresh backingをzeroにするpolicyを採用する。

Windows ownerが実際にzeroを書いたregion128/stencil2048 bytesをFW開始前にreadbackして検査する。これはWindows側初期化policyの検証で、GPU-only Android runtime memoryを読めたという意味ではない。suballocator再利用やrepeat frameへは一般化しない。capture未観測は引き続き未観測として残す。

## 検証の範囲

host OS stubはallocator/IRQL等だけ。実producer、mapper、Device-word helpers、pair builderを使うcold ownerテストを実施する。FWモデルでは正常完了、timeout、画素不一致、cleanup未完了、one-shotとDMA保持を検証する。モデルはGPU実行の証拠ではない。WDK compile/link、署名、実Windows適用と描画は主担当runner/実機で未実施。gateを閉じた状態のWDK buildとcold準備確認から進める。
