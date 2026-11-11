# USB1121: 実freshと最終GPU packageへの最小再結合

`5c39277d`のSCM C修正（公式inbox PS exact SHA + 通常CreateProcess CI）を使用する。
既署名component84fab8b6...は旧fresh198/GPU367 pinを持つ保存物で、新baseline用としない。
既存の二つのPython生成器と1TU署名recipeへ再結合入口を追加した。新frameworkやnative生成器は追加しない。

## mainが固定する入力

- 実取得fresh manifestとそのSHA256。status=captured-unmounted、read_only/unmounted、errors空、SYSTEM trio実hash一致。
- 同じfresh SHAをsource_manifest_sha256に持つ実native inventory receiptとそのSHA256。原本不変、全旧Services取得完了、POST0、parent1120/USBkeyboard1010選択を要求する。receiptの`full_service_preservation_equal=null`は比較未実施の原記録として保持し、`old_services`と最終policyの`service_preservation`を全件比較する。`captured_named_values`と`captured_registry_values`は集合・型・raw_hex・byte長を全件比較し、省略・重複・不一致を拒否する。旧197/198receiptの改名では通らない。
- main/Averroesの最終GPU policyとそのSHA256。draft=false、normal_install_allowed=true、fresh SHA/Recovery boot一致。pending/templateは不可。
- 本物の最終GPU署名manifestとそのSHA256。signed/success=true、BCBF signer、outputs全leaf実hashを照合。最終policyのrender/display trioとrender extra_membersをこの実manifestへ突き合わせる。renderのsource_commit/binary_source_commitと実manifestのsource_commitを照合し、実INFのDriverVerとpolicyのdriver_versionを一致させる。display transportは別の既署名manifest/directoryを維持してよく、そのsource_commit（manifestのnew_display_source_commitまたはsource_commit）・全leafが最終GPU manifestの同名leafのbyteと一致することを要求する。版数の許可リストは設けない。

GPU policyはUSB出力を含める前にmainが固定し、そのbyteを変更せず共通media sealからUSB出力と並べて参照する。USB生成後に同policyへUSB hashを書き足して相互hashを循環させない。

最終freshやpolicyをこのUSB作業が捏造・仮生成しない。main経由で固定された実入力のみを渡す。取得済みfreshのGPUがまだ368でも、元のcapture/receiptの出自はそのまま保持し、実行時retained GPUはこれから同媒体で適用する最終GPU packageの実byteから別途設定する。

## Linuxホスト: 最終入力確定後に一度生成

以下の変数はmainのseal対象となる実パス/hash。例示値を未来のfreshとして使用しない。

```sh
python3 -B "$repo/tools/ci/prepare-usb1121-service198.py" \
  --workspace "$root" --out "$root/port/xnu/out/usb1121-final-inputs" \
  --fresh-manifest "$FRESH_MANIFEST" --fresh-sha256 "$FRESH_SHA256" \
  --native-receipt "$NATIVE_RECEIPT" --native-sha256 "$NATIVE_SHA256" \
  --final-policy "$FINAL_GPU_POLICY" --final-policy-sha256 "$FINAL_GPU_POLICY_SHA256" \
  --gpu-manifest "$FINAL_GPU_SIGNED_MANIFEST" --gpu-manifest-sha256 "$FINAL_GPU_SIGNED_MANIFEST_SHA256"
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
- 既service登録名/実行pathは維持。GPU更新前にUSB serviceを自動起動させず、mainの既combined phaseから最終GPU SYS/transport/UMD/FWの実byte成立後に起動する。profileのintegration_phaseは`after-final-gpu-before-final-flush`。
- 主処理が既サービスMT8167Usb1121Install198の登録値を確認し、disabledなら同じphase内でdemandへ変更してstartする。新しい自動startや専用nativeを追加しない。サービスは既実装で完了/失敗時にself-disableする。旧transaction/予定reboot状態を新候補へ継承しない。既ログはmainのcaptureへ保存し、新payloadのlogs/transactionは空から開始する。
- U198 prefix/profile198は既protocol identityでありcapture番号ではない。run/PIDとSTART/result/SCM_STOP/FLUSH/TERMINALの厳密gateを維持。profile世代をfresh番号で誤認しない。
- mainは既controller観測経路でUSB完了を待ち、主GPU側と合わせた最終safe flush/terminal後だけRecoveryへ進める。PowerShell子のexit0だけでcollectしない。
- PnPの実3010時だけ既bounded予定reboot/resumeを使用する。probe用の追加rebootは不要だが、実PnPの要求まで0に偽装しない。親/HCDの停止証明、rollback、外部VBUS/OC保護、source_ON=false、force_host=falseは変更しない。

実機はmain専有・WiFi確認のためhold。本変更でPS/PnP成功、host列挙、給電改善を主張しない。最終GPU同時phaseでの実適用結果と給電実測は未検証。

mainの既combined実行部に渡す起動コマンドは次の二つ。実行前に既serviceのImagePathが上記固定EXEでLocalSystem/own-process、最終GPU retained byte成立、旧transaction/log退避と空logs directoryを主処理が確認する。これは端末で今実行する指示ではない。

```powershell
& sc.exe config MT8167Usb1121Install198 start= demand
if($LASTEXITCODE){throw 'USB service demand activation failed'}
& sc.exe start MT8167Usb1121Install198
if($LASTEXITCODE){throw 'USB service start failed'}
```

SCM start成功はPS/PnP/給電成功ではない。mainはU198の新run/PIDに結合した既停止/flush/terminal gateまで観測する。既GPU policy本体は再hashが変わる改変をせず、共通sealがGPUとUSBの両manifestを固定する。

## 実233のhost admissionと未確定入力の扱い

`19e5b625`からの本更新はgenerator、launcher、component recipeのみを変更し、
`5c39277d`のruntime C、署名driver/helper、公開ABI、EDK2共通部は変更しない。
USB service/profile/ファイル名の198とU198は固定identityであり、capture番号を意味しない。

GPU380最終packageはこの検証時点で未確定のため、仮manifest・仮署名・配備payloadを作らない。
mainの合図前は、6項目をpinした次の入口で取得原本と確定policyの比較だけを行える。

```sh
python3 -B "$repo/tools/ci/prepare-usb1121-service198.py" \
  --workspace "$root" --out "$root/port/xnu/out/usb1121-native-admission" \
  --native-admission-only \
  --fresh-manifest "$FRESH_MANIFEST" --fresh-sha256 "$FRESH_SHA256" \
  --native-receipt "$NATIVE_RECEIPT" --native-sha256 "$NATIVE_SHA256" \
  --final-policy "$FINAL_GPU_POLICY" --final-policy-sha256 "$FINAL_GPU_POLICY_SHA256"
```

この入口は`native-admission.json`のみを作成する。`payload`、profile、header、native hive、
配備用closureは生成しない。GPUの署名確認・現起動での鮮度・実機での保存成功は証明しない。
通常の配備入力生成は8項目すべて必須であり、未完成・不在のGPU manifestはbundle生成前に拒否する。
単独bundle generatorの再結合入口もfresh/native/final-policyの6項目を必須とする。

実233のhost 1バッチでは、原receiptの比較フラグをnullのまま保持し、55サービスの
root/subtree指紋と333個の名前付き値が実policyと全件一致した。
既存の実GPU379署名manifest（render 0.0.3.79、retained display 0.0.3.72）で
出自・leaf・INF版数の結合を検証した。これはGPU380の完成や実配備を意味しない。
欠落・重複・raw型/値/長さ・別fresh/boot・draft・source/leaf/version不一致・
最終pin欠落/380 manifest不在を含む33ケースが通過した。
テストはhost Pythonのみ。PowerShell実行、Windows build/sign、VM、実機、REAは未実施。
生ログと再実行用バッチは作業rootの
`port/gsi/logs/ios-feasibility/windows-rt-peripheral233-main/usb-current-profile-host1/`に置く。

生成後profile/closureには`target_gpu_version`、`final_gpu_source_commit`、最終policy/manifest hash、
比較結果を記録する。launcherとrecipeは`after-final-gpu-before-final-flush`を要求し、
従来の369固定phaseは使用しない。main側もこのphase名で結合する。
policyへUSBのhashを追記せず、共通media sealから独立したGPU policyとUSB closureを参照する。
親/HCD停止証明、rollback、source_ON=false、force_host=false、root PDOなし、
run/PIDに結合したSCM_STOP/FLUSH/TERMINAL gateは維持する。
最終入力生成・build/sign・実機検証はmainの合図と最終入力の固定後に実施する。

現在GPUを保持して既存disabled serviceを再有効化する専用経路は
[既存serviceの再有効化](usb1121-current-existing-rearm.md)を参照する。
GPU更新同時phaseとの実機併用は未検証であり、同じ媒体へ混在させない。
