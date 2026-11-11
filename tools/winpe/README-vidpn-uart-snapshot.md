# VidPn callbackの固定UART snapshot案

`vidpn-uart-snapshot.h`は未接続のuser-mode helper。service本体、既存reader、
writer、profile、署名品は変更しない。`A04_VIDPN_UART_SNAPSHOT`は既定0。
有効化・呼出し・実機でのUART搬送は未検証であり、このsourceだけでは有効にならない。

既存`vidpn-callback-trace-policy.h`はschema検査だけを行い、callback値をUARTへ
出力しない。本helperはそれを置換せず、将来のownerが明示的に呼ぶための候補。

## 読取りと鮮度

- 固定HKLM `SYSTEM\CurrentControlSet\Services\GE8300Render\Parameters\VidPnCallbackTraceV1`だけをKEY_READで開く。
- schema1、最大44個の値、各REG_BINARY/432B（108DWORD、うちdata88DWORD）。
  「44DWORDのrecord」ではない。subkey、未知label、重複、type/size不正は拒否する。
- canonical indexはEpochV1=0、CkS0..3/F0/L0=1+(k-1)*6+[0..5]、C1U0=43。
- 各読取り前後のkey値数/subkey数/last-writeを照合し、さらに全値を2回取得して
  raw snapshot全体を比較する。変更を検出した場合は再試行せずCHANGEDを返す。
  これは安定した2回読取りであり、Registry全体のtransactional/atomic snapshot保証ではない。
  2回の間で元に戻る変更等の不可観測性は残る。
- 全44枠を検査してから出力する。既readerと同じく古いepochのrecordもshapeを検査し、
  sequence/epoch FILETIME/epoch interrupttimeの全一致でcurrent/staleを区別する。
- 今回の独立startup witnessから得たexpected epochとの一致、完了済み/成功のEpochV1を
  要求する。Registry自身のepochをexpectedにコピーして鮮度成立としてはいけない。
- 欠損、読み取り失敗、torn、未完了callback、古いepochは成功や「未呼出し」の証明にしない。
  snapshot取得成功もGPU/DDI成功とは別。PASSIVE/APCでskipされた記録もあり得る。

## 実source・SYS・sealとの結合（有効化ownerの必須条件）

`a04_vut_binding`は今回run/PID、独立witnessのsequence/FILETIME/interrupttime、
**writer source SHA1・配置済み実SYS SHA256・最終main seal SHA256**を別々に持つ。
digestは生byte列を4byteずつlittle endianでDWORD化する。文字列やpathは出力しない。

ownerは既存の署名package/manifest/pinned inputsからwriter sourceとSYSを結び、
既存preserved/installed identity guardで配置SYSの実hashを照合した後だけ
`sys_verified=1`を渡す。helperはこの外部検証を再実行・代替しない。
seal SHAは実最終seal/receiptから渡し、compile前の仮値や循環する自己hashを埋めない。
実sealをtargetへ届ける既存の厳密なreceipt経路がなければ有効化は未準備とする。
自己申告したdigest値だけで暗号学的出自を証明したことにはならない。

回収側はBINDのrun/PID/epoch/source/SYS/seal全tupleをmainの正本と一致確認する。
同じepochでも別source/SYS、別run/PID、未結合sealは今回結果として受理しない。
host fixtureの数値は合成値で、実候補の出自として使わない。

## 数値packet契約

transport callbackは`emit(context,event,const uint32_t *payload,count)`。
**同期的にconsume/copyし、zeroを返した場合だけqueue受理（wire到達の証明ではない）**とする。保持pointerは禁止。
既存UART lifetime/cancel所有権を保つbounded adapterの実装は統合owner担当。
service.logの本文、Registry名、SSID/MAC/IP/鍵/pointerは出力しない。

| event | DWORD数 | 意味 |
|---|---:|---|
| 0x3d0 BIND | 32 | [0]protocol1、[1..2]run/PID、[3]sequence、[4..7]epoch、[8..12]source SHA1、[13..20]実SYS SHA256、[21..28]seal SHA256、[29]caller SYS検証1、[30..31]reserved0 |
| 0x3d1 SUMMARY | 16 | [0]protocol1、[1]432、[2]全present数、[3]current record数、[4]stale数、[5]current未完了数、[6]current負NTSTATUS数、[7]C1U0数、[8]first枠観測kind bitmask、[9..10]Epoch completed/status、[11]二重snapshot一致1、[12..15]reserved0 |
| 0x3d2 RECORD | 36 | [0]canonical index、[1..35]下記raw-word選択。currentの各kind F0/L0とC1U0のみ。status、IsSupported FALSE、mode/format/timing、入出interrupttimeを含む整数だけ |
| 0x3d3 END | 4 | protocol1、出力record数、総packet数、総payload DWORD数 |
| 0x3df ERROR | 4 | protocol1、固定error enum、reserved0、reserved0。BIND/SUMMARY/ENDなし。今回source/epochの成立を主張しない |

最大15records、18packets、592 payload DWORD（2368B）。`a04_vut_uart_emit`は
`VU event packet-ordinal word-offset word-count words...`の固定hex整数行へ分割する。
1行最大94B、6words、全体100chunks/9400B以下。既存`uart_record`の96B storageを
超えるlogical packetを直接渡さない。`VU`は既存PB状態機械と混同させない。
ownerは新規zero初期化の`a04_vut_uart` contextを1回だけ使い、
既存uart_recordへ接続するsend callbackで累積deadline/cancel/quarantineを維持する。
例として累積30秒budgetを各chunk前に検査し、超過時は即失敗する。
このsend callbackとservice内のcallsiteは本変更には含めない。
本文を追加しない。
S0..3の全recordも検査するが出力しない。F0は初回負status、L0は最新入出、
C1U0は初回status0/IsSupported FALSE。L0未完了やstaleの数を隠さない。
各件数は保存record数であり、S/F/L間の重複を含む。callback回数へ読み替えない。
first枠がbootで消費済みでもL0/F0/U0を選ぶ。未保存の呼出しは復元しない。
End欠落、count不一致、未知event、重複recordは不完全とする。
SOURCE/SYS結合失敗、ERRORやtransport失敗を既存GPU結果の成功へ昇格しない。

RECORDのraw-word（0-based、432BをDWORDとして数えた位置）は固定で、
`3,4,5,14,15,18,19,44,45,46,47,48,49,50,51,52,54,55,56,58,76,77,80,82,83,84,86,104,105,89,90,93,94,88,99`。
順にslot/complete/status、begin/end interrupttime、data24..31、
before QueryVidPn/pinned source status/width/height/format/source-target count、
afterの同7項目、after target active width/height/VSync numerator-denominator/
pinned target status/scanline。値の意味はdata30の観測maskと各query statusを併読する。
未取得0/FFFFFFFFを実modeや成功と解釈しない。FILETIME/interrupttimeは同一単位でも
別epochであり、callback時刻はinterrupttime、startup同定はBINDの両clockで行う。

## 統合と検証の範囲

serviceの診断/cleanup/finalize/STOPPED/reboot状態機械は本変更で触らない。
将来の統合では全client終了後、terminal logを閉じる前のuser-modeから1回だけ呼び、
出力エラーは別結果として残す。GPU結果・guard・既存schemaを緩めない。
新macro1とinclude closure・実binding receiptをmain reviewなしで配備しない。

host fixtureはpure parser/選択/binding/二重読取り/最大出力/transport失敗を検証する。
Windows Registry adapter、実UART transport、driver/OSの動作は未検証。
