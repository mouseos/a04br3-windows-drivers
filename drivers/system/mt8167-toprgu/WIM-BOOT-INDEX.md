# UART画面ESPのWIM起動索引

実機run1ではUEFIのBOOTARM sampleが繰り返され、ExitBootServices・Windows側
マーカー・A04SC画面データは記録されなかった。通常boot/ESPは主担当が復元した。
ログは `port/gsi/logs/ios-feasibility/windows-rt-mma8652-four-uart-screen-run1/`。

失敗ESP `7a515bc1...` と直前の４CPU試験成功ESP `b106fc9b...` はBCD、
bootarm.efi、boot.sdiが同一。BCDのdefaultはWinPE ramdisk、testsigning Yes。
成功版のWIMはimage count 1、Boot Index 1。失敗版の元WIMも1/1だが、
`wimlib-imagex export ... 1 --compress=LZX` で作成した再圧縮WIMは1/0。
`verify` と中の重要ファイルの一致だけでは起動索引の消失を検出できなかった。
索引消失は起動停止の有力な候補であり、ログだけでは唯一の原因と断定しない。

修正: `tools/winpe/compact-winpe-wim.py` は入力ハッシュ・単一image・Boot Index 1を
確認し、`export --boot --compress=LZX --check`、再度Boot Index 1確認、verifyを行う。
ESP生成スクリプトは入力とESP格納後のWIMヘッダでimage count 1/Boot Index 1を要求し、
index 0ならESPコピーの前に拒否する。ヘッダ検査はBCD内容を検証するものではない。

修正版 `mma8652-uart-screen-bootindex1-esp.img` は元の署名済みWIMから明示的に
`--boot --check`で再出力し、ESPを新規生成した。WIM verify、SYSTEM/SOFTWARE、
TOPRGU SYS、画面EXE、startnetの圧縮前後一致、元BCD・bootarm.efi・boot.sdiの
保持、FAT検査を実施。これらはWinPE起動や署名受理の実機証拠ではない。
旧index 0 ESPはこのまま再試験しない。主担当が同じ４CPU bootと修正版ESPを組み合わせ、
EBS、TOPRGU開始、転送、次回BOOT_REASONを分けて確認する。
