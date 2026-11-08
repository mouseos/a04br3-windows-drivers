# fresh132 RTC標準時計 / AFE実open error 通常一括

source基点abf4aa06とINF所在API修正9dca0d03。AFE client5ab5c8b2を単一branchへ統合し、既署名byteを再利用する。clockcompare733931ddのconsumer source/36byte ABI headersを同byteで追加。driver/GPU/SDKは再buildしない。

fresh132の34原本・Recovery boot id・実13node bindingを固定し、11roleを全てretain/no-DIFとする。GPU327/SYS/UMD/FWを保持し、旧GPU clientは起動しない。新MT8167CombinedFollowup132だけをStart2 arm、診断開始時Start4へ戻しCREATE_NEW markerで1回に制限。予定restart0。

RTC560cf boot4b67/FD e633はmainの実搭載winload219464f sealに依存。最初の通常起動前にmainがbootを1回更新、以後保持。boot適用とWinPEでのSYSTEM armは別契約。共通EDK/GPT非変更。Capabilities未資格/実OS採用未証明を保持し、clockcompare exit0も時計同期成功とは称さない。

通常bootで既存RTC/thermal/WLAN query（scanなし）/SD presence/既導入keypad HID bindingを観測し、clockcompare/LC temperature/ACCDET status/AFE jackを固定署名client各1回だけ起動。rotation/HP/GPU clientは実行しない。既知errorを0へ変換しない。いずれか失敗しても後続readonly clientを実行し各結果を別記録しflushする。terminalと同phase finalization0をmainが合わせて判断する。

SCM schema2/11nodes/4clients、PB0x254=installed-only INF取得rc、0x288=finalization、0x207=terminalを保持。旧SCMはFollowup130を増分した12固定service/provider（RpcSs/キーservice含む）をroot/subtreeで前後比較。旧GPU Start4と全ledgerをsubtree fingerprintで保持し、未知identity/型や不一致を成功扱いしない。

回収はmainの同一RO通常mountでmanaged log/markers、RTC stdout、AFE filter-open rc、run131 RUN72316/servicePID1376/clientPID2796に関連するclear/UMD遅延logを45file/name+hash基点と比較する。旧GPU clientは再実行しない。遅延log不在を未実行証明にしない。native post launcher/controllerはmain担当、専用read-combined132-post.ps1のみ本branchで提供。
