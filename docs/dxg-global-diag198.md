# RT8.1 global診断ringの固定読取りhelper

`dxg-global-diag198.exe`は既存OSの診断ringを1回だけ読むARM32 native helper。
GPU368の既存一括試験へ同梱し、helperだけの再起動・実機反復試験を増やさない。
GPU miniport・UMD・EDK2・既存runner sourceは変更しない。

## 固定した契約

保存対象dxgkrnl SHAは`tools/gpu/dxg-global-diag198-contract.json`に固定。
main保存`rea-global-diag198.json`と`rea-global-escape198.json`の全擬似コード、
WDK8.1 `d3dkmthk.h`の実型、同じOSのgdi32 PE export表を根拠とする。

- APIは**D3DKMT_ESCAPE_DIAGNOSTICS=9**。54210のswitch selectorが`Type-1`なのでraw case8。
  API Type8はDRT_TESTであり、このhelperは呼ばない。case9はAPI Type10の別経路。
- ARM32 `D3DKMT_ESCAPE`は28B。offsetはhAdapter=0、hDevice=4、Type=8、Flags=12、
  private pointer=16、private size=20、hContext=24。実headerに対するC_ASSERTを含む。
- hAdapter=0はcase到達前のhandle検証で拒否される。`D3DKMTEnumAdapters`が返す
  最初の非ゼロhandleを使用する。global ringなのでGE8300公開interfaceは要求しない。
  全enum handleを閉じる。adapterを変えて再試行しない。
- hDevice=0、hContext=0、Flags=0。privilege有効化・昇格・registry/policy変更は行わない。
  Type9 branchに追加のSePrivilege検査は保存コード上なく、実OSの拒否statusをそのまま返す。
  汎用adapter/process access helper内の全条件まで成功を保証したわけではない。
- `ULONG length + raw[256*1024]`を渡し、PrivateDriverDataSizeは262148B。
  Type9は`PrivateDriverDataSize == length+4`を要求する。
  5d018はcopyのみでreader offsetを更新しない。
- OSは読取り前に**種別10の診断packetを追加する**。完全無副作用のメモリ読取りではない。
  制御設定・GPU動作・POST ownershipを変更するAPIは呼ばない。追記で満杯ringの古いentryが
  押し出される可能性があるため、呼出しは1回に固定する。
- `DpiGetGlobalDebugData`222d0はglobal flagsを返す別関数で、ring readerではない。

## mainでのbuild・署名と検証

公開recipe `tools/ci/build-dxg-global-diag198.ps1`を、既存Windows ARM build環境で実行する。
`-Cl`は既存ARM cl（19.29系）、`-Link`は既存ARM link（14.29系）、`-VcInclude`、
`-CrtInclude`、`-Kit81Include`（um/shared/kmの親）、`-ArmUmLib`、`-SignTool`、
`-Rt81Gdi32`、checkout内outの新規`-OutputDirectory`を明示する。
`-BuildAndSign`なしは計画だけ。ありの場合も単一TU compile/link/BCBF署名だけで実行しない。
source/tool/header/actual gdi32のSHAをmanifestへ保存する。

actual RT8.1 gdi32の次のexportは確認済みで、manifestは同じ実体のSHAを要求する。
`D3DKMTEnumAdapters`、`D3DKMTCloseAdapter`、`D3DKMTEscape`。
GetProcAddress失敗時は呼出さず非ゼロ終了。exeの静的importはkernel32のみ。

署名済みexeをroot内へ回収後、既存のactual RT8.1 import verifierも実行する。

```
python3 tools/ci/verify-ge8300-ui-client-imports.py --project-root <TAB-A04-BR3-root> --candidate <回収したexe> --out <root内import-proof.json>
```

recipeのsigned-manifestは`RT81_exe_imports_verified=false`を残す。
上記実import検証を終える前にbatch試験へ進めない。
このsource公開時点ではWindows ARM compile/link/署名・実機snapshotは未実施。
LinuxではWDK8.1を使ったARM32 syntax/sizeof/fieldoffset検証と、raw parserの最小host検証のみ。

## 一括試験での実行と回収

GPU368適用後、同じWindows boot内で既存のbatchから1回だけ起動する。
引数は、作成済みlog directory配下の**新規・絶対path prefix**を1つだけ渡す。
既存collectorへ以下の3ファイルとprocess exit codeを追加して回収する。
既存run197 profileやmain runner sourceをこのbranchから変更しない。

```
dxg-global-diag198.exe "C:\Windows\System32\<batch-log-directory>\global-diag368"
```

- `-response.bin`：ULONG lengthを含む全262148B response。APIが負statusでも保存。
- `-raw.bin`：API status0かつ返却lengthがcapacity内の場合だけ、そのlength分のrawを保存。
  failure時は空。空だからOS ringが空だったとは判定しない。
- `-records.txt`：enum/escape/close status、PID/session/FILETIME/uptime、選択adapter LUID、
  record境界、tag23全件のsequence/EventOrder/status、および最初の負tag23を記録。

全ファイルCREATE_NEWで、古い結果を上書きしない。出力予約に失敗したらescapeしない。
API再試行、adapter fallback、role/host/source操作、PnP操作はない。

parserは先頭から各recordのsizeで進み、壊れたbuffer内を再検索してstatusを拾わない。
保存RT8.1のtype6/bytes64/tag0x23だけをadapter-start returnとして解釈する。
これは最終adapter statusであり、最初の内部failureや対象GE8300のstatusとは限らない。
raw、同bootの他証拠、sequence/EventOrder/packed IDと照合する。
ring時刻はKeTickCount由来で、FILETIMEと同一単位・epochとして扱わない。

| exit | 意味 |
|---|---|
| 0 | snapshotとtag23解析完了。負tag23を含んでも証拠取得としては完了。GPU成功ではない |
| 1 | 引数/export/I/O等の失敗。最終process exitが記録末尾より優先 |
| 2 | enum失敗／有効adapter handleなし |
| 3 | Escape status非ゼロ |
| 4 | empty／tag23なし。初期化成功の証拠ではない |
| 5 | length不正／record不正／末尾がpartial |
| 6 | snapshot後のadapter close失敗 |
| 7 | 256K capacity到達。全ring取得と断定できない |

全状態でGPU_SUCCESS_INFERRED=0。NTSTATUSは符号を失わない32bit hexで保存する。
parser host検証は`tests/dxg_global_diag_format_test.c`で行い、API failureを実機で反復試験しない。
