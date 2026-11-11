# 実231基点のrun232/post233準備

別branchでbe8を基点に396ecddaのown-filter-root修正を統合。be8の署名SCM/native/媒体は変更しない。実231 MF2fc91c1b、Recoverye4a25046、native receipt b7f3c0e8、assets131/MF395c4e3a、mutable909f0046を実hashで検査する。旧227/229の再load、実機、build/sign、SDK構築は行わない。

## 準備済みと未確定を分ける

`prepare-followup231-inputs.py --prepare-baseline --workspace ROOT --map BASELINE_INPUTS --out NEW_OUT` は共有200/378 generatorをRAM上で再利用し、現在の全13tuple、54service/326named値、同load native、CPの固定schema/epoch、保持boot chainを検証する。出力は `retained-baseline-draft.json` と `finalize-map.pending.json` のみ。baseline内のtargets空/planned0は現状保持の検証用で、次候補の更新計画ではない。次targets/plannedはnull、draft=true、normal_install_allowed=false、headerは出さない。

HegelのGPU/clientは未確定なので `followup231-accepted.json` はpending/nullを維持。actual signed refs・main review・既知native schema adapterが揃うまで、finalize/host seal/SCM/native/mediaは必須拒否する。未確認のschemaを文字列pinだけで汎用受理せず、ADAPTERS/PS knownAdaptersは現在空。実成果物到着後にそのraw schema/source/全出力を狭いsource adapterで結合する。実tuple未着を仮SHA・仮receipt・成功値で埋めない。

## 既callchainへの明示入口

- generator: `prepare-followup231-inputs.py --finalize ...`（現在pending拒否）
- SCM: 既`build-combined200-service.ps1 -CaptureNumber 231 -Followup231 -Mode Service ...`。候補検証はcompile前。
- native: 既prepare-offline-system-service.ps1、新service231だけ。pending adapterをRegLoadKey前に拒否。
- media: 既`build-private-peripheral-batch-media.py --followup231-profile ...`。
- host: `run-followup231.py`、schema `FOLLOWUP231_RUN232_MAIN_SEAL_V1`、run232/post233。

新clientだけならtargets空/restart0、renderを本当に更新するならrenderだけ/restart1という既契約を出発点に、実候補のsource差分と必要性で確定する。他ハード（keypad1.3、transport372、WiFi1031、battery1.8等）/FW/boot/GPTは保持。terminal後Windows保持、collectは別、同候補を再実行しない。sourceに変更のないハードをDIFへ入れない。

## own-filter-root修正の採用範囲

396ecddaはsource-only独立reviewで阻害なし。旧e255→新1659とnegative casesの既保存Python試験が通過。C runtime compile/実行は未実施。更新対象roleに属し、そのcandidate INFの非associated AddService flag0で所有が確認できるfilterだけを明示指定する。baseline全root raw SHA・signed INF SHA・成功DIF bitに結合し、DisplayName/Owners以外を正規化しない。named値とCP限定schemaも維持する。

次候補でkeypad等を保持する限りfilter_root_transitionsは空（COUNT0）。過去にroot遷移したserviceというだけで再び除外しない。必要な対象が実際に更新される場合のみ、実231 root rawとその候補INFを明示結合して採用する。

runner42955460207と保持cacheを将来のSCM1TUに再利用する。新framework、driver/SDK再build、追加native inventoryは不要。
