# COLOR4 標準DXGI候補 3.3 HANDOFF

今回の限定タスクを署名候補まで完了。実機・runner起動試験は未実施。
run53 flat3.2（SYS/DLL/auto6303 EXE）は全hash不変で凍結保持。
本候補は **別INF 11/07/2026,0.0.3.3**。run53媒体へ混在させない。

## 現物・出自

```text
port/xnu/out/windows-rt-wddm-color-signed-4f557944/
```

- 最終wrapper/UMD/client/package source: `4f55794471a4008c1ceaa3a1da2679c0f3fefd2a`
- runtime pair compiler ARM32 object source: `909b4e574f7f09780cf2070f9b5222f64b70391e`
- E native owner: `e83792d88485883130e137309b0ef027b3c7bc5e`、無変更の実libを再利用。
- compiler lib PCO/NIR/util/support/CRTと独立MDL-lock SEH objectは検証済baselineを再利用。
  新compiler TUは `pco_pair.c` だけ。kernel NoFP/NoNEONをuser compilerへ適用していない。
- WDKでKMD header消費9TUと新UMD 5TUをcompileし、最終SYS/DLLへlink。
- SYS/DLL/CAT/clientはBCBF署名 **Valid**、新CAT membership SYS/DLL/FWの3件成功。
- 回収15fileのhash一致。ZIP SHA256:
  `2bf8ea1201fe9792eaa64c0d59b36387269292e58ea0d14e38c8aefd656dd44a`。

| ファイル | 署名後SHA256（FWは不変） |
| --- | --- |
| ge8300-render.inf | `9cfe636ba637698158a30a9da82e6e077a635f0a4635c38a1d24cde09938f0aa` |
| ge8300-render.sys | `51f92e0975a5274b620769e2c6b45628ae3effd5ef82da080b9af939dd1f4f55` |
| ge8300-umd.dll | `f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b` |
| ge8300-d3d-clear.exe | `28e8e0155bb2982f87ab2b64d5fa049c5d17d639cf22ba0d6f3dcab8597d9cbd` |
| ge8300-render.cat | `d8f22f92ebbb0b66368f58b66a5f4098c5c2aa9cdbe35e816ed1680484862855` |
| ge8300-mips.fw | `5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1` |

`signed-manifest.json` / `linked-manifest.json` / `pair-arm32-manifest.json` /
`client-build-manifest.json` / `signed-client-manifest.json` /
`recovery-manifest.json` / `recovered-hash-check.json` が個別出自とhashを保持する。
`runner-recipe.ps1` は専用h4の実実行recipe（秘密情報なし）。source recipeは
`build-pair-wdk81.ps1`、compiler `build-pair-arm32.py`、`package-linked-wdk81.ps1`。
ログは `port/gsi/logs/ios-feasibility/windows-rt-wddm-color-4f557944.log`。
既存runner環境は破棄せず保持。

## 実pair compilerとV2接続

実D3D11.1 DDIのstage token/signatureを保存し、有限SM4 MOV/decl/LINEAR/RETを検証。
2input POSITION4/COLOR4とVS2output/PS1inputのregister/mask/system-value/component型を確認。
shader作成時に対応stageを検証、Draw時に実VS/PSをpair linkして最終PCOへ通す。
未対応instruction/resource/stateは拒否し、固定USCを返すfallbackを設けていない。

PCOは実decoded source registersからNIR IOを生成し、preprocess→link→reverse-link→
lower→postprocess→encodeを実行。linked IOからcomponent allocationを行い、
Z/Wはcompilerの実useから係数領域を計算する。最終pco_dataのrange/count/offset/
entry_offset/temps/sharedsを120B snapshotへ写す。

実生成結果（Eがworking artifactを独立照合済み、固定909生成でも一致）：

| stage | USC bytes | temps/shareds | vtxins/vtxouts | coeffs |
| --- | --- | --- | --- | --- |
| VS | 88 | 0/0 | 8/8 | 0 |
| FS | 80 | 5/0 | 0/0 | 20 |

USC VS `c623f3047d0dec747d81ab3b298979fdde89576403de6af66f89eb8bf65d710f`、
FS `2103c3048ed23a9dd336274c708a61b6fc21438e960e9766ee2e63d3de815796`。
position start0/count4、COLOR VS start4/count4、FS start4/count16、smooth4、usesZ0/W1、
entry offset両0。実artifactは `pair-host-*` と `pair-metadata.json`。

InputLayoutはslot0の2float4を実InputRegisterで解決し、runtime VB/IBを標準Lockから読み、
POSITION4+COLOR4の3vertices=**96B**を実別VidMm allocation/MDLへ書く。
flatの48B内部allocationとは分離し、同一bufferのサイズ不一致や古い頂点の再利用を避ける。
request_v2は2allocation（同target＋独立vertex）を標準RenderCbへ渡す。
KMDはMmCopyMemoryで全packetを不変captureし、size/version/spans/96B範囲を検証、
両実allocationをresolveしてE `GetDrawOpsV2.prepare_draw` へ渡す。
submit/実IRQ/fence/cache visibilityは既存native control-v2を共用。
shaderのuser compiler storageはRenderCbの同期capture完了後にreleaseし、pending DMAへ貸さない。

## main/Raman向け標準client

```text
ge8300-d3d-clear.exe --hardware=PowerVR
```

同一EXE一回、外部timeout **120000ms／retry0**。実PnP→KMT LUID→DXGI非Softwareの一意選択を維持。
今回のclientは明示 `-EnableColorPair` buildでschema5となる。既定buildはCOLOR profile OFF。
必須disklogは `C:\MT8167D3DClear\clear-*.log`、UMD補助traceは同dir。UART不要。

標準Clear二色→従来flat赤triangle/readbackの後、公式FL9_1 COLOR VS/PS、2element InputLayout、
96B immutable VBを標準APIで生成し、頂点red/green/blueのsmooth triangleをDraw。
CopyResource/Flush/blocking Mapで実GPU結果を読む。private IOCTLで成功を代用しない。

readbackは全画素hashを保存し、pixel-center barycentricの整数比で内点のRGBを
UNORM8各±2で検査、外点は黒を検査する。境界±16/3840だけを明記して除外。
内点140000超・外点300000超・COLOR_MISMATCH=0を要求し、平坦色では成功しない。
COLOR_DRAW_ISSUEDは発行記録でありGPU成功ではない。
最終exit0/API_READBACK_SUCCESS=1はflatとCOLORの全readback成功を要求する。

## 範囲・実機未確認

既定OFFの限定開発profile。FL9_1全機能/製品適合、一般dynamic shader全対応、NoDMA stopは未qualified。
対応は有限POSITION/COLOR full-float4 MOV pair、smooth、full800x600、cullNONE/textureなし/depthなし/blendなし。
未対応shader/stateの成功stubは追加していない。
親1014 WithSpm+SD1／ACPI6e852b1とCPU DODはmainの別tuple管理のまま。
新Power tupleや他driverをこの候補へ混入していない。実機はmainのみ。
