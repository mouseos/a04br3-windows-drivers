# MT6630統合接続候補1029

基点は実配置provider1027 source73427b486f3e621dd998774f38f396fbac54a61d。
ユーザー確認2.4GHz WPA2-Personal/AESは対応subset内で、正しいpasswordでも接続失敗。
failed205 RuntimeFailureは旧1011/gen73のstaleだったため直接原因はまだ確定していない。
新driver投入・実通信検証は本作業に含めない。

## 1候補に統合した修正

- 公式OID_DOT11_CIPHER_DEFAULT_KEY契約に合わせ、既無効slotの再削除は接続/cipher/failed/TXpending拒否より先にSUCCESS。
  OID型/長さ/key index/algorithm/peer検証は既存通り、鍵追加・有効鍵削除の条件も不変。FW呼出し不要のidempotent cleanupだけ。
  https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-cipher-default-key
- held1028 source dcbb8043のtranslated RX復元（実group4/3、CCMPはgroup1）、port別32のRXbudget、照合済みTXairfailure1..4のNBL単位failureを統合。
  Linux gen3 nic_rx.h:477/nic_rx.c:687/3139、nic_tx.c:2381、wlan_lib.h:545に基づく契約補完。
  今回の直接原因を証明した変更とは扱わない。unknown/mismatched/late completion、stop/flush、crypto/PN/port拒否は緩めない。
- 新InitializeでRuntimeFailureを新generationに置換してpreconnect OID観測を開始。
  CONNECT_REQUEST受信時はさらに新generationへ。parent_generationはInitialize gen。
  直前OID拒否はstatus/OIDだけを引き継ぐ。CONNECT以前のauth/cipher/defaultkey/その他OID拒否も記録。
  unsupported OIDの戻り値は維持する。NDISが既completeしたrequestへは戻ってアクセスしない。

## 記録契約

レジストリpath/Generation QWORD8と4つのREG_BINARY1624/schema1は維持。
RuntimeFailureに内部build1029のstage100..127を追加。phase/kind/raw/detailは固定数値だけ。
SSID/MAC/IP/key/PN/fingerprint/IE/bufferを記録へ渡さない。
既checkpointのPASSIVE mutex/writerを使い、stage callbackはserialized worker内・NDIS spin lock外だけ。
Haltはworker drain後にrecordをclose、Shutdown/DPC/send entryではregistry操作しない。

同じ64event配列を時系列rollingで使い、overflow=1なら過去の省略を明示する。
RX/EAPOL/TX eventは各stage4sampleまで。OID/制御結果は最新を保持する。
固定flush上限10、BEGIN/ENDのflushだけを既機構で行い、tail flush成功は合成しない。
registry診断失敗でNDIS/無線の動作結果を変えない。

| stage | 内容 |
|---|---|
|100|context1=Initialize/preconnect、2=CONNECT_REQUEST|
|101/102|OID実status/OID、直前拒否の引継ぎ|
|103/104/105/106|dispatch gate/policy/candidate/wake|
|107/108|BSS/STA/channel request投入結果、実grant event結果|
|109/110/111/112/113/114|auth TX/RX、assoc TX/RX、STA activate、wait/backend失敗|
|115/116/117|NDIS OID完了、OS association/connection completion通知（handshake成功ではない）|
|118/119/120/121/122|RXdecode（phase1raw/2translated）、early EAPOL、EAPOL RX/TX/TX_DONE|
|123/124/125/126|key OID、CID7投入/戻り/結果、PTK event/timeout、port OID結果|
|127|context終了（置換/停止はSTATUS_CANCELLED、接続成功とはしない）|

kind1=NDIS、2=NTSTATUS、3=backend、4=観測値。auth/assoc RXのkind4は実802.11status。
TX_DONE kind3は照合済みFW status、kind1はNBL dispatch結果。
OID raw0x103はPENDINGであり成功/失敗へ読み替えない。
OS通知関数はvoidなので「通知呼出しを行った」証拠で、OSが接続完了を受理した証明ではない。

decoder tools/winpe/decode-mt6630-checkpoint.pyと固定clone readerを新stage対応で同梱。
旧reader/decoderは未知stageを拒否するため新candidateと同じsourceを使う。
初期世代・現接続世代を実Initialize parent/record hashで結合し、clockずれや欠落を推測補完しない。

## build/signと検証

公開recipe tools/ci/build-mt6630-provider1029.ps1（UTF8BOM）。INF11/11/2026,2026.11.11.1029。
元NDIS630/ARMNT/Net class、既1027 compile flags/BCBF signerを保持。
18TUは変更header依存を含む必須再compile、残25TUのみ記録source closureと実obj SHAを照合してreuse。
SDK/PCO/GPU/共有親/音声/BTclientの再buildやmachine環境変更はしない。
旧system include履歴なしはstrict_cache_qualified=false。未署名hashとsignedhashを分離する。

既state-machine/実checkpoint writer、actual OID wrapper、実2securityOIDの空slot削除admission、
translated RX/security/credit/budget、async Pauseを限定host検証する。
registry/NDIS/実機の最終挙動はWindows buildやhost fixtureの成功だけで認定しない。
