# COLOR4 native結合候補 a098c87e

初回run53/e837/H3.2は不変。別候補としてsource push、増分実WDK8.1 ARM、
V2各entry LinkCheck、H KMD69385809との最終SYS link、既存BCBF署名、
現物回収hash照合を完了した。標準COLOR描画の実機成功ではない。

- native build source: `a098c87e13aed96f510cfa671a659c517fe56c1d`
- transport実装: `47eb0b67f3b76055d4d0faa4391b37cebbc738e2`
- fixed contract: `COLOR4-H-BOUNDARY.md` / `native_color_transport.h`
- shader source chain: H official FL9_1 SM4→有限decode→NIR pair→PCO。
  全USC/tokenはe837受理profileと一致、metadata VS8in/out、FScoeff20/usesW1。
- `a04_native_color_transport_bind/prepare/submit` は実GetDrawOpsV2と
  acquired control-v2 lease.opsを使用。prepare成功とGPU完了を区別する。
- 57成果物のhashを照合。54 objectを検証済みTUから再利用。
- BCBF署名Valid。秘密鍵は回収しない。

現物はROOT/port/xnu/out/windows-rt-native-color-a098c87e/。

|現物|SHA256|
|---|---|
|ge8300-native-owner-core.lib|2910c37093d3d1405a063c566dfb51e6b005162eee256c45fb798835d758a721|
|signed/ge8300-render.sys|9510759c4537bd552a46463cf073ad17230f030a001c9db5f11acaa115d2d39f|

`HOST-VERIFIED.json`、`build-manifest.json`、
`signed/signed-native-manifest.json`に固定。
runner42355353070の実物は
`C:\mt8167-native-factory-a098c87e\src\build\native-owner-arm-r1\ge8300-native-owner-core.lib` と
`C:\mt8167-native-factory-a098c87e\signed-color-native\ge8300-render.sys`。

この署名SYSはnative側結合入力。HのCOLOR dispatcher/compiler/UMD/client、
別version INF/CATの最終結合はまだ含まない。新しいinstallable COLOR packageや
全FL適合を完成と扱わず、H側が固定境界から結合する。
EはHのkmd_clear/capture/miniport/UMD filesを編集していない。
