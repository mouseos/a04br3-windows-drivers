# main向け初回flat試験引継ぎ（e837）

## 結論・blocking

提示された **e83792d8 + H0f8/auto6303 + main署名parent1014/0498bd9 + 機種boot6e852b1** で、初回の限定標準DXGI flat試験へ進める。E側の追加blockingはない。追加のGPU小試験、再build、全COLOR実装、全FL9_1適合を初回試験の前提にしない。

これは試験準備の完了であり、標準APIでの実GPU描画成功を確認した記録ではない。媒体作成・実機操作・署名親packageの採用判断はmainが所有する。

## 採用する固定tuple

|要素|固定出自・採用条件|
|---|---|
|Native owner / producer|`e83792d88485883130e137309b0ef027b3c7bc5e`。D3D typed viewport修正、実allocation/MDL/queue/IRQ/lifetime、正常停止経路を含む|
|実KMD archive|H `693858095b1511ff8abc66782cc91767d33bc56f`、SHA256 `771f332253a19fdae4593e569274b71cac96e4035fc7b301fa7c0034e26d1deb`|
|未署名SYS|`fce6b9885c567346aa9b7cea3757c9a50ad321f309771a0a130eab8d3c4cc0b5`。Eの最終DriverEntry SYS link・回収hash照合済み|
|H署名package|`0f8f68d252820443b988b93ae5c2448fdc9a7d00`。typed runtime `3f17e2f2`、Nash `16650913`統合。Hによる署名・CAT membership・回収hash確認済み|
|自動実行/媒体側|**main採用auto6303**。採用hash/実行設定はその正本manifestに従う。Eはこれを変更・再buildしない|
|GPU INF|H0f8の `11/06/2026,0.0.3.1`、MOU0006を排他所有。同版の旧候補がstoreに残る場合も、選択INFと今回の実SYS/DLL hashをmainで照合する|
|親binary source|`287241c37fa0eae538e5e2d136dea5df42de7e43`、`2026.11.7.1014` **WithSpm + SD1**。SD1010全effective flags/audio DMA forwardingを保持|
|親署名の正本|**main `0498bd9` 固定packageを優先**。正本manifestの署名後SYS/INF/CAT hashを媒体へ固定。Eの別署名結果/ad3c247fを混入させず、新たな署名作業をしない|
|機種ACPI/boot|`6e852b1ff13e16609957afc7a2e03da3d371dfe8`。Carver生成のmatched機種boot。共通EDK非変更|
|boot SHA256|`6dca720a68c8f62667ffec70669eb1326a0ca0145a6088313b96bcf89f2f93b4`|
|DOD|main署名済み **1.6 CPU**。MOU0007 CPU presentを維持し、KMDF GPU IRQ ownerを同時に動かさない|
|FW SHA256|`5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1`|

H0f8署名後の固定hash:

|現物|SHA256|
|---|---|
|GPU SYS|`37d3d7a656e99c48e5a5b041479f9ebf44593ffb073ea019660b1729d9d098a6`|
|UMD DLL|`3ebcbbcab37bb2b5e77e7861febb8a2bd63c9152e22fb6f67e6f09594462f64a`|
|標準client|`3055dd6bf2b5bd887c3d967c65ee8d496c63e69fc994f509069d18f6e94bac2f`|
|GPU CAT|`a9ce6e0cbd3291e8c985ea15faaa02f609e4093428f247272ca68b3fba8b941f`|

auto6303がclient等を更新した部分は、そのmain正本manifestを優先し、H0f8の旧ファイルhashと混同しない。auto6303およびmain0498bd9の全hashをEが別途再検証したとは主張しない。

## 初回の実行・判定

- mainだけがmatched tupleを適用・起動する。rootRecovery/Windows未mountからの媒体・実機工程はmain/Ramanの既存手順に従う。Eは端末へアクセスしない。カメラなし。
- 実DXGI inventory/PnPで確認したMOU0006のLUIDを指定する。first-adapter/WARP/REF fallbackを使わない。手動形式は `ge8300-d3d-clear.exe --luid=HHHHHHHH:LLLLLLLL`。auto6303では同じ実LUID選択契約を維持する。
- 標準APIの一連の操作はclear/readback→POSITION4 VS/constant PS triangle→CopyResource/Flush/blocking Map/readback。引数なしinventoryだけの非0終了は、描画成功/失敗の試行と区別する。
- 成功はclient exit0と `API_READBACK_SUCCESS=1` および実clear/triangle readback判定。DRAW_ISSUED、compile/link/署名、model検査だけではGPU成功にしない。
- `C:\MT8167D3DClear\clear-*.log` / `umd-*.log`、UART、HRESULT、画素集計・hash、選択INFと配置SYS/DLL/FW hashを回収する。
- FW ACK/IRQ完了/正常停止等の失敗時は既存poison・DMA/MDL保持を維持する。未知NoDMAを成功に置換しない。追加の未完成packetや強制kickで迂回しない。

## 完成範囲と別途継続するもの

初回flat準備は上記の署名済みstandard client/UMD・実SYS link・matched親/bootまで揃っている。全FL9_1出荷適合、一般アプリ互換、TDR等の未qualified項目は保持するが、ユーザー指定の既定OFF限定開発試験を止める追加gateにはしない。

COLORは別途継続する。E `1ddfccb6` でHの公式FL9_1実SM4 token→有限decode→NIR→pair-link→PCOまで実行し、VS88/FS80・metadataがe837の受理profileへ一致。残りはH最終compilerへの統合、2elements/96byte vertexおよびV2 frontend transport。**これを初回flat候補へ混入させず、完了待ちでmainを止めない。**

親版台帳: 現1010、GPU1014/SD1、1016 withdrawn再利用不可、1018thermal/SD0専用、1020mic予約、1022 GPU+thermal統合予約。1022/thermal変更は今回の1014/6e tupleへ入れない。

## 現物・証拠の保存先

作業root配下:

- `port/xnu/out/windows-rt-wddm-typed-signed-0f8f68d2/`（signed/linked/client/recovery manifest）
- `port/xnu/out/windows-rt-native-factory-e83792d8/combined/`（最終未署名SYS、combined-manifest.json）
- `port/xnu/out/windows-rt-native-factory-e83792d8/HOST-VERIFIED.json`
- `port/gsi/logs/ios-feasibility/windows-rt-native-factory-e83792d8-build.log`
- mainのauto6303/0498bd9正本manifest・媒体検証記録

本書はsource引継ぎ。生ログやbinaryはsource repoへ混入させない。
