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
