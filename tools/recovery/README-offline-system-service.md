# Windows native APIでoffline SYSTEMをreplayし一回限りサービスを登録

対象はprivateなSYSTEMとSYSTEM.LOG1/.LOG2。元ファイルには書かず、新規workcopyを標準RegLoadKeyWでload/replayし、指定サービスだけを登録してRegUnLoadKeyWで保存する。driver/SOFTWARE/DRIVERSのファイルは変更しない。実機へのコピー・再起動・runner起動や接続は行わない。

## 最初の実行：MT8167DeviceInstallだけ

既存Windows runnerの昇格済み64-bit PowerShell/pwshで、このコミットの2ファイルが同じtools/recoveryにある状態で実行する。SYSTEMと両LOGは同じinput directoryに置く。

```powershell
.\tools\recovery\prepare-offline-system-service.ps1 `
    -SourceDirectory 'C:\mt8167-system-run24\input' `
    -OutputDirectory 'C:\mt8167-system-run24\prepared-new-01' `
    -TimeoutSeconds 30
```

出力先は必ず新規。input内や既存出力先は拒否する。必要なら `-ExpectedSystemSha256` / `-ExpectedLog1Sha256` / `-ExpectedLog2Sha256` でホストbackup由来hashを明示できる。省略時も入力hashを取得し、workcopy一致と処理終了時のoriginal不変を検査する。

既定は従来どおりMT8167DeviceInstallだけ。`-ServiceName MT8167HardwareProbe` はHardwareProbeだけを登録し、既存DeviceInstall版の登録・再武装を行わない。名前とEXEの絶対パスは2件の固定allowlistであり、任意サービス名・任意ImagePath・追加引数は受け付けない。内部workerも同じallowlistを照合する。

### bulk準備：MT8167HardwareProbeだけ

```powershell
.\tools\recovery\prepare-offline-system-service.ps1 `
    -SourceDirectory 'C:\mt8167-system-bulk\input' `
    -OutputDirectory 'C:\mt8167-system-bulk\prepared-probe-new-01' `
    -ServiceName MT8167HardwareProbe `
    -TimeoutSeconds 30
```

固定ImagePathは `"C:\Windows\System32\MT8167HardwareProbe.exe"`（REG_EXPAND_SZ、引用符を含む）、DisplayNameは `MediaTek MT8167 Hardware Probe`。ほかの型・値は下表と同じで、DependOnServiceは実在照合するDeviceInstall/RpcSsだけ。EXE、clients、一回限りmarkerの配置・不存在確認はbulk担当側で行う。本レシピはprivate hiveやファイル配備を自動で行わない。

両方を同じ新規workcopyへ登録する従来の明示指定 `-IncludeHardwareProbe` も保持する。`-ServiceName MT8167HardwareProbe` と併用してもDeviceInstallを暗黙追加しない。登録対象はresult.jsonのservicesとworkerの正確な7値に記録する。

bulk側はnative準備成功のcandidate/SYSTEMを私用copy-only媒体へ渡し、旧prepare-mt8167-hardware-probe.cmd.inのWinPE RegLoad SYSTEMを併用しない。builder修正はbulk担当側で行う。

## 処理と正確なサービス値

1. 原本3ファイルを新規work directoryへ名前を保って複製する。transaction logsはSYSTEMと同じdirectoryに置く。入力のformat 1.5とbase-block checksumを確認する。dirty sequenceは許容する。
2. 子プロセス内でSeBackupPrivilege/SeRestorePrivilegeを有効化し、一意なHKLMのprivate aliasへRegLoadKeyWでloadする。Windows native APIによるreplayを使い、Pythonでlogを合成したりheaderの版・sequence/checksumを手修正したりしない。
3. replay後のSelect.Current=1を確認する。次回起動先の食い違いを避けるためSelect.Defaultも同じ1でなければ拒否する。両Select値自体は変更しない。
4. ControlSet001/ServicesのDeviceInstall(Type=32, Start=3)とRpcSs(Type=32, Start=2)の実在・DWORD型を検査する。PlugPlayは追加しない。
5. 選択したControlSet001/Services/MT8167DeviceInstallまたはMT8167HardwareProbeを固定値どおり登録する。未登録または互換性のある部分登録だけを許可し、既存Start=4等を勝手に再武装しない。
6. registry handlesを閉じ、RegUnLoadKeyWを成功させる。process tokenの元のprivilege状態も復元する。
7. 保存SYSTEMのregf、major=1/minor=5、primary/direct format、checksum、primary/secondary sequence一致を検査する。hostがformatを更新した場合は拒否し、版の書換えで取り繕わない。
8. clean SYSTEMだけを別verify directoryへ複製し、LOG無しで再loadして新serviceの7値と、Select・既存DeviceInstall/RpcSsの関連設定fingerprintを確認、unloadする。原本とcandidateの不変・コピー一致を確認する。

| 値 | 型 | 内容 |
| --- | --- | --- |
| Type | REG_DWORD | 16 |
| Start | REG_DWORD | 2 |
| ErrorControl | REG_DWORD | 1 |
| ImagePath | REG_EXPAND_SZ | `"C:\Windows\System32\MT8167DeviceInstall.exe"`（引用符を含む） |
| ObjectName | REG_SZ | LocalSystem |
| DisplayName | REG_SZ | MediaTek MT8167 Device Installation |
| DependOnService | REG_MULTI_SZ | DeviceInstall、RpcSsの2要素 |

検査スコープは **Select、および既存Services/DeviceInstall・Services/RpcSsそれぞれの直下のraw値（名前・型・長さ・SHA-256）のfingerprint、選択した新serviceに書く7値の型と内容の正確照合だけ**。関連既存設定は登録前後とclean candidate再load後に一致することを確認する。比較基点はnative replay後なので、log replayによる更新とservice登録を区別する。

**全SYSTEMのkey列挙・監査は行わない。** Lsa等の保護keyや関連serviceの子keyへ再帰しない。ACL変更や保護keyを迂回する処理は追加しない。APIの書込み先は固定された新service keyだけで、driverバイナリは扱わないが、この限定検査を全SYSTEMの無変更証明とは呼ばない。resultとworker stateに `whole_system_audit=false` と `audit_scope` を明記する。registry linkは辿って変更しない。

## timeoutと配備可否

native APIを呼ぶ子プロセス全体を既定30秒で監督する。timeout時は子を終了要求し、別の5秒上限のcleanup子プロセスで同じ一意aliasのunloadを試みる。親から無期限のRegLoad/UnLoadは呼ばない。停止したkernel callの即時終了やVM全体の復旧までは保証できず、解除確認不能時は配備不可。timeoutしたworkcopyを使用しない。

成功条件はscript正常終了とprivate `result.json` の `status=verified-clean-system-services-only`。配備候補は **candidate/SYSTEMだけ**。resultには入力3ファイルhash/長さ、入力と出力の版/checksum/sequence、native API戻り値、30秒完了有無、正確なサービス値、関連既存設定だけのfingerprintと監査スコープ、candidate hash、recipe/native-source hashを保存する。

work/verify directoryや、その処理中に生成されたLOGを配備しない。入力の古いLOGをcandidateに添えてreplayさせない。EXE/INF/SYS/CAT配備、attempted.bin不存在の確認、private WinPE媒体によるclean SYSTEMコピーとvolume releaseは主担当で行う。依存サービスの存在はdevnode列挙完了の保証ではなく、実bindは通常Windowsで確認する。

このレシピ・C# wrapperは公開可能だが、実SYSTEM/LOG・candidate・private resultを公開repoやCI公開artifactへ含めない。端末hiveを含む私用媒体の作成・配備は主担当の明示した私用運用として区別する。本レシピ自体のnative Windows実行・実機起動は未実施。

## 第3の固定名：MT8167EngineProbe

`-ServiceName MT8167EngineProbe`だけを明示したfresh native準備は、
ImagePath="C:\Windows\System32\MT8167EngineProbe.exe"、
DisplayName=MediaTek MT8167 Engine Probeを持つ別serviceを作る。
Type=16/Start=2/ErrorControl=1/ObjectName=LocalSystem/DependOnService=DeviceInstall,RpcSsは同じ固定7値。
既定ServiceName=MT8167DeviceInstallは変えず、任意名・EXE・argsを受け付けない。
Engine選択と-IncludeHardwareProbeの併用は拒否し、旧service/keyの再武装をしない。
監査範囲は従来のSelect・DeviceInstall/RpcSs直下raw値fingerprintと選択service7値だけ。
全SYSTEM監査やACL迂回を追加しない。

run27のfresh backupと同じSYSTEM/LOGからこのEngine単独候補を作り、
媒体builderのEngine modeへSYSTEM/result/hashを渡す。旧tone対象のnative resultは拒否される。
旧markerを削除せず、新markerはMT8167EngineProbe/attempted.binに独立する。
このsource更新ではnative処理や端末へのコピーは実行していない。

## 明示Engine再arm（次反復）

`-RearmEngineProbe -EngineRearmPlanPath <private plan> -ExpectedEngineRearmPlanSha256 <64桁SHA256>` は
ServiceName=MT8167EngineProbeだけで使用できる。旧HardwareProbeには適用しない。
媒体builderのplan生成modeでfresh backup/修正package/昇版に結び付けたplanを先に作る。
Start4の既知7値raw fingerprintと既存subkey無しが一致した場合だけStart2へ戻す。
未知の登録、Start2の再利用、無指定でのdisabled service再armは引き続き拒否する。
結果にplan SHAとpatch時のdisabled fingerprintを記録し、clean reloadにも同じplanを引き継ぐ。
SYSTEM/LOG3fileの実hash/bytesをplanと照合する。監査は対象7値＋関連Select/DeviceInstall/RpcSsのまま。
WinPE側の旧disklog/marker保存と再利用拒否は媒体READMEを参照。

## GPU専用DeviceInstall：実fingerprint取得と限定再arm

`-InspectFixedGpuDeviceInstall -ServiceName MT8167DeviceInstall` はStart4の正確な既存7値を
workcopyのRegLoadKey/replay/Unloadで照会し、clean copy再loadでraw fingerprint一致を確認する。
原SYSTEM/LOG1/LOG2の完全SHA256 pin3個を必須とし、サービス値を書かない。
成功時はverified-disabled-deviceinstall-gpu-planとgpu-rearm-plan.json/そのSHAを出力する。
この照会modeのwork/verify SYSTEMはdeploy候補として発行しない。

続く `-RearmFixedGpuDeviceInstall -GpuRearmPlanPath ... -ExpectedGpuRearmPlanSha256 ...` は
同じfresh input3fileと実fingerprintを照合してStart4→2だけを許す。
ServiceName/IncludeHardwareProbe/Engine rearmの混用は拒否する。
Engine既存分岐の挙動・固定fingerprint・限定監査は維持する。詳細はGPU README-provider-update.md。
