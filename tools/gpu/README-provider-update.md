# GPU providerの固定policy付きroll-forward更新

固定対象はACPI\MOU0006\0、hardware ID ACPI\MOU0006、service MT8167GE8300。
既存MT8167DeviceInstall serviceをGPU専用modeでbuildする。EXEに埋め込んだ単一の
新旧packageと専用directoryだけを扱い、実行時に候補source/path/hashを指定できない。
任意EXE/command/firmware、GPU実行broker、live remove/disableは追加しない。

## run29で確認された更新経路と現在のbaseline

主担当回収のprivate-windows-rt-gpu-run29-diagnostics/MT8167GpuUpdate/install.logは、
CAT_MEMBER_STATUS 3件=0、DIF_FLAGS=0x00830120 / EX=0x1810、
INSTALL_ACCEPTED_REBOOT_PENDING、SCM STOPPED/3010確認、
PLANNED_REBOOT_REQUEST_ACCEPTED=1 / ERROR=0 / WAITER_EXIT=0を記録した。
UART_NOTIFY_ERROR=0はqueue受付であり、wire最終通知の受信とは区別する。
次bootの新traceとrun30の実配置SYS一致も主担当が確認した。

現在のbaselineは回収済みwindows-rt-gpu-native-04d0e57の最終署名package。
source=04d0e573bd1bee7b619d25441f8ac05d28d5ccd5、INF=11/07/2026,0.0.2.1、
SYS SHA-256=13c8a9ca6b6d7fef921afd38d60cf84ea8e37b599b51af82d6e90342d8b9da07。
3DはJOBSTAGED=0 / STAGE=C000009A、CLEAR=C0000184、phase0で、command未publish。
更新成功と3D成功は別に扱い、3Dの原因修正はEinstein担当とする。

旧d929のbaseline0.0.1.3/9c3、04d0e57候補、MT8167GpuUpdate固定directoryは
初回更新の履歴であり、そのEXEを次回更新に再使用しない。
未replay SYSTEMやRegLoadAppKeyのBADDBから選択INFを推測しない。

## 次の署名済みcandidateから専用EXEをbuildする

```powershell
powershell -File tools/ci/build-gpu-provider-update.ps1 -BaselineDirectory <回収済みwindows-rt-gpu-native-04d0e57> -CandidateDirectory <次のflat最終署名package> -OutputDirectory <checkout内の新directory> -Sign
```

Python policy helperの直接入口も --baseline / --candidate / --output が必須。
旧packageと新packageの両方について、以下をbuild時に検査する。

- signed-manifest.jsonの固定7file名・重複なし・実hash、provider-package-manifest.jsonの
  最終署名state/Valid申告、build-config.jsonとsource/DriverVer/INF hashの一致。
- full source commitにあるGPU INFと実INFの一致。newline差だけは正規化する。
- 実INF/SYS/CAT/FWのhash/size、ARMNT PE、固定hardwareID/service、firmware5c1。
- BCBF6C456C6F0DD0818434763B189488FBF13570のsigner/certificate identity。
  hostのmetadata照合と、署名者trust storeによる検証は別確認である。

新candidateはactivation/job/backing=1と既存の明示publish gateが必要。
DriverDateはbaseline以上、4成分DriverVersionはbaselineより厳密に大きく、SYSも異なること。
新candidateのselected_installed_driver_ver / selected_installed_sys_sha256と、
build-configのinstalled_driver_ver / installed_sys_sha256が実baseline packageに一致しなければ拒否する。
したがって次GPU build/署名検証もbaseline=11/07/2026,0.0.2.1 / 13c8...として行う。
署名後INFの手修正、pre-sign SYS hash、欠落packageや架空の次hashで成功させない。

署名済みEXEへ、新旧source/版、旧SYS hash、candidate manifestの正確なbytesと
INF/SYS/CAT/FWのhashをheaderとしてpinする。headerのGPU_BASELINE_DRIVER_VERSIONと
GPU_UPDATE_DIRECTORYもcompile時に固定し、実行時のJSON解釈やpath指定は受け付けない。
専用EXEのbuildだけであり、GPU/parent/audio SYSの再compileは行わない。

## 候補ごとのdirectoryと一回限りの実行

policyは次のsuffixを決定的に生成する。

```text
\MT8167GpuUpdate-<candidateのfull40hex source>-<64hex transition ID>
```

transition IDはcandidate source、baseline SYS hash、candidate SYS hash、candidate INF hash
からSHA-256で生成する。全構成要素は検証済みhexだけで、traversalや任意directoryを許さない。
同じ新旧packageを別のbuild出力へ作り直しても同じdirectoryになり、manifestの空白変更でも変わらない。
次の高版candidateには別のdirectoryを割り当てる。

各directory内にproviderの固定5file、install.log、attempted.bin、reboot-attempted.binを置く。
CREATE_NEWのmarkerと自己無効化を維持し、marker/logの削除・上書き・archive移動・自動再armはしない。
旧MT8167GpuUpdateと過去候補directoryはそのまま保存する。
再投入しても同じdirectoryのattempted.binで停止し、更新後は旧baseline hash/版の不一致でも拒否する。
新しいdirectoryを生成するだけでSCMを再武装することはできない。

## 主担当/Ramanとの受け渡し契約（policy schema 2）

policy-manifest.jsonは既存candidate_source_commit / driver_ver / files / policy_sha256に加え、
baseline_source_commit、baseline_driver_ver、baseline_sys_sha256、
baseline_signed_manifest_sha256、transition_id、data_directory_suffix、data_directoryを記録する。
installer-manifest.jsonもpolicy_schema=2と同じdirectory/transition/baseline情報を別記し、
source_commit / exe_sha256とcandidate policyを結びつける。

主担当所有のbuild-private-system-apply-media.py / run-private-system-apply.cmd.in /
gpu_media_input.py / private_hardware_probe_input.pyは、この作業では変更しない。
媒体側は署名済みinstaller manifestの新しい固定directoryを検査して使う必要がある。
旧MT8167GpuUpdate固定mapやd929 sourceのままではschema 2を受け付けない。
長さを含めruntimeもMAX_PATH内で検査する。標準System32 pathなら最長fileも260文字未満。

| build output | 通常OSへの配置 |
| --- | --- |
| MT8167DeviceInstall.exe | C:\Windows\System32\MT8167DeviceInstall.exe |
| providerの固定5file | installer-manifest.jsonのdata_directory\provider\同名file |
| native準備済みclean SYSTEM | 主担当の既存copy-only安全手順 |

native planの再武装・fresh Recovery由来/fingerprint確認はRaman別担当。
planは次candidate/installer/policy/新directoryと実disabled serviceをpinする必要がある。
このGPU installerはnative planの作成/書換えやSCM自動再armをしない。
保存済みnative-deviceinstall-gpu-rearm.patchは083021a向けの初回更新履歴であり、
次candidateへそのまま再適用しない。共通native helper、Engine、主担当所有4fileには触れない。

## 実機で維持する更新と再起動の順序

serviceはLocalSystemを確認して自己無効化し、専用directoryの一回markerをclaimする。
固定devnodeのhardware/service/System class、EXEにpinしたbaselineの実配置SYS hashと
選択DriverVersionを標準SetupAPIで確認する。選択INF名と新旧source/版はdisklogへ記録する。
配置SYS hashをlive loaded image hashとは呼ばない。

全5fileをread-only/share-readで保持し、reparseを拒否、full-file hashを照合する。
WinVerifyTrustでCATのtrustとINF/SYS/FWのSHA-256 catalog membershipを検証する。
full-file SYS hashとAuthenticode member hashは別に計算する。
BCBFの承認済みtarget trustが必要で、cache-only/no-revocationをオンライン失効確認とは呼ばない。

検証後だけSetupCopyOEMInfでstageし、固定INFの1候補を選ぶ。
DI_DONOTCALLCONFIGMG / DI_NEEDREBOOT付きDIF_INSTALLDEVICEを一度実行し、
live remove/reenumeration/disable/DIF_PROPERTYCHANGEやDOD lease強制破棄を行わない。
成功範囲はINSTALL_ACCEPTED_REBOOT_PENDINGまでとする。

結果disklog flushとUART補助通知の試行完了後、同じ署名EXEの固定--reboot-after-stop
workerを一つ起動する。workerがSCM STOPPED/3010を確認し、候補別reboot markerを
write/flushしてからSeShutdownPrivilegeを取得し、InitiateSystemShutdownExWで
30秒後のplanned rebootを要求する。ForceAppsClosed=FALSE。UART hard resetは使わない。
SCM終了の最大30秒待機はDMA quiescence証明ではない。disklog必須/UART任意、
要求受付・実再起動・次provider動作・3D完了をそれぞれ区別する。

## 確認範囲

実0.0.2.1/13c8署名packageをbaselineとして再検証し、同じpackageへの更新が
高版条件で拒否され、policy出力が生成されないことを確認した。
新しいcandidateは未回収であり、新policy生成成功やWDK link/sign、次回実機更新成功は未確認。
GPU/default両modeの実API headerによるARM32 source確認は、実機再試験とは別である。
小試験の大量追加、runner/実機/REA/SSH操作は行わない。

schema 2媒体では`--gpu-baseline-package`と`--gpu-candidate-package`も必須。
最終署名manifest・sourceのINF・実fileを共通policy validatorで照合し、fresh backupの
実GPU hashがbaselineに一致すること、native planの候補source/SYS/CATが同じ新候補を
指すことを検査する。配置先はsource/transition由来の固定hex directoryであり、旧directoryを
削除・上書きしない。同じ候補のmarkerが残る場合は再試行しない。
