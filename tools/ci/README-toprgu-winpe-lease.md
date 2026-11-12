# WinPE専用TOPRGU lease package

`build-toprgu-winpe-lease.ps1`は、固定sourceからTOPRGUの2TUだけを直接compile/linkし、
既存BCBF identityでSYS/CATを署名する入口です。通常Windowsへ導入するpackageではありません。
WinPEで1800秒のlease、CPU1 handoff、UART screen/volumeを使う候補を作ります。
SWRST後の永久停喂・再arm不可・D0Exit時のHW WDT保持はmain実装のruntime契約であり、
このbuilderのmarker検証は実機reset成功を証明しません。

汎用`build-mt8167-toprgu.ps1`やrx builderを呼びません。主treeの汎用builderは
cache引数と`-MD`に未対応であり、専用packageのINF正規化もこの入口で行うためです。
GPU、他driver、SDKのbuild/install、WDF overlay復元、証明書生成・import・private key export、
VM/実機操作は行いません。転送用closure/ZIP作成もmain側で扱います。

## mainが先に固定するもの

- mainによるruntime・`winpe_lease.h`・本recipeを含むcleanな固定commit。
- 既cacheのWDK root（`Include/km/shared/um`）。kernel ARM libsが別配置ならそのroot。
- mainが復元・検証したWDF1.13 overlayのheader root（`wdf.h`を直下に持つ）と
  ARM library root（`wdfdriverentry.lib`/`wdfldr.lib`を直下に持つ）。存在は未確認扱いであり、
  builderは不足時に停止します。探索やdownloadで補いません。
- 既cacheの`signtool.exe`/`Inf2Cat.exe` directory、および既clang/lld-link/llvm-readobj。
- `BCBF6C456C6F0DD0818434763B189488FBF13570`の既My private identityと、同一DERの既Root cert。
  trust storeは変更しません。証明書期限・実Windows署名・CAT membershipを検査します。

```powershell
& "$src\tools\ci\build-toprgu-winpe-lease.ps1" `
  -ExpectedSourceCommit $FixedCommit `
  -OutputDirectory "$src\out\toprgu-winpe-lease13" `
  -WdkRoot $ExistingWdkRoot `
  -WdfIncludeRoot $RestoredWdf13IncludeRoot `
  -WdfLibraryRoot $RestoredWdf13ArmLibraryRoot `
  -KernelLibraryRoot $ExistingKernelArmLibraryRoot `
  -SigningToolRoot $ExistingSigningToolRoot
```

`KernelLibraryRoot`省略時は`WdkRoot/Lib/winv6.3/km/arm`を使用します。
`CertificateStore`は既定CurrentUser（必要時LocalMachine）。thumbprintはBCBF以外を拒否します。
全出力・TMP/TEMP/TMPDIR/RUNNER_TEMPはcheckout内`out/`の新規directory配下へ置きます。
祖先reparse point、既存出力、dirty tree、commit不一致を拒否します。
C/H/recipeは`git hash-object --no-filters`で実byteと固定commit blobの完全一致を要求します。
INFだけは`.gitattributes`の`*.inf text eol=crlf`に従う通常checkout（CRLF）を許可します。
ASCII byteを確認後、実CRLF→LFだけを置換し、UTF-8 byteへ
`blob <byte長><NUL>`を付けたSHA1を計算して固定commitのGit blobと照合します。
trim、metadata変更、literal escape変換はsource gateでは行いません。
`source_files`のINF rowへ`actual_sha256`、`canonical_git_blob`、
`normalization=CRLF-to-LF-only`を記録し、従来の`sha256`も実checkout byteのSHA256として保持します。
終了時のsource hash再照合も実byteで行います。INFをraw LFへ戻してdirtyにする必要はありません。

## 固定build条件

| macro | 値 |
| --- | --- |
| A04_TOPRGU_WINPE_LEASE_SECONDS | 1800 |
| A04_TOPRGU_CPU1_HANDOFF | 1 |
| A04_TOPRGU_UART_SCREEN | 1 |
| A04_TOPRGU_UART_VOLUME | 1 |
| A04_TOPRGU_THERMAL_STOP | 0 |
| A04_TOPRGU_DEADLINE_SECONDS | 0 |

TUは`wdf_toprgu.c`と`reset_command.c`だけです。WDKの`Include/km/crt`を`-I`へ追加せず、既signed RX12と同じClang標準header選択を使います。
mainのsyntax診断ではWDK8.1 `intrin.h`とClang builtinの`__ldrexd`宣言衝突が確認されています。
WDK/Clang/header本体へのpatchは行いません。

各`-MD/-MF`から実依存path/size/SHAを保存し、
`winpe_lease.h`が前者の実compile依存にあることを検査します。ARMNT/PE32と
exact marker `A04RGU_WINPE_LEASE_V1:1800`（consumerと同じASCII＋末尾NUL）を
unsigned/signed SYSで確認します。
署名前後ではchecksum/security directory以外の非署名payloadが不変であることも照合します。
source、実依存header、link libs、tool hashを終了時に再検査し、source driftを拒否します。
依存hashはcompile後の観測であり、全headerを過去の全読取り時点から固定していたとの主張はしません。

実source INFはliteral escapeではなく実CRLFです。Git blobはLF、通常checkoutはCRLFであり、
以前のtool表示の二重escapeからの説明を訂正します。現物の`literal_count`は0です。
package生成時は実CRLFを維持・正規化し、literal `\r\n`形式にも変換サポートを残します。
ただしliteral形式はsource gateで特別扱いしないため、固定commitにないbyteを通過させる用途には使えません。
元INFは変更せず、改行変換後のmetadata変更は
`DriverVer=11/06/2026,0.0.13.0`だけです。current runnerの日付より未来なら停止します。
変換済みINFに対してのみInf2Catを実行し、SYSを先に署名してからCAT生成・署名へ進みます。

## 出力とadmission用の証拠

`package/`の固定leafは次の4ファイルです。

- `mt8167-toprgu.sys`
- `mt8167-toprgu.inf`
- `mt8167-toprgu.cat`
- `mt8167-toprgu.cer`（既Rootと同byteの公開DER。PFXではありません）

`manifest.json`は`TOPRGU_WINPE_LEASE_PACKAGE_V1`。
全検証後にだけ`success=true`、`signed=true`を確定します。
`lease_seconds=1800`、`force_swrst=true`、`source_commit`、`critical_flags`、
`certificate.thumbprint`、各outputs rowの固定leaf `name/bytes/sha256`、
`steps`の`name/exit_code/log_sha256`と実argv、source/dependency/tool/lib hashを保存します。
必須署名検証step名は`verify-sys`、`verify-cat`、`member-mt8167-toprgu.sys`、`member-mt8167-toprgu.inf`です。
後二者はWindows SignToolの`/c <actual CAT>`でSYS/INF membershipを検証します。

各toolの実行中だけ`ErrorActionPreference=Continue`へ切り替え、native stderrをstdoutへ
統合してUTF-8 `Out-File`で全量保存します。stderrの最初の行だけでcompile失敗とは判断しません。
`finally`でnativeが更新する実`$global:LASTEXITCODE`をstepへ記録し、元のEAPへ戻してlog hash/manifestを保存します。
実exitが非0なら失敗とし、終了code自体を得られなかった起動失敗も成功には扱いません。

失敗時は成功済みobject/SYS/署名stageとlogsを保持し、manifestをfailure付きで保存します。
自動retry・上書きはしません。packageはWinPE admission用で、通常Windowsへの導入許可や
lease期限での実機reset成功は、このbuilderだけから判断できません。

このsource整備ではWindows compile/link/sign、VM、実機のいずれも未実行です。
