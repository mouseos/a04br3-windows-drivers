# 親1121／HCD1019のWindowsビルド・署名recipe

対象sourceは `work/mt8167-musb-role-power1019`。runtime sourceの固定基点は `2a540408c2d54c771759a50b634a006c9fdfc6f3`。recipeを含む実際のHEADを親／HCD双方の出力manifestへ記録する。runner42751027265の操作とWindows上での実行はmainが行う。本担当はrunnerを照会・操作していない。

## 固定入力と再compile

`tools/ci/musb1121-hcd1019-inputs.json` が入力closure。根拠は署名親clock1120 manifest（source `0f4a51ba`）と署名HCD1018 manifest（source `32444ffe`）。親のcompile argsは前者からpathだけを写像する。HCDのcompile args／link entryは後者を生成した `32444ffe:tools/ci/build-mt8167-ep0-1018.ps1` に合わせる。原recipeは `tools/ci/baselines/hcd1018-build.ps1` にhash固定の参照資料として保存した。これは実行しない。

| 対象 | 処理 |
|---|---|
| 親本体 `wdf_parent_probe.c` | 署名1120のsafe flags・全機能フラグを保持して再compile。ABI3関連header／incの変更を含む |
| `clock_snapshot.c` | 署名1120のargsで再compileし、未固定の旧objectへの依存を避ける |
| HCDの10 TU | 全て再compile。旧1018の6object再利用を廃止 |
| 親の13 helper | 現source/header closureと署名manifestのobject hashが一致する場合だけ再利用。新規compile済みとは報告しない |
| Windowsホスト模型 | 実WDK型で既存7模型を実行。依存11 TU＋試験7 TUを同じcheckoutからcompile |
| SDK／WDF | 既存2518 header hash、kernel 4 libs、WDF 2 libsを照合。SDK setup／downloadは行わず、不足・不一致なら停止 |

変更したowner/state/board-proof headerとparent incの本番依存先は親本体と `musb_physical.c`。`affected_production_TUs` に逆引きを記録し、13 helperに変更header依存がないことも照合した。project includeは条件分岐内を含めて再帰hash固定。ARM compileには診断用の `-MD -MF` だけを追加し、実compilerの依存ファイルもevidenceへ保存する。

親は1120のclock snapshot ABI1／resource1117を維持し、thermalは再build・再packageしない。出力はowner ABI3の親2026.11.10.1121（`shared-parent-musb1121.inf`）とHCD2026.11.10.1019（`mt8167-musb-hcd.inf`）の対応tuple。INFをsourceからbyte同一でコピーし、INF由来のCAT名も検査する。

ローカルで再梱包した `recipe-inputs.zip` は既存13 helper objects＋WDF 2 libs＋WDF headers 42本＋署名manifest 2本、計59ファイル。新規コンパイル物・秘密鍵は含まない。元は保存済み `link-reuse.zip` と `wdf13-existing-closure.zip`。Gitへpushするのはsource／recipe／hash metadataだけで、このZIPやSDK／object／署名packageはpushしない。

入力ZIP SHA256: `7ae304ceb5bf787e8b4e5fc25e5a2315a5b52db42706002adb26db8ab82d9482`。その他の既存cache入力はclosureの `remaining_existing_inputs` を参照。

## mainがWindowsで実行するコマンド

新しい専用TaskRootへcloneし、mainが入力ZIPを置く。C／headerのLFを維持し、INFのみcommitted `.gitattributes` のCRLFとする。既存のsource／package／cacheを出力先にしない。

以下はWindows PowerShell 5.1の新しいセッション（必要なら `powershell.exe -NoProfile -ExecutionPolicy Bypass`）内で実行する。`$ExpectedRecipeCommit` は最新handoffに記載されたSHAに固定する。下のkit／署名toolパスは以前のcompile args由来の例であり、runner42751027265上の実在を本担当が確認したものではない。同じhashの既存cacheが別場所ならmainがそのpathを指定する。不足時にSDK setupへ進まない。

```powershell
$Task = 'C:\mt8167-musb1121-hcd1019'
$ExpectedRecipeCommit = '<handoffのrecipe commit SHA>'
# Taskは新規専用領域。mainがrecipe-inputs.zipをここへ転送する。
git -c core.autocrlf=false clone --branch work/mt8167-musb-role-power1019 https://github.com/mouseos/a04br3-windows-drivers.git "$Task\source"
if ($LASTEXITCODE) { throw 'clone failed' }
git -C "$Task\source" checkout --detach $ExpectedRecipeCommit
if ($LASTEXITCODE) { throw 'checkout failed' }
if ((Get-FileHash "$Task\recipe-inputs.zip").Hash.ToLowerInvariant() -cne '7ae304ceb5bf787e8b4e5fc25e5a2315a5b52db42706002adb26db8ab82d9482') { throw 'input archive drift' }
Expand-Archive "$Task\recipe-inputs.zip" $Task
$src = "$Task\source"
$recipe = "$src\tools\ci\build-mt8167-musb1121-hcd1019.ps1"
# 最初にmain側のnative parserで構文を検査する。
$tokens = $null; $parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile($recipe, [ref]$tokens, [ref]$parseErrors) | Out-Null
if ($parseErrors.Count) { $parseErrors | Format-List; throw 'PowerShell parse failed' }
$Kit = 'C:\mt8167-public-fullsource323\wdk81'
$common = @{
    ParentSignedManifest = "$Task\inputs\parent1120-signed-manifest.json"
    HcdSignedManifest = "$Task\inputs\hcd1018-manifest.json"
    ReuseDirectory = "$Task\inputs\reuse"
    KitInclude = "$Kit\Include"
    UmInclude = 'C:\mt8167-minimal-service-host\cache\kit81\Include\um'
    WdfInclude = "$Task\inputs\headers\wdf"
    KernelLibraryDirectory = "$Kit\Lib\winv6.3\km\arm"
    SignTool = 'C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
    Inf2Cat = 'C:\mt8167-musb-ep0-1018\tools\signing\bin\Inf2Cat.exe'
    OutputDirectory = "$src\out\musb1121-hcd1019-plan"
}
# 既存入力照合とplan保存だけ。compiler／signer／デバイスを実行しない。
& $recipe @common
if (!$?) { throw 'plan/input validation failed' }
# 別の新規出力へ、12 ARM TU＋18 host TU、7模型、対応tupleのlink／署名。
$common.OutputDirectory = "$src\out\musb1121-hcd1019-signed"
& $recipe @common -BuildAndSign
if (!$?) { throw 'build/sign failed' }
```

署名identityは既存 `BCBF6C456C6F0DD0818434763B189488FBF13570`。秘密鍵をexportしない。SYS／CATの署名・trust・同一identityと、SYS／INFのCAT所属を確認する。両SYSのimportsを保存済みRT8.1 exportと既署名baseline allowlistに照合する。`lld-link`／`llvm-readobj` は既存clang20.1.8と同じbinから使用し、全toolのhashをmanifestに記録する。

回収対象は `musb1121-hcd1019-signed-tuple.zip`、`musb1121-hcd1019-evidence.zip`、`signed-tuple-manifest.json`、`compile-plan.json`。manifestの両moduleのsource_commitが同じ実HEADであること、owner_ABI=3、INF versionとhash、compile12／親helper再利用13／HCD object再利用0／模型7を確認する。出力packageは既存parent1120／HCD1018の場所へ上書きしない。

## StageOnly、実DIF、実機成功を分ける

現実機USB20の選択driverはユーザー申告のkeyboard1010。保存済み1018署名packageの存在は、1018 HCDが選択・開始された証拠ではない。

このrecipeは **StageOnlyも実DIFも行わない**。driver storeへの登録・INF選択・デバイス開始は別工程で、`StageOnly=false`／`DIF_INSTALLDEVICE=false`／`deployment_performed=false` をmanifestへ記録する。将来mainがStageOnlyを実行して成功しても、storeへ置けたことだけを示す。実DIFの成功、選択INF／実SYS／開始状態の照合、実host列挙・転送・給電改善とは区別する。

通常のPC接続はdevice-roleであり、host用 `MT8167\USBPORT20` PDOが不在でもそれだけで故障扱いしない。HCDを出現させるためのroot node捏造、強制role、source ON、手動GPIO操作をこのrecipeから要求しない。親のID／外部VBUS／OC／停止証明の条件を満たす実host試験はmainが別途判断する。

## 今回の検証範囲

ローカルではPython verifierでsource closure、逆依存、INF／署名manifest由来のhash metadata、PSの字句上の括弧対応を検査し、header drift等7ケースの拒否を確認した。`python tools/ci/verify-musb1121-hcd1019-recipe.py --self-test` で再現できる。

Windows PowerShell本体・native parser・Windows compiler／linker／署名toolは未実行。runner環境の入力充足、Windows 12 TU compile／7模型、link・署名・CAT・imports照合の成功はmainの実行待ちである。ローカル字句検査はnative PowerShell構文検証の代わりではない。実USBPORT開始・PnP再生成・host列挙・5V給電改善は引き続き未検証。
