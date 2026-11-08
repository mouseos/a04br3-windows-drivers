# Resident Copy runtime340（実機未検証）

基点339/338正常copyを置き換えず、同じTQ protocolの準備・再armを分離する。
GPU143reset原因を確定した変更ではない。339証拠候補と338採用artifactは不変。

Linux一次source: CT3_GPL_LGPL_Src/kernel-4.4/drivers/misc/mediatek/gpu/gpu_rgx/m1.8ED4333936/services/server/devices/rgx/rgxtransfer.c
CreateTransferContextKM:270、Destroy:399、Submit:465、InitCmdCCB:783、Acquire:816、Release:854、Schedule:896。
作成/破棄と反復submitが分離され、CCB全group予約/UFO/jobrefと既contextへのKICKを更新する根拠。
このLinux版はm1.8でありWindows側m1.11のバイナリ/構造と同一とは主張しない。
Windows packetは既存m1.11型/既署名337TQ producerを保持する。

初回boot前にresident RT/stagingの正規PFN/PTEを検証、USC/PDS/descriptorをcold作成する。
prepare_copyは前job完了後、同FW root/epochを保持し、空のliveCCB/同期UFO一致を検証、
新jobrefで完全TQ+UPDATE groupを再生成しPASSIVEでCPU→GPU cache ownershipを戻す。
submitは実Dxgk IRQ lockでepoch/job/generation/IRQ gateを照合。実ISR receipt→TQ/UFO→SLC/GP→
CPUcache返却→fresh GP IRQの既完了連鎖を保持し、fresh IRQまでcommand_busyを維持する。
phase7のみ再利用可、phase1二重arm/実行中/曖昧phase/別epochは拒否。
障害時の参照は正規Stop/TDR DMA-off/IRQ-close/VerifyOff証明まで保持する。

copy資産がresidentでもRender jobをCopyへ誤dispatchしないようjob種別を独立に保持する。
Clear/Drawで資産配置が失われないよう各cold boot前に作成する。Copyが選ぶstagingは
そのgraphが準備した1allocationに限定し、別stagingへlivePTEを書き換えない。
Clear/Draw自身のcold再構築、動的WDDM residency変更時の正規stopは今回変更しない。
CPU数/timeout/cacheflush/DDK packet ABIの回避変更なし。専用probebootなし。

実prepare_copy + 実transfer/CCBコードを64連続transaction、578拒否gateでASan/UBSan検証。
hardware/cache/IRQ/firmwareだけmock。実silicon動作・画素保持・reset解消は未証明。

owner private構造変更によりplatform consumerも依存再compile対象（native5TU+miniport1TU）。

再現recipeはbaseline/copy.usc152B（SHA5c68917f…）を検証し専用generated headerを生成。
実署名source03f35eacのbuildは既337generated headerを再利用した。後続tools変更は
同asset/header byte一致を確認して依存を明示しただけでruntime C変更はない。
