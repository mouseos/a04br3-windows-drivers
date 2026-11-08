# run145 Patch119/3のKMD/native契約修正

基点は署名340 source03f35eacとrecipe eb2bc820。desktop別branchは不変更・保全。実機main専有、同340再実行しない。

## 実証拠

fresh146回収のR22 C00000BB AllocList2/PatchList2、R23 C00000BB GOP1/slot1/gen1、R28flags0/DMA0..16、R29context1/private0..16。同PID2536 GPR1はfirstCopyRenderCb0→firstMapbefore→stagingLockbefore、GPR2pixel行無し/invalid無し。従ってPatchのnative Copy prepareがNOT_SUPPORTED。IRQL DISPATCHやpaging flag失敗を今回の直接原因にしない。

Microsoft DxgkDdiPatchはPASSIVE_LEVEL、any errorreturn→119/3、同DMAの複数Patchを許す。P2～4は内部pointerでHRESULTではない。
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_patch
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_patch

## source rootとの接続

native340 bind(linear)はcold_for_bindで前Clearを正常stop/retire→coldgraph再作成→stagingattach。prepare_copyはcopy_transfer/onlineを必須にし、その新coldgraphのassets/bootへ到達しない。失敗guard削除でなく、Menciusのcold firstprepareとonline rearm分離が必要。正常要求についてPatch前の必要assets/所有権を保証する本修正と結合する。BuildPagingBufferへエラーを単純移動しない。

KMD修正はticket二重copy/range/patchlocation/perms/context/kind/count/全allocationboundをnativeprepareより前にIRQ lock内で照合する。state4はpending予約として認める。nativeが本当にpreparedtokenを返してstate1になった時だけpost検証が通る。同state1の再Patchは既prepare_pendingの早期returnによりtoken/epochを変更しない。state2/submittedをpreparedと偽装しない。validなdraw/clear/textureの既用法とprivate cursor16Bを保持。guard削除/SUCCESS上書き/偽fenceはない。

## 固定失敗記録

R18 schema2のstatusは実失敗NTSTATUS、a=phase、b=kind、c=slot、d=generation。phase0=session admission、1=入力/range/flags、2=count/kind、3=patchindices/perms、4=二重ticket、5=prepare前参照/residency、6=nativeprepare、7=prepare後再照合。既GeoFactoryCheckpointは失敗statusをPASSIVEでZwFlushKeyする。正常Patchに新flushを追加しない。保存失敗でも元statusを維持する。予備R19はMencius native exactstage失敗の専用slot（同じschema2/実status、a=owner stage、b=cold/live、c=transfer phase、d=generation）として予約。native側だけが書く。KMD側はR19未実装を成功として扱わない。

## 検証・差分scope

実GeoPatchImpl/inspect_preparedをhost抽出実行、ASAN/UBSANで不正context/handle/permission/ticket/residencyのnative呼出し前拒否、同preparedtoken再Patch、元native失敗status/固定phase保持を確認。WDK/native callbackはmock。これだけでsilicon/root解消を証明しない。MSVC SEH/PCO/SDK/UMD/FW/clientに差分なし。kmd_clear1TUのみ追加ARM compile対象。native共有fileはMencius所有。

本KMD差分単独の再実験はしない。Mencius cold/live本修正と結合後、fresh146以降の正式captured基点からmainが媒体を作り、保持pixel9934で同起動結果を判定する。
