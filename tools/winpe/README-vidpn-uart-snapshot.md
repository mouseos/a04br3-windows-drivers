# GPU379 VidPn UART snapshot候補

未統合、admission-readyではない。service本体・既reader・writer・profile・署名品は変更しない。
A04_VIDPN_UART_SNAPSHOTは既定0。既存vidpn-callback-trace-policy.hはshape検査のみで、同じUART機能はない。

## 現379で取得できる値と鮮度の限界

現SCMの独立current-start epoch取得元は確認できていない。本helperはこれを実在する前提にせず、
常に **current_boot_unproven（BIND[3]=0）** を出す。Registryのraw epochで同epoch/不一致を分けるだけで、
今回boot、今回client APPLYへのcallback、callback成功/未呼出しを証明しない。
古いraw epochと古いrecordが整合したまま残る場合もある。

runtime入力は既存PBのcombined_run_idとcombined_service_pidで足りる。
runはGetTickCount XOR PIDの既存値で、試行番号232ではない。run=0も保持する。
ownerはzero初期化したa04_vut_bindingにこの2値を入れて呼べる。
writer source SHA1と実SYS SHA256は任意の宣言値。未取得ならzero/unknownを出す。
sys_verified=1は既preserved/installed identity guardで実SYSを照合した場合のみ。
未知を既知へ昇格させるための仮hash/flagは渡さない。

source identity→署名package manifest→配置SYS→**host最終seal**は外部で結合する。
最終sealをruntime入力に要求/埋込みしない（SCM自身hashを含む循環を作らない）。
hostはPB run/PIDとUART boot fence、候補source/SYS正本、後続same-load回収epochを照合して鮮度/出自を判定する。
UART単独の宣言値は暗号学的証明ではなく、このhost結合前に今回callbackや実sourceを確定しない。

## snapshotの厳格境界

唯一のschema根拠はb9fb writer vidpn_callback_trace.hと既read-vidpn-callback-trace.ps1。
最大44値、各REG_BINARY/432B＝108DWORD（header20＋data88）。「44DWORD record」ではない。
固定HKLM SYSTEM\CurrentControlSet\Services\GE8300Render\Parameters\VidPnCallbackTraceV1だけをKEY_READで開く。
subkey/未知label/重複/type/size/epoch shape不正は拒否。
canonical indexはEpoch=0、CkS0..3/F0/L0=1+(k-1)*6+[0..5]、C1U0=43。

全値を2回取得し、各回前後でcount/subkey/last-write、2回分でraw全体を比較する。
変化なら再試行せずCHANGED。不足ならMISSING。全値検証完了まで送信しない。
安定した二重読取りでありtransactional/atomic snapshotではなく、観測間で元に戻る変更等は不可観測。
全recordのshapeを検査してsequence/epoch FILETIME/epoch interrupttimeの一致を判定する。
不一致recordは別epochとして数え、今回未呼出しに読み替えない。
Epoch未完了/負statusもraw証拠として出す。callback未完了/PASSIVE/APC skipを成功にしない。
返値0はsnapshot取得だけ。保存record数はS/F/L重複を含み、callback回数や不在証明ではない。

## 固定整数packet

emit(context,event,payload,count)は同期consume/copyしzeroでqueue受理。wire到達やGPU成功を示さずpointer保持禁止。
digest設定時は生byteを4byteずつlittle endianでDWORD化する。

| event | DWORD数 | 内容 |
|---|---:|---|
| 0x3d0 BIND | 32 | [0]protocol1、[1..2]PB run/PID、[3]current_boot_proven=0、[4..7]raw epoch FILETIME/interrupttime、[8..12]宣言writer SHA1、[13..20]宣言SYS SHA256、[21]raw sequence、[22..23]digest present、[24]caller SYS照合flag、[25..31]reserved0 |
| 0x3d1 SUMMARY | 16 | [0]1、[1]432、[2]全present数、[3]raw epoch一致record数、[4]不一致数、[5]一致未完了数、[6]一致負NTSTATUS数、[7]一致C1U0数、[8]first枠kind mask、[9..10]Epoch completed/status、[11]二重読取り一致1、[12..15]reserved0（不在/成功証明なし） |
| 0x3d2 RECORD | 36 | [0]canonical index、[1..35]下記選択。raw epoch一致の各kind F0/L0とC1U0のみ |
| 0x3d3 END | 4 | 1、出力record数、総packet数、総payload DWORD数 |
| 0x3df ERROR | 4 | 1、固定error enum、0、0。BIND/SUMMARY/ENDなし、epoch/source成立を主張しない |

RECORDのraw-word位置（432BをDWORDとして0-basedで数える）は固定：
3,4,5,14,15,18,19,44,45,46,47,48,49,50,51,52,54,55,56,58,76,77,80,82,83,84,86,104,105,89,90,93,94,88,99。
slot/complete/status、begin/end interrupttime、data24..31、before/afterの
QueryVidPn/pinned-source status/width/height/format/source-target count、after target
active width/height/VSync numerator-denominator/pinned-target status/scanline。
data30の観測maskとquery statusを併読し0/FFFFFFFFを実modeに昇格しない。
各clockのepochは別であり両者を同じ時刻として直接比較しない。

最大15records/18packets/592DWORD。全44値を検査するがS0..3全文は出さない。
bootでfirst枠が消費されてもL0/F0/U0を残す。未保存呼出しは復元しない。
End欠落/count不一致/重複/未知eventは不完全。ERRORやtransport失敗は成功ではない。

## 96B UART bufferとの接続

a04_vut_uart_emitは VU event packet-ordinal word-offset word-count words... の固定hex整数行に分割する。
1行最大94B/6words、最大100行/9400B。本文/名前/SSID等なし。
logical packet全体を既存uart_recordの96B storageへ直接渡さない。VUは既存PB制御eventと別。
ownerはzero初期化したa04_vut_uartを1回使いsend callbackを既存uart_recordへ接続する。
そのcallbackで累積deadline（例30秒）/cancel/quarantine所有権を維持する。
このsend callbackとservice callsiteは本変更に含まれない。
将来呼出しは全client終了後/terminal log close前のuser-modeで1回だけ。
既存診断結果、cleanup/finalize/STOPPED/reboot契約と既schemaは変えない。

host fixtureはmissing/shape/epoch不一致/未完了/torn/最大出力/送信失敗を検証する。
Windows adapter・実UART・host decoder統合は未検証。helper単独の追加実機試験/buildは要求しない。
