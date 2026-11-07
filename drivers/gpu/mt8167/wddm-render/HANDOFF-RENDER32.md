# 初回標準DXGI flat試験・render 3.2 最終HANDOFF

限定タスク完了：auto clientの実WDK build/link・署名・回収hash照合、0f8 SYS/UMDとの
組合せ出自固定、INF/CAT-only昇版を保存した。標準APIの実機描画は未確認。
媒体・実機はmain/Raman担当。初回flatに不要な機能拡張をこの引渡しへ追加していない。

## 現物と固定ソース

作業ルート内の正式入力directory：

```text
port/xnu/out/windows-rt-wddm-render32-6604ce5b/
```

- INF/package/再現helper source: `6604ce5b62ceb2eb65615512ae4a1fbc2d00b92b`
- auto EXE source: `6303fe54dc745950ea79658c35677c820adeddac`
- SYS/UMD/FW/cert: 旧署名済 `windows-rt-wddm-typed-signed-0f8f68d2` からbyte不変で再利用。
  E native ownerは `e83792d88485883130e137309b0ef027b3c7bc5e`、H KMD baselineは69385809。
- SYS/DLL/EXEは今回再compile・再署名していない。新規署名はCATだけ。
- 旧3.1package、元auto6303packageも不変で保持。新packageへ旧CATを流用していない。
- 新INF: **11/07/2026,0.0.3.2**。
- ZIP SHA256: `bfdb1ce35558aa14ed0555b8ba80685993248a1e038c951ca8a9e98fe67b6db4`。

| ファイル | SHA256 |
| --- | --- |
| ge8300-render.inf | `71f4e0bf7b585d334bbf27992dd04a0514960ef7b4f68f0b68794ce4fff3268f` |
| ge8300-render.sys | `37d3d7a656e99c48e5a5b041479f9ebf44593ffb073ea019660b1729d9d098a6` |
| ge8300-umd.dll | `3ebcbbcab37bb2b5e77e7861febb8a2bd63c9152e22fb6f67e6f09594462f64a` |
| ge8300-mips.fw | `5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1` |
| ge8300-render.cat | `14c9e55fd5091be5aaa972e747b21e20aa6bcdc6b38e530cdb26341b25990121` |
| mt8167-development.cer | `33680a69f647193e7f7adcecf137db525262019c3a8f971ab3095d40f894d72a` |
| ge8300-d3d-clear.exe | `fd6b20243bd991ba1c91c9d5c452ac3a29ddf1cc0cd23e630f928f2d288889d4` |

`combined-provenance.json`にEXE/source/引数/timeout/retry/全7出力hashを固定。
`signed-manifest.json`、`baseline-signed-manifest.json`、`linked-manifest.json`、
`signed-client-manifest.json`、`client-build-manifest.json`で各段階の出自を維持。
`recovered-hash-check.json`で回収hash、source INF一致、旧package不変を確認済み。

SYS/DLL/EXE/CATはrunnerでBCBF署名 **Valid**。新CATのSYS/DLL/FW membershipは3件成功。
ホスト側で全7出力hash一致、元SYS/DLL/FW/EXEとのbyte一致を確認済み。

## date/order修正理由

現KMDF2.5 INFは11/07/2026、旧render3.1は11/06/2026だった。
Windowsの同rank候補比較では日付が版より先に効くため、版3.1が大きくても旧候補を選ぶ危険がある。
日付を同じ11/07へ揃え、版を0.0.3.2へ上げた。HardwareID match/signature等のrank要因を
無視して必ず選択されると主張しない。実選択INFと配置hashはmainのservicing後に回収する。

`package-linked-wdk81.ps1`の固定DriverVerも3.2へ更新済み。
今回の再現は `repackage-typed-inf-wdk81.ps1` を使用する。
署名済baseline manifest hash
`51a22de80a36dc205ecccd4d6e560c6fb289e29424f1d522a0d7a5f1d66bccaf`、
auto signed-client manifest hash
`0e608b5ee14b26f0b34309f77bd035e59e447649f037fb325cc42d8ad59146cd`
を引数で検証し、SYS/DLL/clientの再署名をしない。
Inf2CatはUTC既定で実行し、runner時刻変更やsourceのbackdateは行っていない。
実行ログ：`port/gsi/logs/ios-feasibility/windows-rt-wddm-repack-6604ce5b.log`。

## Raman service固定契約

| 項目 | 値 |
| --- | --- |
| EXE | このdirectoryの `ge8300-d3d-clear.exe` |
| source | `6303fe54dc745950ea79658c35677c820adeddac` |
| args | `--hardware=PowerVR` |
| 外部timeout | **120000ms** |
| retry | **0** |
| 必須ログ | `C:\MT8167D3DClear\clear-*.log`、UMD補助traceは同dir |

Ramanが同timeout/retryを採用済み。serviceが同EXEを一度起動するだけでよく、
inventory解析・2段階起動・手動LUID指定・再起動は不要。
Timeoutは失敗として保存し、プロセス終了をGPU停止/IRQ drainの証明にしない。
成功偽装、自動retry、first-adapter/WARP fallbackは禁止。

選択は実ACPI\MOU0006＋Service GE8300Renderから同PnP interfaceをKMTで開き、
その実LUIDとDXGI Adapter1非Software候補を照合。0/複数や情報取得失敗は拒否する。
Vendor/Deviceは推測値をpinせず全候補の実測値を記録。
詳細は `CLIENT-AUTO-SELECTION.md`。schema4は全候補、決定LUID、選択理由を保持する。
標準Clear二色→POSITION4/constantPS triangle→staging readbackの成功条件は不変。

## 適用条件・未確認

既定OFFの限定開発profile。FL9_1全適合/製品適合、NoDMA stop等は未qualified。
実機の標準API成功はmainの次試行で判定する。UARTは必須条件ではない。
MOU0006は排他的置換、IRQはDxgkのみ。既存KMDF GPUとの同時IRQ所有を避ける。
表示はmain署名済DOD1.6 CPUを使用。親1014 WithSpm+SD1とACPI6e852b1は別入力として
mainが取り込む（このpackageに親/DODを混入させていない）。
