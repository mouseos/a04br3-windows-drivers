# 通常I/Oの詳細UART trace

実際の配置済み0.0.1.7はf8f54c9b162cc0b9cc342fb0dc94953c1f59cae5由来の
SYS SHA256 fb4d168baa3d8ec8297c1fe33b195a9e1af85a79bf4f4267103b677bbd24fa2a。
この基点とboot build flagsを維持し、正常要求の大量UART出力だけを既定OFFにする。
PIO/DMA/CDB/転送長/guarded write範囲/単一キュー/worker/排他/resetは変更しない。

`A04_MSDC_VERBOSE_IO_TRACE=1`（build時）で従来のサンプリングとESP書込traceへ戻る。
未指定または0ではBuildIo、正常StartIo、blocks、ESP write、成功CDBの詳細を出さない。
request counterは出力OFF時も同じように加算する。失敗CDBは既定で残す。
誤った値はbuild時に拒否する。全driverの出力をまとめて無効化するswitchではない。

DMA pollは独立した既存`A04_DMA_POLL_TRACE`を維持する。正常IO traceのON/OFFに
連動させず、DMA failure、transfer failure、write guard拒否、SCSI/SRB拒否、reset/worker、
FindAdapter/HwInitialize、bugcheck、GPT signature、stress sentinel等はそのまま残す。
通常起動SYSは従来どおりDMA poll=0。WinPE等でpollを有効にした別buildの観測を、
通常起動SYSの出自や今回の抑制対象と混同しない。

固定boot候補は`tools/ci/build-msdc-quiet-boot.ps1 -OutputDirectory <checkout内新規dir>
-CertificateThumbprint <既存開発証明書>`でcompile/CAT/署名できる。
`-DetailedIoTrace`と`-DmaPollTrace`は独立した追加指定。SDK再構築や配置・再起動はしない。
INF版は0.0.1.8、日付はrunner UTCで有効な11/07/2026。

`tests/msdc_io_trace_test.py --out <project root内の専用dir>`は固定baselineと候補の
実A04RunIo/BuildIo/Senseを同じCDB/SRB/MMIO/DMA fixtureで比較する。
データ・sense・status・転送長・counter・MMIO/DMA呼出しを照合し、詳細ONではbaselineと
全traceを比較、既定OFFでは正常traceだけが除去され異常traceが一致することを確認する。
またminiport.c全体がlogging gateの挿入以外で変わっていないことを厳密に確認する。
baselineの32-bit LBAを32bit右shiftする既存UART引数だけはhost fixture内で64bit castへ
同一正規化し、UBSanを有効にする。製品sourceのその引数とI/O処理は変更していない。

大量正常IO出力と他driver記録との文字混線は主担当の実UART観測。起動の遅延量や
この候補による速度改善量は未測定。ホストfixtureのログ量削減を実機速度改善と扱わない。
実機boot、stall原因、長期安定性や電源遷移の成功を今回の署名候補だけで広告しない。
