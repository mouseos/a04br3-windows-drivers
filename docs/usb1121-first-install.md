# ABI3親1121/HCD1019の初回導入

GPUのrun197/render-only profileとは独立したUSB専用の導入契約。
基点はfresh196、parent1120、legacy keyboard1010である。
既署名1121/1019 tupleのdriver本体・HWID・版・ABIを変更しない。
同じリポジトリのbranchで、準備・stage・DIF・親再列挙・退役・復旧を追加する。

## HWIDと所有権

- 旧keyboard: `MT8167\USB20`、HID class、`mshidkmdf`と`A04Usb20Keyboard`。
- 新HCD: `MT8167\USBPORT20`、USB class、`MT8167MUSBHcd`。
- 実parent: `ACPI\BEN0001`、`Mt8167Shared`。

`wdf_parent_probe.c`の`ParentAddProbeChild`は、USBPORT child構成でUSB機能だけを
新HWIDへ変える。`MusbRoleWork`はID/detect/source/OCの既存観測と旧leaseの空きを
確認して実親配下にPDOを作り、role離脱時にMarkMissingする。
1121のrecipeは`A04_SHARED_USBPORT_CHILD=1`、HCD INFはUSBPORT20と一致する。
従って両packageを先にDriverStoreへstageすれば、後でrole条件が成立した際にも
通常PnPでHCDを自動選択できる。root devnodeは作成しない。

旧HIDの停止はそのdevnodeに対するDIF_PROPERTYCHANGE/DICS_DISABLEだけで行う。
共有serviceの`mshidkmdf`全体や他のHIDを止めない。
旧keyboardのD0Exit/ReleaseHardwareはpoll停止、controller停止、親interface解放へ
進む。新親のrole workも`usb.leased`を検査するため、旧leaseを強制的に消さない。
導入recipeはGPIO、MMIO、強制host、SetVbus/source ONの操作を一切呼ばない。

## mainが後で実行する一括経路

1. ホスト上で`tools/ci/prepare-usb1121-first-install.py --workspace <root> --out <新規bundle>`。
   fresh196の完全SHAとboot ID、native receiptの完全SHA、実parent/keyboardの選択版、
   実SYSとrollback packageの一致を検証する。既署名候補6leaf、復旧元6leaf、recipeを固定コピーする。
2. 既存Windows build環境で`tools/ci/build-usb1121-pnp-helper.ps1`を実行する。
   `-Csc`、`-SignTool`、checkout内の新規`-OutputDirectory`、`-BuildAndSign`を指定する。
   ARM managed helperだけをcompile・BCBF署名する。driver/SDKは再buildしない。
   `BuildAndSign`なしの出力は計画だけであり、導入用helperではない。
3. bundleと署名helper/manifestをmainの既存配布経路で対象Windowsへ渡す。
   `recipe/install-usb1121-first.ps1`へ`-Mode Apply`、`-BundleDirectory`、
   `-ExpectedPlanSha256`、`-ToolManifest`、`-ExpectedToolManifestSha256`、
   新規`-TransactionDirectory`を渡す。各SHAはホストで確定した実ファイルの値を使う。
4. Applyは対象Windows上の実USB tupleを再確認し、全candidate/rollbackをtransaction配下に
   固定してからHCDと親を双方stageする。どちらかのstageが失敗すればDIFへ進まない。
5. 旧keyboard PDOをPnP停止し、DN_STARTEDが消えたことを確認する。
   親のdriverを固定INF/versionでDIF_INSTALLDEVICE選択し、親PnP再起動と同期再列挙を行う。
   親にはDI_DONOTCALLCONFIGMGを指定して選択と再列挙の順序を分ける。
6. 実親配下のUSBPORT20だけを探す。通常PnPの自動bindを優先し、既に現れた実PDOに限り
   固定INF/versionでDIFを補完する。新旧同時開始、別service/class/version、旧filter継承は拒否する。

Windowsが再起動を要求したら、そのphaseをjournalへ残してmainへ返す。
mainが通常再起動を行った後、同じ引数と`-Mode Resume`で続ける。
WMIのLastBootUpTimeでbootを区別し、同一bootのResumeはPnP変更へ進まない。
この値が取得不能なら変更前に停止する。親のDICS_PROPCHANGEは現在のhardware profileを指定する。
PC cable等でUSBPORT20が存在しない場合は`await-host-role-or-parent-restart-not-hardware-success`。
架空PDO作成やrole強制で先へ進めない。自然なrole変化で出現したPDOにもstage済みINFが使われる。
Resumeは既完了stage/DIFを繰り返さず、実状態を再確認する。

## 復旧と証拠

各変更の前にintent、後に完了をatomic journalへ保存する。新しいtransactionだけをApplyで
開始し、既存transactionは同じplan/tool hashのResume/Rollbackに限定する。
失敗時に新packageを無条件削除したり、原SYSTEMを書き戻したりしない。

`-Mode Rollback`は実HCDを先にPnP停止し、保持済みの旧parent1120/keyboard1010 packageを
stageする。旧親を選択・再列挙した後に、旧keyboard PDOだけを再enableする。
再起動要求やPDO欠落なら復旧原本とjournalを保持して返す。
復旧開始後はRollbackだけで再開し、Resumeで候補導入へ戻らない。
候補をstageしただけのDriverStore cacheは削除しない。
停止不能・PnP vetoの場合は破壊的な強制操作へ進まない。

Windows RTでのABIはWDK8.1 `SetupAPI.h`のARM32 byte packingに合わせる。
helperはIntPtr=4と構造体サイズ28/556/1564/20を検査してからPnP APIへ進む。
新規PnP helperはmainでのARM build・署名・実Windows実行が必要。
このbranch作成時にはWindows/runner/実機へアクセスしていない。
Linuxではbundleの実byte照合、C#ホストcompile、pure状態判定、PowerShell parserだけを検証する。

HCDの選択とDN_STARTED成功、USB転送成功、給電改善は別の到達点である。
最終状態にも`power-delivery-unverified`を残し、給電改善や一般USB機能の成功は主張しない。
