# GPU providerだけを標準PnPで更新する最小経路

既存MT8167DeviceInstall serviceのGPU専用compile modeを使う。
Engine媒体・共通service source、DOD package、共有親、TOPRGUは変更しない。
更新先はACPI\MOU0006\0、hardware ID ACPI\MOU0006、service MT8167GE8300だけ。
任意EXE/任意package選択、shell、provider制御IOCTL、GPU workloadの実行はない。

## 固定候補と現在の未完了

- 候補source: 04d0e573bd1bee7b619d25441f8ac05d28d5ccd5。
- native_clear_activate=1、native_clear_job=1、native_backing=1。
- 新INF: sourceに記録された11/07/2026,0.0.2.1。署名済みINFをここで書き換えない。
- 旧baseline: INF11/06/2026,0.0.1.3、実SYS SHA-256
  9c3c7d846565c0e5e73c2afe1d4d9900089f74bbe92d97ff8d1627729321265e。
- firmwareは既存vendor版5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1。

実candidate SYS/CAT/最終署名manifestは未回収。架空hashや成功stubを埋めず、
実candidateがないとpolicy生成とinstaller buildを拒否する。
未replay SYSTEMやRegLoadAppKeyのBADDBから選択INFを確定しない。
主担当の既存native RegLoadKey成功helperによる照会結果と、確定済み実SYS hashを区別する。

## 最小再利用と分離

基点ff7d27dのtools/winpe/mt8167-device-install.cへA04_GPU_PROVIDER_UPDATEを追加した。
未指定の従来display modeは維持。GPU modeは別helperでのみbuildし、同名の
MT8167DeviceInstall.exeとして既存固定SCM ImagePathを再利用する。
データは別のC:\Windows\System32\MT8167GpuUpdate\providerへ固定する。
disklogはMT8167GpuUpdate\install.log、CREATE_NEWのmarkerは同directoryのattempted.bin。
従来displayのmarkerを削除したり、Engine serviceを再武装したりしない。

既存WinPE prepare template（3501ffb）はSYSTEMをWinPEでreg loadし、既存serviceも
拒否するため、そのまま使わない。新規の汎用媒体builderを増やさず、主担当の既存
guard付きcopy-only媒体へ下記の固定file mapとnative準備済みSYSTEMを渡す。

## 実package回収後のbuild

Einsteinの既存verify-ge8300-package.ps1が作るprovider-package-manifest.jsonと
build-config.jsonを含む、最終署名済みpackage directoryを入力する。
手入力の代用manifestやpre-sign SYS hashは受け付けない。

```powershell
powershell -File tools/ci/build-gpu-provider-update.ps1 -CandidateDirectory <実署名package> -OutputDirectory <checkout内の新directory> -Sign
```

Python helperは固定source、activation/job/backing、publish gate、baseline、
新DriverVer、全4fileの実hash/size、ARMNT PE、固定hardware/serviceを照合する。
manifestの正確なbytesとINF/SYS/CAT/firmwareのSHA-256を生成headerへ埋め込み、
そのheaderを含むservice EXEを署名する。公開鍵/信頼設定の無効化は行わない。
hostのmanifest/signature_status申告だけをtargetでの信頼検証の代わりにしない。
候補のbuild-configは履歴としてhashを記録するがtargetは実行/解釈しない。

サービス実行時は全5fileをread-only/share-readで保持し、reparse pointを拒否して
full-file SHA-256を比較する。CATをWinVerifyTrustで検証し、SHA-256 catalog member hashを
CryptCATAdminCalcHashFromFileHandle2で計算、INF/SYS/firmwareそれぞれのCAT membershipも
WinVerifyTrustで検証する。未信頼証明書、不一致、欠落はPnP操作前に失敗する。
オフライン向けcache-only/no-revocation設定であり、オンライン失効確認済みとは主張しない。

## 主担当の既存native準備とWinPEへの固定map

既に無効化されたMT8167DeviceInstallを再利用する場合、c98875bのnative準備helperへ
tools/gpu/native-deviceinstall-gpu-rearm.patchの最小差分を統合する。
この作業ではRaman/主担当の共通fileへ適用していない。
追加switch -RearmFixedGpuDeviceInstallはDeviceInstall単独に限定し、native replay後、
従来の正確なservice値検査を維持したままStart=4だけをStart=2へ戻す。
他の値・依存service・Selectの変更や、HardwareProbe/Engineの再武装は許さない。
clean candidateの再loadでは全値が正確に一致しなければ失敗する。

```powershell
powershell -File tools/recovery/prepare-offline-system-service.ps1 -SourceDirectory <回収SYSTEMとLOGのdirectory> -OutputDirectory <新directory> -ServiceName MT8167DeviceInstall -RearmFixedGpuDeviceInstall
```

既存native RegLoadKey成功手順でreplay・unload・clean再loadが完了したcandidateだけを使う。
RegLoadAppKeyのBADDBを回避するためのheader修正、ログ合成、WinPE reg loadは追加しない。
署名とnative準備の後、既存媒体のバックアップ/volume guard/copy-only手順へ渡す。

| build output | 通常OSへの固定配置 |
| --- | --- |
| MT8167DeviceInstall.exe | C:\Windows\System32\MT8167DeviceInstall.exe |
| providerの固定5file | C:\Windows\System32\MT8167GpuUpdate\provider\同名file |
| native候補candidate/SYSTEM | C:\Windows\System32\config\SYSTEM（既存の安全な置換手順） |

MT8167GpuUpdate/attempted.binが存在しないこと、同serviceを使う別媒体と同時に
予約されていないことを主担当が確認する。Engine serviceの実行設定は触らない。
installer/CATの署名者が既存の承認済みtrust storeで有効である必要がある。
共通cert登録処理は主担当の既存媒体で行い、任意証明書を受け取るbrokerは作らない。
新規driverをWinPEのDismでstageするだけで更新完了とはしない。

## 通常OSでの更新と計画再起動

serviceはLocalSystemを検査し、自己無効化→一回marker→必須disklogの順で進む。
固定devnodeのhardware/service/System classを確認し、実配置SYS hash9c3...と
標準SetupAPI propertyの選択DriverVersion=0.0.1.3を確認する。
選択INF名を正確なUTF-16でdisklogへ残す。disk hashをlive loaded imageのhashとは呼ばない。

検証後にSetupCopyOEMInfでpackageをstageする。stage成功を選択/起動成功とは扱わない。
SetupDiSetDeviceInstallParamsでDI_ENUMSINGLEINFとDI_DONOTCALLCONFIGMGを指定し、
固定INFだけのdriver listを作る。候補が1つでpinned DriverVersionに一致する場合のみ
SetupDiSetSelectedDriver→SetupDiCallClassInstaller(DIF_INSTALLDEVICE)を一度呼ぶ。
DI_NEEDREBOOTを設定し、live remove/reenumeration/disable/DIF_PROPERTYCHANGEは行わない。
active GPU/DOD leaseを手動で破棄せず、Windows標準インストールと次回bootへ延期する。

成功範囲はINSTALL_ACCEPTED_REBOOT_PENDING。稼働候補確認は常に0として記録し、
インストール前後のCM状態とinstall flags、実Win32 errorを保存する。
disklogをflush後にSeShutdownPrivilegeを取得し、InitiateSystemShutdownExWで
30秒後のplanned rebootを要求する。ForceAppsClosed=FALSE、独自resetは使わない。
要求成功・失敗を別記し、要求受付を実再起動や新provider起動成功とは扱わない。
UART通知は補助であり、その不在だけでは更新や計画再起動を拒否しない。

再起動後の選択INF/DriverVer、実配置SYS hash、PnP状態、DOD leaseとGPU観測は主担当回収。
このserviceはprivate-clear実行brokerではなく、opt-in providerを正式更新するだけである。

## 検証範囲と一次仕様

既存display/GPU両modeを実Windows API headerでARM32構文確認した。
GPU側はcandidate配列を宣言だけにした未リンクCOFFで、ARM helper/CRT不足も確認した。
これは実candidate、実署名installer、WDKリンク、更新成功や再生/3D成功の証拠ではない。
大量の模擬試験・実機/runner/REA操作は行っていない。

- [SP_DEVINSTALL_PARAMS flags](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w)
- [SetupDiCallClassInstaller](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdicallclassinstaller)
- [WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust)
- [CryptCATAdminCalcHashFromFileHandle2](https://learn.microsoft.com/en-us/windows/win32/api/mscat/nf-mscat-cryptcatadmincalchashfromfilehandle2)
- [InitiateSystemShutdownExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-initiatesystemshutdownexw)
