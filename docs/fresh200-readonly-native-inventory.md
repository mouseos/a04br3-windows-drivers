# fresh200の循環しないread-only native入口

`tools/recovery/read-fresh200-native-inventory.ps1`はpost200 readerとは別の準備入口。capture contract/native receipt/完成GPU368 policyを入力に要求しない。既read196のbounded Values・node/selected class・ROOT BT hardware-ID列挙を分離し、旧policyは名前の包含下限としてだけ使用する。旧native197 receiptの改名・値の転写は行わない。

実入力はfresh200 manifest SHA `fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9`、Recovery boot ID `64657ae7-390c-431b-ae4d-ae5de209793b`、そのSYSTEM/SYSTEM.LOG1/SYSTEM.LOG2。実三本のbytes/SHAを検証し、新規OutputDirectory/cloneへコピーしてからRegLoadKeyする。原本はloadしない。全処理後にunload、privilege復元、原本三本・manifest・template SHA不変を確認する。

7ec02fc5d4030fe8cedb3c1e4fe135c5804c2869のexisting-key backup/restore openを `ReadonlyInventory200HiveNative.cs` に独立分離した。private cloneとしてこのclassがloadしたmount/rootのみを許可し、Select/ControlSet001内のServices/Enum/Class/GraphicsDriversの限定範囲だけを開く。子keyの実在列挙、REG_OPTION_BACKUP_RESTORE|OPEN_LINK、disposition=REG_OPENED_EXISTING_KEY、REG_LINK拒否を保持する。writable=trueのOpenPathは拒否し、registry value/key write APIを追加しない。既ACLを変更せず、保存hive原本やlive control setへfallbackしない。既存key openにRegCreateKeyExを使う7ec方式を継承するため、単にAPI名がRegOpenKeyExではないことと、snapshotを変更する操作とは区別する。private cloneへのhive replay自体はcloneを変更しうる。

収集する内容：

- NewServiceName=`MT8167CombinedFollowup200`の実不在。存在した場合は観測値を保存して失敗終了し、不在と偽らない。
- template旧18 serviceのfull root/subtree fingerprint、実Services namespaceにある追加CombinedFollowup/Usb1121 task service。必要な追加名は`-AdditionalServiceNames`で指定できる。旧fingerprintとの一致は要求せず、新しい実fingerprintを保存する。
- template125個のkey/nameを全件読取り。さらに実在する新しいretained task serviceのStart/ImagePath/Type/ObjectName/DependOnServiceを、存在するものだけ追加する。type/bytes/raw_hexはRegQueryValueExの実戻り値。必要な旧名が欠落すればreceiptへ記録して停止する。rawhex全量をstdoutへ出さない。
- semantic13 nodesに加えてUSBPORT20、ROOT MT6630COMBOを実列挙し、選択classのINF/date/version等を保存。parent1120か1121かはここで観測し、想定値を入れない。各driver serviceのroot情報は`actual_driver_services`へ分離し、full subtreeを比較したと偽らない。
- POST ownershipの実raw値、WDF timestamp、FactoryCheckpointの値とtimestamp、NativeCheckpoint、ModuleIdentityV1の原144byteと観測build/source。過去R40等を現在bootと決めつけず、same-boot correlationを別要件とする。module identityはlegacy literalでありcompile sourceとは別。

`full_service_preservation_equal=null`、`preservation_comparison_performed=false`であり、収集だけで保持比較の成功を主張しない。`policy_sha256`は旧templateのreferenceだけで、現在bootでGPU368が実行された証明ではない。capture contractはこの実receiptの後にsealする。

旧native workerの125固定関数は変更せず、fresh200専用 `Peripheral-CapturedValues200` を分離した。実receiptで増えた正確な名前集合/type/bytes/rawを2048件以内で比較する。post200 readerもこの専用関数を利用する。全profileの任意値を無条件許可する緩和ではない。

## mainが既Windows VMで実行するコマンド

新しい公開固定HEADのcheckoutから、管理者権限の新しいWindows PowerShellプロセスで1回実行する。既loaded CLR classを再利用しない。全private pathは同Workspace内とし、outputは新規directoryにする。原本のコピー先やhost成果物を実機へ配布しない。SDK/PCO/driver/serviceをbuildしない。Add-Typeの既CLR C# compile用一時物も新output/compiler-temp内に限定する。

```powershell
# $Workspace はmainの実private workspace。root相対配置を保ってupload済みとする。
$Fresh200 = "$Workspace/port/xnu/out/private-windows-rt-prebackup-run200"
$Template196 = "$Workspace/port/xnu/out/windows-rt-combined197-fresh196/service/peripheral-policy.json"
powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File tools/recovery/read-fresh200-native-inventory.ps1 -WorkspaceRoot "$Workspace" -Manifest "$Fresh200/manifest.json" -ExpectedManifestSha256 fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9 -SourceDirectory "$Fresh200/files/Windows/System32/config" -PolicyPath "$Template196" -ExpectedPolicyFileSha256 d3efa3d8c3a4b60efd7c44cddb267ef22cd5d388d7817e44796aec9f7d6c0b4f -OutputDirectory "$Workspace/native200-readonly"
if ($LASTEXITCODE) { throw 'Keep receipt/clone/log; do not finalize or arm' }
Compress-Archive -Path "$Workspace/native200-readonly/*" -DestinationPath "$Workspace/native200-readonly.zip"
Get-FileHash "$Workspace/native200-readonly.zip" -Algorithm SHA256
```

回収はZIP全体、特にreceipt.json、module-identity.bin、clone三本とsource_recipe_files。失敗時もJSON/clone/compiler-temp/host stdout stderrを保持する。receiptのerrors[]・load_rc/unload_rc/privilege_restore_error=0・originals_unchanged=true・new_service.present=falseをmainが独立確認してから、seal-gpu368-capture200.pyへ渡す。

bootは0dc155acのretained-chain対応をそのまま含む。既boot0943のsealed host artifact／最後の書込みcontroller／trial199 no-writeをSHA結合し、physical_read_this_capture=falseを保存する。新たなboot物理readは不要。UMD/FW/log inventoryは実同boot supplementを使い、未取得値を作らない。USB199の1790は別案件であり、この入口からPnP/service起動/trust変更をしない。

ローカルhostでは実manifestと三本のSHA、およびclone限定／writer拒否／旧125関数byte保持のソース契約を検査済み。Windows PS parser/CLR実compile/実hive loadは未実行で、mainの上記実行が必要。生成したreceiptをまだ存在すると扱わない。GPU368 trial・DWM成功は未確認。

## attempt1の実guard失敗と限定修正

実receiptは44 services取得後、named値5件で`Bounded named service value required`。load/unload/privilege復元は全0、原本不変。templateの先頭5件はservice146で、次5件が`SYSTEM\CurrentControlSet\Enum\ROOT\SYSTEM\0001`のService/ClassGUID/Driver/HardwareID/ConfigFlagsだった。Services限定guardがこの既BT leafを拒否した。

新readerとfresh200専用後段比較関数へ、この**1 leaf・5名前・型**だけを追加した。Service/ClassGUID/DriverはREG_SZ(1)、HardwareIDはREG_MULTI_SZ(7)、ConfigFlagsはREG_DWORD(4)。他のEnum leaf、子key、未知名、型違いは許可しない。C# open権限・clone限定・旧125関数・原本は変更しない。新しい実値を再読取りし、templateの旧raw値をコピーしない。

mainは既`C:/mt8167-native200-main`のinputをそのまま再利用できる。新公開source ZIPを`C:/mt8167-native200-source-attempt2.zip`へuploadし、**新しいsource展開先とoutput**で実行する。attempt1を消さない。外側PowerShellのNativeCommandErrorによって回収が飛ばされないよう、子のstdout/stderr取得中だけContinueにし、rc取得後はStopに戻して必ずarchiveする。

```powershell
$w='C:/mt8167-native200-main'
Expand-Archive C:/mt8167-native200-source-attempt2.zip "$w/source-attempt2"
$reader="$w/source-attempt2/tools/recovery/read-fresh200-native-inventory.ps1"
$t=$null;$parseErrors=$null
[void][Management.Automation.Language.Parser]::ParseFile($reader,[ref]$t,[ref]$parseErrors)
if ($parseErrors.Count) { $parseErrors | Format-List | Out-File "$w/parser-errors-attempt2.txt"; throw 'Keep parser errors; no clone execution' }
$oldPreference=$ErrorActionPreference
try {
 $ErrorActionPreference='Continue'
 & powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $reader -WorkspaceRoot $w -Manifest "$w/port/xnu/out/private-windows-rt-prebackup-run200/manifest.json" -ExpectedManifestSha256 fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9 -SourceDirectory "$w/port/xnu/out/private-windows-rt-prebackup-run200/files/Windows/System32/config" -PolicyPath "$w/port/xnu/out/windows-rt-combined197-fresh196/service/peripheral-policy.json" -ExpectedPolicyFileSha256 d3efa3d8c3a4b60efd7c44cddb267ef22cd5d388d7817e44796aec9f7d6c0b4f -OutputDirectory "$w/native200-readonly-attempt2" *> "$w/reader-host-attempt2.log"
 $rc=$LASTEXITCODE
} finally { $ErrorActionPreference=$oldPreference }
$items=@("$w/reader-host-attempt2.log")
if(Test-Path "$w/native200-readonly-attempt2"){$items+="$w/native200-readonly-attempt2"}
Compress-Archive -Path $items -DestinationPath C:/mt8167-native200-result-attempt2.zip
Get-FileHash C:/mt8167-native200-result-attempt2.zip -Algorithm SHA256
if($rc){throw "Reader exit $rc; collect the result ZIP and inspect actual receipt"}
```

host regressionは実attempt1 receiptと実template125件の名前/型を使って、既5件の失敗位置、新BT5件の限定受理、近隣leaf/子key/未知名/型違い拒否を確認する。Windows再実行成功はmainの実receiptが届くまで未確認。
