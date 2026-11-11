# 8715 VU host decoder

`decode-vidpn-vu.py UART --expected-run 0xCA30 --expected-pid 1428`。
run/PIDは今回のPB runtime identityを外部から指定する。runは試行番号232ではない。
大きいUARTには`--start-byte N --end-byte M`でhostが保存した観測区間を指定する。
最大input windowは16MiB。driver/端末/VMへの接続、log書換え、RST、build機能はない。
標準出力へJSON、拒否はexit2/transport_complete=false。

成功の意味は**整数streamの完結だけ**で、必ずcurrent_boot_unproven=true。
source/SYSはruntime宣言値とcaller検証flagをそのまま区別して出し、
source_proven/deployed_SYS_proven/host_seal_binding_proven/hardware_successは全てfalse。
runtime sealを発明しない。source→manifest→実SYS→host seal/boot fence/後続回収の
外部結合はmain担当。今回のcallbackや未呼出しの証明へ昇格しない。

## 受理境界

- 8715の実C formatterによる`VU event ordinal offset count words...`だけを認識。
  全tokenはuppercase8桁hex、単一space、改行終端。LF/CRLFのみ。
- 各chunkは残数と6の小さい方のword数。offset/ordinalは0から連続。
  欠落/重複/混在/順序変更/非整数/未知event/partialは拒否しresyncしない。
- 最大100行、9400B（VU行の実改行byteを含む）、1行94B、18packets、592DWORD。
- BIND32→SUMMARY16→RECORD36を0〜15件→END4の順だけ。
  ERROR4は既知errorとして拒否結果を返し、snapshot成功としない。
- BINDのexpected run/PID、unproven0、epoch sequence/interrupttime、digest present、
  caller flag、reservedを検証。SUMMARYの全44個数/epoch一致不一致/first maskと
  emitted record数/negative/未完了/semantic FALSEの整合性も検査。
- RECORDは実helperが選ぶF0/L0/C1U0のcanonical index昇順のみ。Fの負status、
  Uの完了/status0/IsSupportedFALSEを検証。END件数/総word/packetを照合。
  前後S0..3全文はUARTに含まれないため、元Registryを再検証したとは主張しない。

## UART雑音と別boot

別行の前後雑音やPB行は無視し、内容をJSONへ転載しない。VU行内/先頭に混ざる雑音は
破損として拒否。途中で見つけた都合のよいBIND/ENDへ切り替えない。
2streamがあれば同じrun/PIDでも拒否し、「最新だけ」を自動選択しない。

既host observerでboot境界に使う実`BOOT_REASON`が、最初のVUから入力区間末尾までに
現れた場合は、ENDの後であっても旧boot混在として拒否する。
前のboot履歴がVUより前にあるだけなら、それを鮮度成立の証拠には使わない。
このmarker不在はboot不在の証明ではない。新しい/欠損したboot markerや、同identityの
古い完全streamをbyte列だけで識別することはできないため、常にunprovenを保持する。
実boot境界を跨ぐ入力をhostで切り出して受理を演出しない。区間の正当性と元UART hashは
外部boot fenceで検証する。出力には入力/厳密VUのSHA256と相対byte区間を保存する。

## host確認

`tests/vidpn_vu_decoder_test.py ROOT内の新規出力dir`を1回実行する。
8715の固定合成snapshot fixtureを再利用するC生成器をhostで1回compileし、
実helperのcollector/formatterから44値の最大streamとMISSING streamを生成する。
そのbyte列を正例とし、雑音/PB/CRLF、欠落/重複/oversize/END/偽boot宣言/別boot等の
破損を同じhost run内で検証する。fixtureのsource/SYSは合成値で、実候補の証拠ではない。
Windows/driver build、実UART、実機操作、今回bootの証明は含まない。
