# Pixel比較完了の固定slot証拠client

新file ge8300-d3d-texture-pixel-boundary.exe。旧boundary055bbff3...と旧clear f3af...、旧texture2727328f...は配布bytes/正本commit/pinsを変更しない。新macro GEO_PIXEL_RESULT_CLIENTだけ有効化し、標準D3D device/clear/draw/copy/map、pixel oracle、shaderbytes、UV/filter列、成功閾値は既存と同じ。既900秒client制限を媒体policyで維持する。driver/SYS/UMD/native/FW/SCMの変更なし。

既GPR1はprecommit済み4096B。既stage0..25とUMD8..17を維持。slot32/33=clear0/1、34=triangle、35=color、36..43=texture0..7を一回ずつ使用する。比較全loopとsample判定の直後、Unmapや次の描画前に64Bを一回WriteFile＋FlushFileBuffers。途中Draw/Copyへ新sync/logを加えない。全suite完了時も最大12Flushだけ。write/flush失敗時はUnmapしてE_FAIL、成功証拠を合成しない。

GPR2 rowは16 little-endian DWORD: magic0x32525047/version2/PID/slot/PASSbool/kind/case/tickLow/tickHigh/MATCH/MISMATCH/aux0/aux1/aux2/pixelHash/checksum。checksumは先頭15DWORDに既GPR1と同じFNV1a word演算。旧GPR1の40B rowは変更しない。既decoderだけではGPR2を解釈できないためtools/ci/decode-ge8300-pixel-progress.pyを併用。CRCまたはcount/判定不整合はinvalid、slot無しは未観測でFAILやPASSへ推測しない。PID/RUN/選択binaryhashと相関する。

- clear: MATCH=expected BGRA一致数、MISMATCH=不一致数、aux=expected/480000/0。PASSは既mismatch==0。
- triangle: MATCH=red+black、MISMATCH=other、aux=red/black/sampleMismatch、PASSはred>0 AND black>0 AND other==0 AND samples==0。**全黒もMATCH480000/MISMATCH0になりうるがPASSfalse/red0で拒否**。triangle正しい面積を新たに仮定しない。
- color: MATCH=interior+exterior-mismatch、MISMATCH=既count、aux=interior/exterior/edgeExcluded。PASSは既interior>140000/exterior>300000/mismatch0、UNORM8許容差2を維持。
- texture: 同count、aux=interior/exterior/edgeExcluded。PASSは両領域nonzero/mismatch0、既8UV/filter列の既期待値/許容差2を維持。

PASSはCPU readback値の比較が完了した証拠であり、Unmap/次job/サービスfinalization/driver全体正常完了の証拠ではない。MapS_OKだけでは行を作らない。Flush成功も媒体障害やtorn writeを保証しないのでCRCとdecoder照合を必須とする。

1TU buildはtools/ci/build-ge8300-pixel-client.py。既332build-manifest/client memory.objのhashを検証して保持。既ARM17763 UM4libsとBCBF signing identity、既SDK81 headersを再利用。全SDK/PCO/shader再build無し。出自はsource hash/1TU flags/object hash/保持memory hash/signatureを別manifestに記録。

次GPU根本修正版の試験へ同時採用する。証拠取得だけの専用boot/旧338同条件再試験はしない。媒体/実機owner main/Mencius。desktop branchとは分離。
