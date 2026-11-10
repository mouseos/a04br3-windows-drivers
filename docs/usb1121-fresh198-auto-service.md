# MediaTek MT8167 USB1121 / fresh198自動導入recipe

このrecipeはmain専用。VM・媒体書込み・実機実行はmainが行う。
GPU367を保持し、親1121/HCD1019の通常Windows導入だけを行う。
元90eのinstaller/C#と署名済みDLLは変更・再buildしない。

## 固定入力

- Recovery fresh198 manifest: `4cfe2edfded6ff752ff145b46c33af910b30370e99079f9c75704cf17e2467a3`
- native197 receipt: `2417cc9699e071cc0e576371e4e4dc74121904efb8fa39a077b21c40339db2e2`
- signed helper ZIP: `e37b36fc550b170b3d233cd7585121f72df309f7e29f7325cd67c2566829e6b1`
- helper manifest: `656ab72fda2cd79518aa47ddabdcd008db86673c675e83ccbb7875073b501064`
- DLL: `b0c3585525ab406218e03eaa06a3e453db4c92796898baa72bd7bc54786474ba`
- 固定PnP source: `90e4132b7d7e62e9629d1bd768e5c09ad063c517`
- helper build recipe: `1a753dccd604641e767e03748418c8a5c5c9d352`
- 親1121/HCD1019既署名manifest: `15433a4037b4d739f9525d87967f5782e6576c337ac4c2d6d0ada69d6deeb55f`

新しい導入planはfresh198とnative197 receiptだけに結合する。
helper-manifest内の旧plan hashは既署名DLLのbuild出自であり、今回の導入planではない。
`windows-rt-combined197-fresh196/service/peripheral-policy.json`はGPU367 packageの
UMD/FW witness取得にだけ使う歴史的なパス。旧fresh196 manifest/SYSTEM/planは導入しない。
現在選択はparent1120/keyboard1010であり、既存HCD採用を仮定しない。

## 実行順序

root内で準備（ローカル・非実機）：

```sh
python3 -B tools/ci/prepare-usb1121-service198.py --workspace "$ROOT" --out "$INPUTS"
```

mainは専用のclean checkoutと既存SDK/import librariesを使う。
inputsはprivate SYSTEMを含むため公開・配布しない。新SCM serviceだけ1 TUをcompile/signし、
既署名DLLのtrust、candidate/rollbackのCAT署名・INF/SYS memberを検証する。

```powershell
& "$src\tools\ci\build-usb1121-service198.ps1" -Inputs $Inputs -ExpectedInputClosureSha256 $ClosureSha -OutputDirectory "$src\out\usb1121-service198" -KitHeaderRoot $ExistingKitHeaders -LibraryRoot $ExistingArmLibraries -SignTool $ExistingSignTool
```

同recipeはfresh SYSTEM trioのcloneだけをnative APIでloadし、新serviceをStart=2で追加する。
ControlSet001・POST0を確認し、全既存Servicesの再帰fingerprintとGraphicsDrivers値を
追加前後および再load後に比較する。既存serviceを再arm/disableせず、元trioを再hashする。
出力は`usb1121-service198-signed.zip`。DLL/driver/SDK rebuild=0。

mainが署名済み出力を回収しmanifest SHAを固定した後、root内で媒体を生成する：

```sh
python3 -B tools/winpe/build-usb1121-fresh198-media.py --workspace "$ROOT" --inputs "$INPUTS" --signed-package "$SIGNED" --expected-service-manifest-sha256 "$SIGNED_MANIFEST_SHA" --base "$ROOT/port/xnu/out/windows-rt-cpu-usb-audit-scope-v2-mapfix" --tools "$ROOT/port/xnu/out/windows-rt-tools" --out "$MEDIA" --reboot-after-commit
```

既存guard/map/release/UARTと署名済みcompare-filesを使う。fresh198のconfig全原本、
保持driver/EXE/UMD/FW/旧transportを比較してから専用payloadを配置し、SYSTEMを最後にcopy/比較する。
既存ファイルへ上書きするのは準備済みSYSTEMだけ。boot/GPT変更0、offline DIF/DISMなし。
失敗時の自動rollbackやboot変更はしない。新媒体はprivateであり、一般配布物ではない。

## 通常Windowsでの動作

新service `MT8167Usb1121Install198`は、既存peripheral serviceの
SCM→CreateProcess client方式で固定pathのPowerShellを起動する。
PowerShell本体のWindows trust、固定client/profileのSHAを確認し、clientが全payloadを再照合する。
GPU367/UMD/FW/旧transportとPOST0も実行前に確認する。

90e installerを初回Apply、journal既存ならResumeで呼ぶ。
両package stage→旧provider停止証明→親DIF/再起動/再列挙→親が作った実PDOだけのbindを維持する。
StageOnlyは実DIFや実host列挙の証明にならない。
installerがreboot_requiredを記録した場合だけ通常Windows再起動を要求し、次bootでResumeする。
端末に必要な再起動がある場合、旧service Start設定は保持されるため既存observerもその設定に従う。

terminalまたは失敗時は新serviceだけをdisableする。host PDO待ちもterminal記録で停止し、
強制host/source ON/root PDOは作らない。通常PC接続のままhost給電を要求しない。
停止・電源制御・外部VBUS/OC保護は元90eとdriver実装から緩和しない。

## 検証の区別

ローカルではPython入力生成・hash/CRC・source同一性・PE/CLR構造を確認。
Windows parser、新SCM TUのcompile/link/sign、native SYSTEM patch、生成媒体実行、
ARM PowerShell/CLR loader、PnP、host列挙、給電改善はmainの実行待ち。
helper既署名と新SCM実行可能性・実機成果を混同しない。

## 署名後のnative失敗からの単独再開

mainの初回Windows実行ではservice compile/sign、DLL trust、candidate/rollback CAT membershipは成功。
その後OpenPath(3 args)でAccess denied。旧helperはwritable時に各祖先へKEY_READ|KEY_WRITE、
read時もREAD_CONTROLを含むKEY_READをRegOpenKeyExへ渡す。
SeBackup/SeRestorePrivilegeを有効にするだけではRegOpenKeyExのDACL検査を回避できない。
従来の限定service snapshotより広い全Services再帰に保護キーが含まれる可能性があり、
旧logだけでは失敗した具体的キーは未特定。BTHPORTと断定しない。

新`Usb1121OfflineHiveNative.cs`は専用namespaceとprivate clone root handle・path allowlistを使い、
親で列挙済みの既存の直下キーだけをREG_OPTION_BACKUP_RESTOREで開く。
OPEN_LINKとSymbolicLinkValue検査を維持し、missing key作成を成功と扱わない。
ACL/owner変更・保護キー省略はしない。失敗時は`native/failure-receipt.json`に
phase/key_path/messageを記録する。cleanup失敗も別の小receiptへ残して停止する。
新service以外の全旧Services/GraphicsDrivers fingerprint比較を維持する。

同じ専用checkoutを新recipe HEADへ更新したmainが以下だけを実行する。
`$out`は既存署名出力、`$ExpectedExeHash`は実行前の保持EXEの実SHAをmainが固定した値。

```powershell
$out='C:\mt8167-usb1121-fresh198\source\out\usb1121-service198'
$ExpectedExeHash=(Get-FileHash "$out\payload\MT8167Usb1121Install198.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
& "$src\tools\ci\resume-usb1121-service198-native.ps1" -Inputs 'C:\mt8167-usb1121-fresh198\inputs' -OutputDirectory $out -ExpectedSignedExeSha256 $ExpectedExeHash -SignTool $ExistingSignTool
```

途中`native`は`native-failed-<GUID>`へ退避して保持し、immutable fresh198 trioからcloneを再生成。
保持EXE/objectとruntime sourceを元9b3fa5d4に結合し、再開前後のbyteを照合する。
compile/link/sign呼出し0。新しいnative helperのAdd-Typeはhost専用で、実機用署名DLL再buildではない。
manifestのsource_commitは元署名binaryの9b3fa5d4、recipe_source_commitは修正recipe HEAD。
初回失敗で保存されなかった未署名EXE hashはnullと明記し、再構築した値を実測に見せない。
旧sourceとの同一性と署名trustを再確認してからnative、manifest、ZIP作成を進める。
media builderもこのbinary/native recipe出自の分離を受け付ける。

修正recipeのWindows実行・Access denied解消・保全照合はmainの再実行待ち。
実機適用・host列挙・給電改善は引き続き未検証。

## UART terminal版（ready7ecを保持し、新service 1 TUだけ更新）

branch `work/usb1121-service198-uart-terminal`。
元ready7ec manifest `5447b112e3be16a7b6fd13c1733cc4411bab6b2eee3abaa6da5a4da3ed0f3d7b`、
SYSTEM `e9fcc62c3abf5464cb4fd3d2c0bd01c80172adc4bae198d8532090d695003b71`、
native result `0f6570cca2b78d39e7731b677aa24dde5c8269a021364d5caf9a2928aa49c023`を固定。
旧packageを変更せず、新規checkout-local outputへnative全leafをexact copyする。
service name/登録値とnative preparer/helper source bytesは同一。新native処理は実行しない。
新EXEのsource_commitはUART branch、native_preparation_source_commitは7ec02fc5として分離する。
input closure/profile/helper/driverは同じ。driver/helper/SDK rebuild=0、新serviceだけ1 TU compile/link/sign。

```powershell
& "$src\tools\ci\build-usb1121-service198.ps1" -Inputs 'C:\mt8167-usb1121-fresh198\inputs' -ExpectedInputClosureSha256 '09fcdfca60dac35fd3572a23d43c9de4100ab70e799ccdb9e1bd6bb7b2598955' -ReadyPackage 'C:\mt8167-usb1121-fresh198\source\out\usb1121-service198' -OutputDirectory "$src\out\usb1121-service198-uart" -KitHeaderRoot $ExistingKitHeaders -LibraryRoot $ExistingArmLibraries -SignTool $ExistingSignTool
```

媒体はこのUART版manifestを回収・mainがpin確定した後に一度だけ生成する。
新media builderはUART契約を持たない旧packageを拒否する。旧WIMを先にbuildする必要はない。
mediaのSYSTEMは上記e9fcc62cと同一。boot/GPT変更0。

### wire formatとcontrollerの判定

`tools/usb/usb1121-uart198-contract.json`が機械可読契約。
各recordはASCII、LF込み50 bytes。PBとは異なる固定prefix：

```text
U198 000000C6 RRRRRRRR PPPPPPPP EEEEEEEE CCCCCCCC\n
```

run/PID/event/codeは大文字hex各8桁。profileは198（hex C6）。
runは同bootのFILETIME/tick/PIDから生成。PIDは元SCM serviceのPIDであり、
後述observer processでも同じrun/PIDを引き継ぐ。hostは今回bootのSTARTからjoinし、
旧UARTや別run/PIDのeventを混ぜない。

| Event | 意味 | Code |
|---|---|---|
| 1 START | SCM登録・RUNNING/log準備後、installer開始前 | 0または開始準備error |
| 2 RUN_RESULT | PowerShell/90e installer process終了 | Win32/process exit code |
| 3 REBOOT_REQUIRED | installerが要求した再起動 | 3010 (`00000BC2`) |
| 4 SCM_STOP | observerが実SERVICE_STOPPEDを確認 | 0。非0は観測失敗、回収gate不成立 |
| 5 VOLUME_FLUSH | LocalSystem/C:\Windows確認後のC: FlushFileBuffers戻り値 | 0またはerror |
| 6 TERMINAL | SCM停止・log close・volume flush後、以後disk変更なし | 最終結果。3010なら予定再起動待ち |
| 7 REBOOT_REQUEST | source上限確認後の再起動要求API結果、またはbudget拒否 | 0またはerror |

通常順序は1→2→4→5→6。予定再起動は1→2(3010)→3(3010)→4(0)→5(0)→7(0)→6(3010)。
再起動API失敗は7(error)の後にnewservice disable・再flush(5)を行い、6(error)となる。
budget超過は4(0)→7(ERROR_RETRY)→5(0)→6(ERROR_RETRY)で、再起動を要求しない。

予定再起動は**最大4回**。旧provider停止、親bind、親restart、HCD bindの最大4境界を覆う。
`logs/planned-reboots.bin`でdeployment全体のattempt数を保持し、5回目はERROR_RETRY(1237)で停止。
counterを自動で削除・初期化しない。同候補の失敗後の再適用や任意retryを許可するものではない。

hostは同boot/run/PIDのSTART・RESULT・SCM_STOP(0)・最後のFLUSH(0)と実UART TERMINALを要求する。
TERMINAL=3010ではRESULT/REBOOT_REQUIRED=3010およびREBOOT_REQUEST=0も必須で、
予定Windows reboot/Resumeだけを待つ。Recovery回収へ進まない。
TERMINAL≠3010なら正常／失敗のどちらでも静止・flushの回収境界としてRecoveryへ進める。
STARTが非0でも、その後の停止/flush/terminalが成立すれば失敗ログ回収は可能。
code0自体はhost列挙・VBUS・USB給電改善の証明ではない。journalのphaseも回収・判定する。

terminalがない、flushが非0、SCM停止が未確認の場合はunknownとして証拠を保持する。
実行中timeoutを根拠にresetしない。observerのSCM待ちは30秒で観測失敗を返すが、
そのtimeoutはhostへresetを許可するeventではない。

### bounded UART・停止後observer

peripheral serviceの`uart_record`を再用（2秒BUSY上限、pending待機1000ms、cancel後250ms、
未完了OVERLAPPEDはquarantineしてprocess寿命まで保持、96-byte上限）。
C: volume flushも既peripheralのhelperをbyte同一で再用。lock/dismount/raw書込みはない。

SERVICE_STOPPEDをSetServiceStatusした後はSCM processが終了され得るため、
同署名EXEの`--uart-terminal RUN PID RESULT`を先に起動する。
元SCM callbackはlogを閉じ、最後にSERVICE_STOPPEDを報告して戻る。以後処理しない。
独立observerがQueryServiceStatusExで実STOPPEDを確認し、SCM_STOP record、log close、
volume flush、UART-only FLUSH/TERMINALを行う。terminal後のfile/registry変更はない。
予定再起動要求はflush後、30秒delayで行い、その結果と3010 terminalを先に通知する。

### 検証と未確認

host stubで実production formatter・停止observer・reboot上限関数をcompileし、固定50-byte形式と
正常／installer失敗／3010／SCM未停止／flush失敗／reboot API失敗／4回上限の7経路を確認。
これはWindows service自体のbuild、実UART、C: flush、予定再起動、実機PnPの証明ではない。
それらはmain実行待ち。ready7ec packageとnative proofは保持する。
