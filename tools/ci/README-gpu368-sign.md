# GPU368 署名専用recipe

b936b334076352c0261c801697cce5d489a8c915の署名・CAT検証経路を、fixed198の実回収7成果物へ適応した。compile/link/SDK setup/service変更/実機操作は行わない。既367 recipeは変更しない。

実compile sourceはa04e22489709db79d7c5a4355419bc7aaf95b057。署名recipeのHEADとは区別する。source-build-manifest.jsonのSHAは169a862a5fc3a219594e238ec24220448fd60d1b8ca384cf83759883cb333120。main-output-review.jsonは8cc2253f8e396f3d19ed03726009b5484453d926600c4a5c09ce7966adbb9eb7、main-import-existing-export-proof.jsonはb85712d80ac11175e9ec0d3acc906cc59dd3a5af59c35d6cadf1638ae65a92c8。全7出力、native/modules/UMD/link/PCO回収receipt/originの実SHAも照合する。

mainがWindowsで実行する。署名recipeは別checkoutにcloneし、稼働中のa04 build treeを変更しない。既SignTool/Inf2Catと既BCBF秘密鍵を使用する。Baseline366Packageは旧outputs366.zipのpackage366展開先。既存の全14 leafとsigned-manifest.jsonを固定SHA照合する。パスを推測せず実在する展開先を指定する。

```powershell
# この公開branchを別checkoutへcloneし、公開された固定HEADで実行する。
# $Baseline366Package / $ExistingSignTool / $ExistingInf2Cat はmainの既存実在パス。
# $ReviewDirectory はlocal回収物から下記2ファイルをuploadした既存ディレクトリ。
$Linked198Directory = 'C:/mt8167-rt-srv-cold198-lf/build/rt-srv-fixed198'
python -B tools/ci/sign-ge8300-fixed198-368.py --baseline-package "$Baseline366Package" --linked-directory "$Linked198Directory" --main-review "$ReviewDirectory/main-output-review.json" --import-proof "$ReviewDirectory/main-import-existing-export-proof.json" --output "$PWD/build/gpu368-signed" --sign-tool "$ExistingSignTool" --inf2cat "$ExistingInf2Cat" --sign
if ($LASTEXITCODE -ne 0) { throw 'GPU368 signing failed; do not deploy partial output' }
Compress-Archive -Path "$PWD/build/gpu368-signed/*" -DestinationPath "$PWD/build/gpu368-signed.zip"
Get-FileHash "$PWD/build/gpu368-signed.zip" -Algorithm SHA256
```

Python 3.9以降、cleanな署名recipe checkout、新規outputが必要。元VM出力のUMD/clientはumd/配下を使用でき、回収後root aliasも同一SHAの時だけ受理する。--signを外すと入力照合とsigning-plan.json保存だけになり、署名toolは起動しない。planと署名実行では異なるoutputを使う。

render INFはDriverVerを11/10/2026,0.0.3.68へ変更しCRLFを正規化する。他の論理行は変更しない。同a04生成render SYS・UMD DLL・4クライアントexeをBCBF6C456C6F0DD0818434763B189488FBF13570で署名する。旧FWは5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1を保持する。新render CAT生成後、INF/SYS/UMD/FWの所属と証明書identity/trustを確認する。4クライアントは独立PE署名のみであり、CAT所属とは記録しない。旧補助dxgi exe・UMD import lib・cerはbaselineのままとmanifestに明記する。旧import libをa04生成物と扱わない。

transport357の署名済SYS/INF/CATをbyte同一保持する。SYS SHAは902009a9201a416c6a160504d47809f1e7fbfccf08ce32376b40caa60ae829c5。保存start357-build-manifest.jsonのactual_include_closure 7件は全てa04とSHA一致する。interface356.hのGUID/version/snapshot/callback、frontend_memory.hのopaque allocation/MDL lease、transport356.cのPFN/SG照合契約は不変。別TU windows_shared_scanout.cとdma_pages.h/gpu_mmu.hも14ac163b9e579d91f72a99e321da292412bf1f4cからa04までbyte不変。recipeはこの10pathを実checkoutで再照合する。旧transport実compile source efaf1008add44565f6c5fc4597e67804b22570fbと宣言source14acを分離して記録する。RT/SRVのprivate native allocation構造はtransport公開ABIに露出しない。このsource/ABI互換監査を根拠として旧357を保持し、新a04生成transportは7入力の一つとして検証するが署名・採用しない。a04とのbinary同一や実機検証成功は主張しない。

回収対象はgpu368-signed.zip全体。package368/signed-manifest.json、output-file-hashes.json、evidence/内の入力review・import proof・署名/CATログ・certificate-report.jsonを含む。署名によるPE変更はchecksum/security directory/certificateのみと照合する。全出力を独立reviewしてから候補化する。runtime identityのGPU359-start-gap-v1/e072 literalは継承値であり、実compile source a04と混同しない。

SHARED DEFAULT Texture2D single mip/slice/sample、RT+SRVはfull-native-extent BGRA、同allocationの同時read/write不可という制約を維持する。blend/scissor/部分copyの追加実装はこのbranch範囲外。DWM/実hardware成功は未確認。Linuxでの入力・staging・拒否契約検証とfullsource verifier通過はWindows実署名成功を意味しない。PCO375完全archiveは既回収品を保持し、この署名処理から再構築しない。
