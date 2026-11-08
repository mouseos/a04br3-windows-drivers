# MT8167 Windows RT 8.1：4方向OS mode probe

DOD1.7/UEFI8e完成packageを変更しない別client。固定ディレクトリ `%SystemRoot%\System32\MT8167DisplayRotationProbeV1` に署名済3EXEを一組でstageし、既存LocalSystem診断から `mt8167-display-rotation-probe.exe` を引数なしで1回起動する。System32以外、reparse、任意引数は拒否する。新service/driver/IOCTL/capturehookは無い。

固定出力 `logs\display-rotation-probe-v1.log` はCREATE_NEW。既存logがあれば上書き/再試行しない。過去logを消して毎boot実行する運用ではなく、mainの次一括で新規stageする一回用候補。

## tokenとhandle

LocalSystem/session0/非impersonatingを確認し、SeTcb/SeAssignPrimaryToken/SeIncreaseQuotaを実tokenで必要時だけ有効化。AdjustTokenPrivilegesのPreviousStateを保存しfinallyで逆順復元する。WTSGetActiveConsoleSessionId/WTSQueryUserTokenから実consoleのprimary usertokenを得る。tokenのprincipal/sessionを変更せず取得handleを閉じる。user/password/SID/環境/画面内容はlogへ出さない。

CreateProcessAsUserWはcross-session handle継承を禁止する。そのため固定user監督 `mt8167-display-rotation-session.exe` をdefault desktopへbInheritHandles=FALSEで起動し、SYSTEMと当該userだけ許可したfirst-instance/local-only固定pipeで実生成client PIDを検証する。監督もserverのSYSTEM tokenを確認。logと匿名mappingの既存handle2個だけをDuplicateHandleする。

監督→固定worker `mt8167-display-rotation-user.exe` は同sessionで起動。STARTUPINFOEX HANDLE_LISTで固定loghandleとsnapshot handleだけを継承し、workerはsnapshotをFILE_MAP_READでmapする。任意command/引数/環境による操作選択は無い。

## 4方向と元mode復元

単一active pathをQueryDisplayConfigで選び、EnumDisplaySettingsExWの現在DEVMODEを一度だけ匿名memoryへ保存する。workerはDMDO_0/90/180/270各1回。元orientationが奇数なら元幅高さをnative寸法へ戻し、要求orientationが奇数のときだけswapする。各候補をCDS_TESTし、DISP_CHANGE_SUCCESSFUL=0のときだけflags0で適用する。UPDATEREGISTRY/GLOBAL/NORESET/unsafe flags、センサー値合成は使わない。

適用後のenum orientation/幅高さとQDC source寸法/rotation/target寸法を記録し、source adapter/id/target idと物理target active sizeの保持も検査する。QDC rotationとDEVMODE enumは同じ方向だと推測して相互代入しない。

workerのSEH finallyで原DEVMODEを復元。監督もworker終了後に原mode/QDCを確認し、違う場合だけCDS_TEST→flags0で復元する。worker30秒・監督90秒で有界。SYSTEM側はjobを終了して全process drain確認後、必要時だけ固定監督をrestore-only経路で1回起動する。この経路では共有snapshotの原modeしか使用せず4方向を再試行しない。worker失敗を復元成功だけで成功へ変えない。

OS終了、session消失、kernel内停止でjob drain不能などでは復元実成功を保証しない。drain不明時に競合applyしない。正常終了・例外・worker強制終了の復元経路をsource実装したもので、実機確認済みとの意味ではない。

## 成功条件と再現

BEGIN_MODE=0..3各1回、各TEST_RC/APPLY_RC/MODE_VERIFY_RC=0、要求と実enum/QDC sourceの寸法一致、元modeのRESTORE_VERIFY_RC/WORKER_RESTORE_RC/SUPERVISOR_RESTORE_RC=0、PRIVILEGE_RESTORE_RC=0、FINAL_RC=0が必要。RESTART_REQUIREDやAPI失敗を受理成功にしない。

観測するのはOS mode受理/寸法と原mode復元。DOD内部DDI直接trace、実センサー姿勢、touch、物理表示、GPU画素・描画成功を立証しない。固定logはSENSOR_ATTITUDE_VERIFIED/PHYSICAL_DISPLAY_VERIFIED/TOUCH_VERIFIED=0を明示する。

[EnumDisplaySettingsExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsexw)、[ChangeDisplaySettingsExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-changedisplaysettingsexw)、[QueryDisplayConfig](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig)、[CreateProcessAsUserW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessasuserw)、[WTSQueryUserToken](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsqueryusertoken)、[AdjustTokenPrivileges](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-adjusttokenprivileges)を一次契約とする。

`tools/ci/build-mt8167-display-rotation-probe.ps1 -EnableCandidate` へ既存KitHeaderRoot/VcToolsRoot/UcrtInclude/LibraryRoot/SignToolPath/新OutputDirectoryを渡す。3user-mode TUのみARM compile/linkし既存BCBF署名。SDKinstall/driverbuild無し。RT8.1 ARMの実PE exportへ全static importとforwarderを照合し、API不在をstubで代用しない。

`tests/display_rotation_plan_test.c` は実pure寸法helperの全元向き/要求向きと不正境界だけをhost実行する。Windows APIはmockしない。ビルドhostでもこのclientを実行しない。実mode/API/token/復元はmainの次通常起動で初めて検証する。
