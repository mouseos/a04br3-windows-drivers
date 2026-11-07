# 標準DXGI限定開発候補 0f8f68d2

署名・CAT membership・回収hash確認済み。実機実行はmain担当で未確認。
成果物は作業ルートの `port/xnu/out/windows-rt-wddm-typed-signed-0f8f68d2/`。

## 固定出自

| 項目 | 固定値 |
| --- | --- |
| H package/helper source | `0f8f68d2` |
| H typed runtime | `3f17e2f2`（Nash `16650913`と同じ4TU、b7852497のtrace/ClearView/client保持） |
| 再利用baseline | `693858095b1511ff8abc66782cc91767d33bc56f`の実objects、PCO/NIR/CRT closure |
| 今回追加compile | `umd/typed_view.c`だけ。Nash実4TUをhash検証して再利用 |
| E owner | `e83792d88485883130e137309b0ef027b3c7bc5e`、typed viewport修正入り |
| KMD | H69385809実archive＋E e83792d8、未署名SYS `fce6b9885c567346aa9b7cea3757c9a50ad321f309771a0a130eab8d3c4cc0b5` |
| client | `3f17e2f2`固定の実WDK ARM32 compile/link、標準DXGI/D3D11のみ |
| 親 | `287241c37fa0eae538e5e2d136dea5df42de7e43`、1014 WithSpm+SD1 |
| ACPI | `6e852b1ff13e16609957afc7a2e03da3d371dfe8` |
| INF | `11/06/2026,0.0.3.1`、MOU0006排他適用 |
| firmware | `5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1` |
| signer | `BCBF6C456C6F0DD0818434763B189488FBF13570`、全4署名Valid |

署名済みhash：

```text
SYS    37d3d7a656e99c48e5a5b041479f9ebf44593ffb073ea019660b1729d9d098a6
DLL    3ebcbbcab37bb2b5e77e7861febb8a2bd63c9152e22fb6f67e6f09594462f64a
client 3055dd6bf2b5bd887c3d967c65ee8d496c63e69fc994f509069d18f6e94bac2f
CAT    a9ce6e0cbd3291e8c985ea15faaa02f609e4093428f247272ca68b3fba8b941f
ZIP    8e961ec4c0f50b99388864dcfaa71f82fe39e201806133e257c6cf4c6efad679
```

`signed-manifest.json` / `linked-manifest.json` / `client-build-manifest.json` /
`signed-client-manifest.json` / `recovery-manifest.json` /
`recovered-hash-check.json`で個別出自とhashを追跡できる。
SYS/DLL/FWのCAT membershipはrunnerのsigntoolで3件確認済み。
clientは独立Authenticode署名。秘密鍵の回収は行っていない。

## mainの媒体・実機手順

MOU0006の既存KMDF providerを排他的に置換する。IRQ所有はDxgkのみ。
MOU0007 DODは既存CPU present構成を維持し、GPU vendor-copy interfaceを必要とする
構成との混在を避ける。今回HはDOD/親/媒体共通filesを変更していない。
同版0.0.3.1の旧候補を過去にstage済みなら、DriverVerだけで今回の実配置を判定せず、
上記SYS/DLL hashと選択INFを回収する。

まず標準clientを引数なしで実行するとDXGI adapter inventoryをdiskへ保存し、描画せず非0終了する。
その実DXGI/PnP証拠からMOU0006のLUIDを選び、同じEXEを次の固定形式で実行する。
標準APIだけでLUIDを選択し、first-adapter、WARP/REF fallbackは行わない。

```text
ge8300-d3d-clear.exe --luid=HHHHHHHH:LLLLLLLL
```

`HHHHHHHH`/`LLLLLLLL`はログのLUID_HIGH/LOW各8桁hex。
既存mainの固定serviceから起動可能。任意payload/brokerは不要。

試行は一連の標準API操作：
CreateDevice→800x600 BGRA target/staging→magenta全画素readback→green全画素readback→
POSITION4 VS/constantPS/immutable VB/CB/raster→黒Clear→赤triangle Draw→CopyResource→
Flush→blocking Mapで実readback。
DXBCは公式compiler生成のvs/ps_4_0_level_9_1、SHDRはE finite whitelistと一致。

必須disklogは `C:\MT8167D3DClear\clear-*.log`、UMD traceは同dirの `umd-*.log`。
UARTを必要とせず、UMD補助traceの輸送失敗だけでは標準APIを拒否しない。
成功はclient exit0＋API_READBACK_SUCCESS=1＋色/triangle実readback判定。
DRAW_ISSUEDはGPU成功を意味しない。HRESULT、red/black/other画素数、inside/outside、hashを保存する。
失敗時はUMD OPEN_INTERFACE/VERSION、CreateContext、PCO/state、ErrorCbとclient HRESULTを対で回収する。

## 範囲と継続

既定OFFの限定開発profile。標準runtime交渉を9_1 pipelineで行うが、FL9_1全適合や出荷適合を主張しない。
未対応operationはErrorCb/unsupported。NoDMA stop/TDR等の未qualified項目は保持。
今回clientはPOSITION4/constantPS flatを使用し、COLOR4 pairの全結合を初回試行の門にしない。
一般アプリ互換性、COLOR pair/frontend拡張は並行継続する。

新runnerは保持。H専用dir `C:\mt8167-wddm-client-h2`、署名packageは
`src\build\signed-0f8f68d2`、回収ZIPは `signed-0f8f68d2.zip`。
ソースrecipeは `link-typed-incremental-wdk81.ps1` と `package-linked-wdk81.ps1`。
固定実行ログは作業ルート `port/gsi/logs/ios-feasibility/windows-rt-wddm-seal-0f8f68d2.log`。
