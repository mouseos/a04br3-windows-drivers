# USB1121: 実freshとGPU369最終packageへの最小再結合

`5c39277d`のSCM C修正（公式inbox PS exact SHA + 通常CreateProcess CI）を使用する。
既署名component84fab8b6...は旧fresh198/GPU367 pinを持つ保存物で、新baseline用としない。
既存の二つのPython生成器と1TU署名recipeへ再結合入口を追加した。新frameworkやnative生成器は追加しない。

## mainが固定する入力

- 実取得fresh manifestとそのSHA256。status=captured-unmounted、read_only/unmounted、errors空、SYSTEM trio実hash一致。
- 同じfresh SHAをsource_manifest_sha256に持つ実native inventory receiptとそのSHA256。原本不変、全旧Services保存、POST0、parent1120/USBkeyboard1010選択を要求する。旧197/198receiptの改名では通らない。
- main/Averroesの最終GPU369 policyとそのSHA256。draft=false、normal_install_allowed=true、fresh SHA/Recovery boot一致。pending/templateは不可。
- 本物のGPU369署名manifestとそのSHA256。signed/success=true、BCBF signer、outputs全leaf実hashを照合。最終policyのrender/display trioとrender extra_membersをこの実manifestへ突き合わせる。

GPU policyはUSB出力を含める前にmainが固定し、そのbyteを変更せず共通media sealからUSB出力と並べて参照する。USB生成後に同policyへUSB hashを書き足して相互hashを循環させない。

最終freshやpolicyをこのUSB作業が捏造・仮生成しない。main経由で固定された実入力のみを渡す。取得済みfreshのGPUがまだ368でも、元のcapture/receiptの出自はそのまま保持し、実行時retained GPUはこれから同媒体で適用する最終369packageの実byteから別途設定する。

## Linuxホスト: 最終入力確定後に一度生成

以下の変数はmainのseal対象となる実パス/hash。例示値を未来のfreshとして使用しない。

```sh
python3 -B "$repo/tools/ci/prepare-usb1121-service198.py" \
  --workspace "$root" --out "$root/port/xnu/out/usb1121-final-inputs" \
  --fresh-manifest "$FRESH_MANIFEST" --fresh-sha256 "$FRESH_SHA256" \
  --native-receipt "$NATIVE_RECEIPT" --native-sha256 "$NATIVE_SHA256" \
  --final-policy "$GPU369_FINAL_POLICY" --final-policy-sha256 "$GPU369_FINAL_POLICY_SHA256" \
  --gpu-manifest "$GPU369_SIGNED_MANIFEST" --gpu-manifest-sha256 "$GPU369_SIGNED_MANIFEST_SHA256"
sha256sum "$root/port/xnu/out/usb1121-final-inputs/input-closure.json"
```

出力は既存bundle/helper、BOM付きlauncher、新profile、header、input-closureと実fresh SYSTEM trioコピー。新しいnative hiveは生成しない。
元freshファイルは全て再hash。新profileは実fresh/nativeと最終policy/GPU manifestのSHAを保持し、C headerへlauncher/profile両hashを固定する。旧schemaは履歴入口として残し、再結合入口はREBOUND_SERVICE_V2を使用する。

## 保持Windows runner: 固定入力後の1TUだけ

公開固定sourceのclean checkoutをC:/mt6630-*の独立dirへ配置し、生成済みinputsをその外の専用dirへ転送する。SDK/cache/machine環境を変更しない。BaselineManifestは既回収719 manifest SHA6b8bc000...であり、用途は既import libs/ABI・service名の由来確認だけ。新fresh/GPU保持条件はInputsから取り、旧native SYSTEMを再利用しない。

```powershell
& "$src\tools\ci\build-usb1121-ps-image-component.ps1" `
  -Inputs $FinalInputs -ExpectedInputClosureSha256 $FinalInputClosureSha256 `
  -BaselineManifest $Saved719Manifest `
  -OutputDirectory "$src\build\usb1121-final-pinned"
```

1TU compile/link、既helper署名とcandidate/rollback CAT membership検証、BCBF EXE署名・実verify、全payload hashをcomponent-manifest.jsonへ記録する。driver/helper/GPU/SDK再buildは0。最終入力前の仮署名は行わない。署名後はZIP/全leaf/ARMNT/importsを回収検証する。`runtime_source_commit=5c392...`はC/既UART headersの由来で、更新launcherはrecipe source commitと入力closure内の実hashで別に追跡する。

## 同一GPU媒体・phaseへの部品受渡し

- `payload/MT8167Usb1121Install198.exe` → `Windows/System32/MT8167Usb1121Install198.exe`
- payload内の他ファイル → `Windows/System32/MT8167Usb1121Install198/`配下。同名EXEをデータdirへ二重配置しない。
- 旧ready7ec/native SYSTEMや旧fresh198 profileを媒体へ混ぜない。主GPU更新のnative SYSTEM/サービス登録処理はmain側のまま。
- 既service登録名/実行pathは維持。GPU更新前にUSB serviceを自動起動させず、mainの既combined phaseから最終369SYS/transport/UMD/FWの実byte成立後に起動する。profileのintegration_phaseは`after-gpu369-before-final-flush`。
- 主処理が既サービスMT8167Usb1121Install198の登録値を確認し、disabledなら同じphase内でdemandへ変更してstartする。新しい自動startや専用nativeを追加しない。サービスは既実装で完了/失敗時にself-disableする。旧transaction/予定reboot状態を新候補へ継承しない。既ログはmainのcaptureへ保存し、新payloadのlogs/transactionは空から開始する。
- U198 prefix/profile198は既protocol identityでありcapture番号ではない。run/PIDとSTART/result/SCM_STOP/FLUSH/TERMINALの厳密gateを維持。profile世代をfresh番号で誤認しない。
- mainは既controller観測経路でUSB完了を待ち、主GPU側と合わせた最終safe flush/terminal後だけRecoveryへ進める。PowerShell子のexit0だけでcollectしない。
- PnPの実3010時だけ既bounded予定reboot/resumeを使用する。probe用の追加rebootは不要だが、実PnPの要求まで0に偽装しない。親/HCDの停止証明、rollback、外部VBUS/OC保護、source_ON=false、force_host=falseは変更しない。

実機はmain専有・WiFi確認のためhold。本変更でPS/PnP成功、host列挙、給電改善を主張しない。GPU369同時phaseでの実適用結果と給電実測は未検証。

mainの既combined実行部に渡す起動コマンドは次の二つ。実行前に既serviceのImagePathが上記固定EXEでLocalSystem/own-process、最終GPU retained byte成立、旧transaction/log退避と空logs directoryを主処理が確認する。これは端末で今実行する指示ではない。

```powershell
& sc.exe config MT8167Usb1121Install198 start= demand
if($LASTEXITCODE){throw 'USB service demand activation failed'}
& sc.exe start MT8167Usb1121Install198
if($LASTEXITCODE){throw 'USB service start failed'}
```

SCM start成功はPS/PnP/給電成功ではない。mainはU198の新run/PIDに結合した既停止/flush/terminal gateまで観測する。既GPU policy本体は再hashが変わる改変をせず、共通sealがGPUとUSBの両manifestを固定する。
