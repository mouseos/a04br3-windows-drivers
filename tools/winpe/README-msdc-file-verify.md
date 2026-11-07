# eMMC複数ブロック書込みの再現ビルドと限定ファイル検証

対象は `work/emmc-write-performance` の `6b5043e` を含む固定コミット。実機操作・runner起動はこの手順やツールでは行わない。進行中のWinPEコピーを中断せず、主担当が次の検証区切りでまとめて実施する。

## WDK8.1再現ビルド

既にSDK/WDK8.1 ARM headers/libs、LLVM、既報の2010–2040開発証明書があるWindowsビルド環境で、固定コミットのcheckoutから実行する。既存のSSH/runnerを新規起動・再設定するスクリプトではない。

```powershell
.\tools\ci\build-msdc-boot-write-performance.ps1 `
    -OutputDirectory '.\build\msdc-cmd25-candidate' -Sign
```

証明書の既定thumbprintは `2655FC89A7082D3BAB3F26EC556EE8A3C52F4358`（CurrentUser/My）。必要なら `-CertificateLocation LocalMachine` または `-CertificateThumbprint` を明示する。新しい鍵は生成せず、公開CERだけを出力する。秘密鍵は出力しない。署名不要のcompile/linkだけなら `-Sign` を付けない。

既存成功版2371bd0と同じguarded isolated passive boot構成を固定する。これは当時のSYSとバイト一致するビルドではなく、現行ソースで同じ構成系列を再現するもの。

| 設定 | 値 |
| --- | --- |
| A04_BOOT_EXPERIMENT | 1 |
| A04_BOOT_PASSIVE_EXPERIMENT | 1 |
| A04_PASSIVE_IO | 1 |
| A04_ENABLE_WRITES | 1 |
| A04_READONLY_DISK_PROBE | 1 |
| A04_DMA_PROBE / A04_DMA_DISK_PROBE | 各1 |
| A04_VIRTUAL_BOUNCE_EXPERIMENT | 1 |
| A04_SERIAL_QUEUE_EXPERIMENT | 1 |
| A04_ISOLATED_REQUEST_EXPERIMENT | 1 |
| A04_EMMC_IDENTITY_HANDOFF | 1 |
| **A04_BOOT_MULTIBLOCK_EXPERIMENT** | **1** |
| **A04_DMA_WRITE_MULTIBLOCK_EXPERIMENT** | **1** |
| **A04_DMA_POLL_TRACE** | **0** |

`READONLY_DISK_PROBE` は既存コンパイル構成名であり、この組合せをread-onlyと呼んではいけない。GPT範囲でguardされた書込みを有効にする構成である。scratch LBA、reset診断、PIO、独立したread-only multiblock等の継承環境は除外し、ビルド終了後に元の環境変数を復元する。

比較用に同じソースの単セクタ構成を別の新規出力先へビルドできる。これも自動では実行・適用されない。

```powershell
.\tools\ci\build-msdc-boot-write-performance.ps1 `
    -OutputDirectory '.\build\msdc-single-sector-baseline' -SingleSectorBaseline -Sign
```

この場合multiread/multiwriteは共に0、poll traceは0。比較構成の違いもmanifestへ残る。出力先が存在する場合は上書きせず拒否する。

出力は `package/mt8167-msdc.inf`・SYS・CAT、署名時の公開CER、`msdc-file-verify.exe`、コミット・全設定・ソースと成果物SHA-256を含むmanifest。署名はSYS→catalog再生成→CATの順。旧SYSに新しいCATを混ぜない。WinPEの既存staging用INF名が `a04-msdc-probe.inf` でも、パッケージ本体の対応とhash pinを新しい一式へ更新する必要がある。既存WIMの差替え・起動時信頼ストア反映は主担当の媒体builderで行う。旧起動済みWinPEへこの手順が影響することはない。

## 専用ファイルによる限定検証

主担当が候補ドライバをロードしたWinPEで、既存 `guard-windows-volume.exe` による対象volume確認を行ってから、ARM32の次のコマンドを実行する。正常OSで実行する場合も既存ファイルやコピーの完了を待ち、結果を区別する。

```bat
X:\a04br3\guard-windows-volume.exe
rem guard成功を確認した場合だけ次へ進む
X:\a04br3\msdc-file-verify.exe --verify
```

ツール自体もC:がfixed drive、label=Windows、NTFS、512-byte sector、read-onlyでないこと、空き16MiB以上を確認する。volume serialやCIDを取得せず、`GetVolumeInformationW`のserial出力先にはNULLを渡す。

固定の `C:\Windows\Temp\MT8167MsdcFileVerify` を新規作成し、既存なら拒否する。Windows/Tempのreparse pointも拒否する。専用の `verify.bin` はCREATE_NEW・shareなし・delete-on-closeで開き、既存ファイルを上書き・削除しない。raw device、任意パス、raw LBA、入力データを指定する引数はない。

ファイルは64KiBのみ。2つの決定的な合成パターンを使い、合計128KiBのpayloadを書き、128KiBを読み戻して全バイト比較する。第1回は4KiB要求、第2回は512/1024/4096バイトと末尾を含める。VirtualAllocで整列したバッファ、NO_BUFFERING、WRITE_THROUGH、FlushFileBuffersを使用する。既存ドライバと同様に、成功をDMA inactiveだけで判定することはない。

合格条件はexit0、両passの `VERIFIED_BYTES=0x00010000`、`VERIFY_ERROR=0`、`CLEANUP_ERROR=0`、最後の `RESULT=PASS`。stdoutには要求数・write+flush/read+compare時間（ミリ秒）・最初の不一致offsetとbyte値・エラーコードを記録する。数値は16進。stdoutの保存・既存UART auditへの組込みは主担当が行う。ログを保存する場合は自分で新規確保した出力先へ向け、volume release前に終了させる。release後はC:へ書かない。

失敗時も作成したファイルだけをcloseで削除し、作成した専用ディレクトリを空の場合にだけ削除する。OS停止等でディレクトリが残った場合は次回を拒否する。既存ディレクトリをツールが自動清掃・再利用することはない。

ファイルI/Oはfilesystem metadata更新も伴う。4KiBのWriteFile受付だけでは実CMD25の発行を証明しない。小さい範囲の内容照合であり、全媒体、電源断耐性、長時間安定性、DISM/バックアップ全体の速度を保証しない。時間は同じ条件の比較用で、保証された速度倍率ではない。同期I/OがOS内で停止した場合の厳密な実時間上限も保証しない。

## 検証の区別

ローカルARM32 COFFコンパイル／PEリンクと、主担当のWDK8.1ビルド・署名、WinPEでのdriver受理、上記ファイル一致、実バックアップの速度改善は別工程。manifestのhardware_verifiedはビルド時点ではfalse。実機で一致を確認するまで性能修正の実機成功とは扱わない。
