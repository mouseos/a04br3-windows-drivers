# 全allocation Lockの契約修正候補

332を保持する独立候補。`clear_ddi.c`の全領域LockはNumPages=0、pPages=NULLであるがLockEntire=0だった。335ではLockEntire=1を指定する。ReadOnly/DonotWaitと実LockCb、元要求のretryごとの再構築、成功時のみhandle更新、source unlock、失敗時retainは維持する。

一次資料：
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_lockcb のRemarksは、LockEntire未指定かつpage list無しではD3DERR_NOTAVAILABLEを返すと説明し、LockEntire指定時はNumPages=0とpPages=NULLを要求する。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-_d3dddicb_lockflags のLockEntireはallocation全体をLockする指定。

これは実sourceの契約欠落の修正であり、run135 resetの直接根因が確定したという意味ではない。GPR stage16はretryループ全体の入口、17は出口なので、一回目callback未帰還とbusy再試行中のresetを区別できない。また同slotの再利用によりClear世代は特定できない。現TDR成功も未証明。

production Lock/Bltのmock runtime回帰は全callbackでLockEntireとpage listの組合せを検査する。ARM増分helperは332のUMD22membersのうちclear_ddi1TUだけ変更し、他21members/entry2obj/PCO/native/KMD/FW/clientをhash固定する。新INF/CATと新DLL以外は再build・再署名しない。GPUcopyのwire/owner変更は含まない。現媒体と332正本は不変更。実機成功は主担当の証拠待ち。
