# GPU358：分割common-bufferのcold zero検査

357の800x1280 profileではGPU VAが連続でも、TPC storageは複数の独立した
64KiB common bufferへ跨る。storage_pointerは一つのblock内だけを返す契約なので、
全TPCへCPU連続pointerを要求したcold callerがC0000182を返していた。

既storage_readを256Bずつ呼んで全wordが零であることを確認する。GPU VAの減算で
used内のspanを検査してからULONG化、aligned bufferを使用し、各storage readは
既存block inventory/extent/word-readの契約を満たす必要がある。欠落、不整列、
範囲外、read失敗、非零はいずれも拒否。code/pointer/descriptor照合は従来どおり。
容量、PTE数、DMA request上限、所有権、実制御、guard、DIS0を変更しない。

production full caller ASan/UBSanでold357 exact failure、新358正常、TPC末尾非零拒否、
storage block extent欠落によるstorage_read拒否、従来800x600正常を確認する。
OS/DMA allocationとCPU転送はmockであり、実DMA・起動・scanoutの成功証拠ではない。
初期testのfault2（TPC長+1）はalignment検査で、read失敗とは区別する。
後続testは実block extent欠落を使う。新MMIOやbinary変更・逆解析は行わない。

再現: python3 tools/ci/test-ge8300-segmented-cold358.py --out build/segmented-cold358
署名再現: tools/ci/build-ge8300-segmented-cold358.py を既357 cacheから実行する。
変更C TUはwindows_native_job.c/miniport.cだけ。header影響が無いことと、再利用memberの
source/include closureを検証し、raw80の変更2/不変78、UMD22/PCO36/DIS0不変を要求する。
177/179/357固定物、FW0943、BT/providerは更新しない。実機はmainのみ。
