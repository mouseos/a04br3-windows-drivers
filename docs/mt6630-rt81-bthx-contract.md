# 採取RT8.1 inboxのMT6630 BTHX起動条件

対象を固定した静的解析結果。Windows10資料から実RTの挙動を推定したものではない。
標準radio起動・実接続・通信・共存・SCOの実機成功は未確認。

対象SHA256:

- BthMini.SYS（6.3.9600.16384）: `18be9f55df80e137e77cea47cca8e210977ac1f82436bb2e6c9d64777cbb654b`
- bthport.sys（6.3.9600.16421）: `4456ed0e0cbdbb6e590039dc5f2b4aa06466043d2c34a917670e81b9e99f9a1f`

REA/Ghidra12.1.4で独立解析。バイナリ・命令ダンプ・逆コンパイル出力はこのrepoへ収録しない。
以下のアドレスは各imageの解析VA（base0x10000）。

## 受理条件

BthMini 0x176b0がGET_VERSION（0x410403、出力4bytes）を送り、返却値と1のANDが0なら
0xC0000424で拒否する。bit0がある場合、選択値1をSET_VERSION（0x410407、入力4bytes）へ渡す。
双方のIOCTL結果は0を要求する。GETの値が厳密に1でなければ拒否する実装ではない。

BthMini 0x177d0がQUERY_CAPABILITIES（0x41040b、出力16bytes）を送り、0での完了後に
次の3条件を全て要求する（0x17816–0x1782a）。

| 能力構造体のoffset | WDK8.1 field | 条件 |
|---|---|---|
| 0 | MaxAclTransferInSize | unsigned値が0x103以上 |
| 4 | ScoSupport | 2（ScoSupportHCIBypass）と等しい |
| 8 | MaxScoChannels | 1と等しい |

いずれか不一致なら0x17850へ分岐し0xC0000182を返す。
None/0、HCI/1、Bypassでもchannel0/2はいずれもこの検査を通らない。
Idle/Wake（offset12/13）はこの3条件検査の対象ではない。
構造体にSize/Version fieldを追加する契約ではない。

IOCTL helper 0x179fcはIoBuildDeviceIoControlRequestのInternalDeviceIoControl=FALSEを使う。
通常のIRP_MJ_DEVICE_CONTROLであり、METHOD_NEITHER/カーネル用という宣言から
IRP_MJ_INTERNAL_DEVICE_CONTROL必須とは判断しない。
STATUS_PENDINGの場合は完了eventを待ってIoStatus.Statusを返す。
このhelper/交渉側はIoStatus.Informationの厳密長を検査しないが、driverは正しい長さを返す。

## 開始経路と基本radioの境界

BthMiniは0x18000で0x38byteのminiport callback tableをBTHPORT_RegisterMiniportへ登録する。
table[0]は0x176b1、table[1]は0x177d1（Thumb bit付き）。
BthPortのAddDevice内0x72828がtable[0]を呼ぶため、version交渉はAddDevice段階。
PnP START_DEVICEは0x73610→0x177b4→0x74188へ進み、table[1]を呼ぶ。
戻り値が負なら後続初期化に進まずPnP開始失敗へ返す。
採取BthMiniの該当callbackにはNone/0で基本radioだけを開始する例外分岐がない。

能力検査成功後にのみ0x174ccが受信IRPを確保し、0x122fcがEvent1本とACL2本の
READ_HCI（0x410413）を開始する。入力は4byteのpacket type、出力はpacked context
（DataLen:ULONG、Type:UCHAR、Dataはoffset5）。Eventは最大257byte＋5byte、
ACLはMaxAclTransferInSizeを0x1000へclampした値＋5byte。
WRITE_HCIは0x41040f、同じpacked入力、出力は転送byte数のULONG（4byte）。
READとWRITEの以後の到着順は非同期で、ここで固定の全HCI command列を主張しない。

## 現実装へ反映する判断

combo client2008はversion/長さ/major/packed contextを既存WDK8.1と合わせている。
None/0を返すので採取inboxの開始検査を通過できない。これは静的に確定した予測であり、
この候補を実機へ投入して同じ失敗を測定したという意味ではない。

標準radioを起動させるためにBypass/1を無根拠に広告しない。今回driver codeを変更せず、
provider1015/client2008の署名bytesを保持する。文書だけの変更で新しい署名版と呼ばない。
必要な残実装は、実MT6630設定に基づくsideband SCO能力とWindows音声providerの結合。
QUERY_CAPABILITIESにactive peerを事前要求する循環gateは不要だが、能力自体の裏付けは必要。
FC72 bit/rate/master-clock、codec latency、方向別DMA drainとHFP owner結合の未確定値を
成功扱いして埋める修正は行わない。bthhfenum以後の音声動作はこの調査の確定範囲外。
