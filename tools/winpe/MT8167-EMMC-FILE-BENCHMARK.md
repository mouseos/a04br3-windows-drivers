# Samsung AJTD4R / MT8167 MSDC0 filesystem比較クライアント

既存msdc-file-verify（d4d8d57d）を基に、同一4KiB×1024要求の単一write/flush/read/compareへ限定したfreestanding ARM32 Win32 client。ドライバ・媒体・service・registryを変更しない。署名鍵は既存のものを明示して使用する。

```
mt8167-emmc-file-benchmark.exe --bench
```

これ以外の引数は拒否する。main/Menciusのperipheralprofileで、**1.8適用中に1回、1.9適用・再起動後に1回だけ**使う。workerは実機で実行しない。診断fileは両回とも固定の `C:\MT8167MsdcFileBenchmark\benchmark.bin`。専用directory自体から新規作成し、既存directory/fileがあれば無変更で失敗する。旧試行の残骸を自動削除/再利用しない。mainの診断rootへEXE/pinと独立したbefore/afterログを置き、既存ログも上書きせずCREATE_NEWのcaptureへ接続する。

対象はfixed C:、label Windows、NTFS、512byte sector、空き16MiB以上に限定する。raw device/volume handle、任意パス、LBA、device IOCTL、userfile走査、既存fileのoverwrite/deleteはない。VirtualAllocのaligned 4KiB buffer、NO_BUFFERING/WRITE_THROUGH、CREATE_NEW/shareなし/delete-on-closeを使う。生成した4MiB fileだけをcloseでcleanupし、自分で新規作成したdirectoryだけを空の場合に削除する。既存fileはopen/deleteしない。filesystem metadata更新は発生する。

file長/write payloadの上限は4MiB。4KiB write1024回→実FlushFileBuffers1回→seek0→4KiB read1024回→同じ決定的patternの全byte照合を、1process内で行う。2passや多重起動、別sizeのwarmupは行わない。合計payloadは4MiB write＋4MiB read。これは通常writeimageのphysical readbackではない。rawdiskwriteは禁止のまま。

時計は両回ともGetTickCountのms。同じ署名EXE/hash/引数/bytes/clockを使用する。書込み＋pattern生成、flush、read＋compareを別計測する。全phaseのbytes/requests/elapsedとVERIFY_ERROR/CLEANUP_ERROR/FLUSH_OKを16進でstdoutへ出す。失敗時も終了までの成功bytes/elapsedを残す。合格はexit0、RESULT=PASS、WRITE_BYTES/READ_BYTES=0x00400000、各REQUESTS=0x00000400、FLUSH_OK=1、VERIFY_ERROR/CLEANUP_ERROR=0。時間0msの場合は分解能不足であり速度を無限大として計算しない。write throughput比較にはWRITE_FLUSH_MS、readにはREAD_COMPARE_MSを使い、bytes/secondsで計算する。

要求間ではphase120秒を超えたら打切りcleanupへ進む。ただし同期Win32 I/O/flush/closeがOS内で停止した場合の絶対実時間上限は保証できない。強制killやhandleの早期破棄を追加しない。error/cleanup失敗ならその条件で再実行せずログを返す。

before/afterには搭載MSDC SYS SHA256/選択INF1.8または1.9、親1026、CPU数、同時activity、client hash/exit/logを対応づける。1回ずつの結果は限定的なfilesystem実測で、全媒体・電源断耐性・常用安定性や純粋なdevice速度を証明しない。一括profileの他変更やbackground I/Oがある場合、差をMDL変更だけの因果効果とは断定しない。host mock試験やrunnerのbuildを実eMMC速度として報告しない。

再現build:

```powershell
.\tools\ci\build-mt8167-emmc-file-benchmark.ps1 -OutputDirectory '<checkout内の新規dir>' -CertificateThumbprint 'BCBF6C456C6F0DD0818434763B189488FBF13570'
```

WDK8.1のkernel32.libのみlinkし、CRTなし、既存鍵でEXEを署名する。manifestにcommit/flags/source/exe hash/clock/固定pathを残す。署名は主担当環境の既存trustを前提とし、新規鍵や信頼設定を追加しない。
