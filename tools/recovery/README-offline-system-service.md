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

既定で対象はMT8167DeviceInstallだけ。将来の任意 `-IncludeHardwareProbe` は同じ値型・依存を持つMT8167HardwareProbeを追加するが、最初のGPU復旧試験では指定しない。

## 処理と正確なサービス値

1. 原本3ファイルを新規work directoryへ名前を保って複製する。transaction logsはSYSTEMと同じdirectoryに置く。入力のformat 1.5とbase-block checksumを確認する。dirty sequenceは許容する。
2. 子プロセス内でSeBackupPrivilege/SeRestorePrivilegeを有効化し、一意なHKLMのprivate aliasへRegLoadKeyWでloadする。Windows native APIによるreplayを使い、Pythonでlogを合成したりheaderの版・sequence/checksumを手修正したりしない。
3. replay後のSelect.Current=1を確認する。次回起動先の食い違いを避けるためSelect.Defaultも同じ1でなければ拒否する。両Select値自体は変更しない。
4. ControlSet001/ServicesのDeviceInstall(Type=32, Start=3)とRpcSs(Type=32, Start=2)の実在・DWORD型を検査する。PlugPlayは追加しない。
5. ControlSet001/Services/MT8167DeviceInstallを下表どおり登録する。未登録または互換性のある部分登録だけを許可し、既存Start=4等を勝手に再武装しない。
6. registry handlesを閉じ、RegUnLoadKeyWを成功させる。process tokenの元のprivilege状態も復元する。
7. 保存SYSTEMのregf、major=1/minor=5、primary/direct format、checksum、primary/secondary sequence一致を検査する。hostがformatを更新した場合は拒否し、版の書換えで取り繕わない。
8. clean SYSTEMだけを別verify directoryへ複製し、LOG無しで再loadしてサービス値と無関係なregistry内容を確認、unloadする。原本とcandidateの不変・コピー一致を確認する。

| 値 | 型 | 内容 |
| --- | --- | --- |
| Type | REG_DWORD | 16 |
| Start | REG_DWORD | 2 |
| ErrorControl | REG_DWORD | 1 |
| ImagePath | REG_EXPAND_SZ | `"C:\Windows\System32\MT8167DeviceInstall.exe"`（引用符を含む） |
| ObjectName | REG_SZ | LocalSystem |
| DisplayName | REG_SZ | MediaTek MT8167 Device Installation |
| DependOnService | REG_MULTI_SZ | DeviceInstall、RpcSsの2要素 |

サービス登録前後で、対象サービスkeyと上記7値以外のraw registry値（型・長さ・SHA-256）とkey集合のdigestが一致することを確認する。比較基点はnative replay後なので、log replayによる正当な更新とサービス登録を区別する。registry linkは辿って変更しない。無関係なdriver registry値をまとめて書き換えたり、driverバイナリを更新したりしない。

## timeoutと配備可否

native APIを呼ぶ子プロセス全体を既定30秒で監督する。timeout時は子を終了要求し、別の5秒上限のcleanup子プロセスで同じ一意aliasのunloadを試みる。親から無期限のRegLoad/UnLoadは呼ばない。停止したkernel callの即時終了やVM全体の復旧までは保証できず、解除確認不能時は配備不可。timeoutしたworkcopyを使用しない。

成功条件はscript正常終了とprivate `result.json` の `status=verified-clean-system-services-only`。配備候補は **candidate/SYSTEMだけ**。resultには入力3ファイルhash/長さ、入力と出力の版/checksum/sequence、native API戻り値、30秒完了有無、正確なサービス値、無関係なregistry内容のdigest、candidate hash、recipe/native-source hashを保存する。

work/verify directoryや、その処理中に生成されたLOGを配備しない。入力の古いLOGをcandidateに添えてreplayさせない。EXE/INF/SYS/CAT配備、attempted.bin不存在の確認、private WinPE媒体によるclean SYSTEMコピーとvolume releaseは主担当で行う。依存サービスの存在はdevnode列挙完了の保証ではなく、実bindは通常Windowsで確認する。

このレシピ・C# wrapperは公開可能だが、実SYSTEM/LOG・candidate・private resultを公開repoやCI公開artifactへ含めない。端末hiveを含む私用媒体の作成・配備は主担当の明示した私用運用として区別する。本レシピ自体のnative Windows実行・実機起動は未実施。
