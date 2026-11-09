# VidMmのpaging busy契約

run165の停止は0x10E/B/C00000A3。Microsoftの[0x10E仕様](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x10e---video-memory-management-internal)ではsubtype BはBuildPagingBufferの不正な戻り値を意味する。

[BuildPagingBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_buildpagingbuffer)の許可値はSUCCESS、GRAPHICS_ALLOCATION_BUSY、GRAPHICS_INSUFFICIENT_DMA_BUFFER。従来wrapperはowner admissionを取得できないとDEVICE_NOT_READYを返し、既使用中のaperture/保持jobでもDEVICE_BUSYまたはGEO_BUSYを返していた。

350はowner-held allocationを扱えない間、許可されたGRAPHICS_ALLOCATION_BUSYを返す。未送信または未完了の参照、実MDL、fenceを保持し、実drain/reset証拠を得るまで解放しない。pagingが成功したという結果、架空のDMA/割込み/完了は作らない。未知の失敗を一律にbusyへ変換せず、既busyとowner admission閉鎖だけを対象にする。

これは実native submit失敗の解消とは別の修正である。349r2の実claim処理と同じ実機更新にまとめ、GPU側の成否はR48/R49/実IRQと同じプロセスの画素で確認する。実機検証は未完了。

既349r2からKMD clearとminiport identityの2TUだけを増分ビルドし、他78objects/UMD/FW/clientを保持する。実コードと全ソースビルドの出自はfullsource gateで確認する。
