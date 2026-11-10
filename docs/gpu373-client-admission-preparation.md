# run216/fresh215/post217: kernel371保持・client373＋再結合launcher

基点は公開680b8d09のcontroller/profile。373はclientの論理versionで、公開ソース5653962fb84a43ad3e27b4cbf1ca6aeac454cd77のdxgi-present356.cがCLIENT_VERSION373を出力する。実filenameはmt8167-dxgi-present356.exeを維持。launcherもmt8167-dxgi-console-launch363.exeのfilenameを維持してevaluator hashを再結合する。372署名品は保持するだけで配備しない。

追加source `tools/recovery/gpu373_client_admission.py` は、未来のprofile全複製ではなく小さいtransition admissionとruntime validator。実fresh215/同Recovery補足/新215不在の実native、全実file hash、実署名Valid/BCBFと実RT8.1 import proofを要求する。client hashは固定値を捏造せず、後日到着する署名manifest/実byteからのみ得る。launcher manifestのevaluator_sha256が新client実hashと一致し、Session1/console-system-token/GPU_calls0/UserTokenSubstitution falseを維持する。DWM372の暗黙選択とkernel/INF/CAT変更を拒否。clientはINF CopyFiles外なので、その更新をrender DIFへ流用しない。

| mode | DIF/stage mask | SCM経路 | 追加Windows restart | 保持対象 |
|---|---|---|---|---|
| client373＋launcherのみ | 0 | A04_DIAGNOSTIC_ONLY=1で既transaction(1)を直接使う | 0 | 全13package、render371/transport369/WiFi1029、boot98de/GPT |
| 明示WiFi更新併用 | wifi bit3=8のみ | 既通常phase0→bounded restart→phase1 | 1 | render/transport/その他12package、boot98de/GPT |

WiFi1030は署名tuple/正式row到着後に明示refを渡す分岐。既署名package/CAT validatorそのものを通し、completed:true等の別manifestを要求しない。未知hash/versionを仮代入しない。base validatorが未承認の新tupleを拒否する場合、その正式署名schemaに基づく限定admission更新を別途行い、任意packageを許可しない。clientだけの計画をWiFi hash待ちのために作り直す必要はない。

680をそのまま新clientへ適用できない具体的箇所と次の差分:
- prepare-peripheral-followup200.py: strict363/356hashとfresh同byte要求をtransition hookへ。client候補だけpreserved_filesへ新hash、baseline_witnessesへ旧capture hashを別保存し、WinPE pre-copyとWindows runtime guardを混同しない。
- 同loopの`elif role=='render'`はTIMING/新variant時に旧368へ戻さない分岐にする。全13rowsはactual215 selected/SYSと署名出自のまま保持。kernel371manifestを373へ改名しない。
- prepare-peripheral-followup184.py emitter: install_targetsからmask0/8を生成し、client-only時DIAGNOSTIC_ONLY=1。mask0でも通常phase0のままだとmt8167-peripheral-update.c末尾で3010になるため、mask0だけの変更では追加restartを除去できない。generic driver Cへ成功stubを足さず、既診断専用分岐を使う。
- build-combined200-service.ps1: fresh215/client-profileの明示modeを追加しSCM1TUだけcompile/link/sign。新sourceとservice/native outputはWorkspace内かつcheckout-local。LF/CRLFはcanonicalJSON/正規化textで比較し署名済みfileを編集しない。
- prepare-offline-system-service.ps1: MT8167CombinedFollowup215だけ新規7値、actual service/value集合を実nativeから採用。client-only reboots0かWiFiあり1を明示分岐。旧service208/212等のrearmなし。
- media/admission/controller/collector: 新pair署名・import・launcher再結合hashを同じtransitionで検証。client-onlyはdriver/INF/CATコピー先・DIF・boot/GPT操作0。runtime用package payloadの既read-only hash/CAT確認まで削除しない。最終transaction/run216/fresh215/post217、terminal Windows hold、run/PID/flush、collect-only gateを維持。mask0のadmission/reboot期待も一致させる。

この段階では上記guardを含む新transition validatorとcapture/read closureを公開した。既680の全入口を新profileへ開放した最終generator/media admissionではない。署名373 pair・optional1030 rowとmain最終source/map固定前にSCM/header/媒体を生成しない。未到着署名schemaの推測による全profile複製を避けるため、最終入口接続は確定入力を受領した次工程で行う。

同Recovery補足collector:

```sh
python3 -B tools/recovery/capture-peripheral-preservation.py --manifest "$ACTUAL215_MANIFEST" --manifest-sha256 "$ACTUAL215_SHA" --out "$NEW_SAME_RECOVERY_SUPPLEMENT" --ntfs3g-remote /tmp/rt-ntfs3g --gpu-runtime-assets --include-system-dxgi
```

既3+2静的assets/128logs/64MiB境界を保持し固定System32/dxgi.dllだけoptional追加。ro,norecover論理file copy/hashで、欠落はmissing_filesへ明示する。source/disk/loader実体の物理readやdxgiの実loadを主張しない。client admissionは実dxgi copyが欠けた場合に拒否する。collector flag省略時のclosureは従来どおり。

同load inventoryは既entryへ`-TimingCaptureNumber 215 -IncludeWifiCheckpoint -IncludeGraphicsTopologyCache -IncludeDisplayDeviceBindings`。Enum/DISPLAYとControl/Videoは全64keys/depth4、固定strings/DWORDだけ。stringはmax4096B・環境展開なし、EDIDblob禁止。Control/Video registry linkは既helper拒否を維持し追跡しない。partialを明示し、cacheやmonitorの存在だけでlive ownerを断定しない。source read-only observer以外にdriver変更はない。

実施済み: collector optional仕様のhost test、transition mode/reject host tests、public source、許可されたactual215一回native読取り。未実施: SCM215 compile/sign、native service215 patch、client373/WiFi1030/DWM372配備、run216、未到着署名hashによるfinalize。架空fresh/receiptは作成しない。actual215の実結果はrootのport/xnu/out/windows-rt-native215へ保存し、source repoへ混ぜない。
