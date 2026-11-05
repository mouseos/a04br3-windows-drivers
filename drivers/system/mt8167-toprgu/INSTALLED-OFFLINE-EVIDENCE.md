# 搭載WindowsのTOPRGU導入に関する過去証拠

この記録は過去の取得物に限る。現行のディスク、証明書、BCD、実行中WinPEの状態を示さず、導入の許可条件として使用しない。実機操作は主担当が行う。

- 過去に採取された搭載SYSはSHA-256 `6fc56829bdc93489a0888a395fa6f8736c4ff34ccaa35162bb6d940d20695fc1`。旧版パッケージと一致。候補のWinPEテスト署名SYSは `9ca96c1f54964a132578a00e2362795c789ac627d4e925df69aa8bc17b28e7fa` で別物であり、搭載OSでの受理は未確認。
- 過去SYSTEM解析ではCurrent=1、サービスStart=0、System Bus Extender、ImagePath=System32\\drivers\\mt8167-toprgu.sys、ACPI\\MOU0005\\0にバインド、PnP INFはoem35.inf。DriverStoreの別登録を現在のバインドと混同しない。
- 過去SOFTWAREはREGF 1.5、sequence 1175/1174。コピーのベースで候補証明書は未発見だが、LOG1/LOG2をregipy 6.5で再生するとhive外を指すというエラーで止まった。実効的な信頼状態やログ破損を断定できない。Windows RT 8.1 6.3 build 9600.16452はこの過去コピーの情報。
- `28b5bdb43e4305e27c2e13192f248ba4616af0e00863e6b4d73eb32ad0122104` のBCDは過去の実験でESPに適用された候補。testsigning有効の記録はあるが、その後の更新の有無は不明で、現行BCDの証明にはならない。WinPEのBCDとも区別する。
- 過去WinPEではtestsigning下の自己署名ドライバがDriverEntryに達した。新SYSと搭載Windowsにおける署名・PnP開始・CPU1引継ぎは未確認。

`tools/recovery/report-toprgu-snapshots.py` はルート内の過去ファイルを読み、SHA-256とREGFヘッダーをJSONに報告する。ハイブやログを再生せず、入力の変更も行わない。ヘッダーのシーケンス一致はログ整合や読み込み可能性の証明ではない。

## 更新と復元の前提

現行SYSTEM、SOFTWAREと対応ログ、搭載Windows側BCD、現行SYS、対象ボリュームIDの一貫した取得と、Windowsでのログ再生・署名ポリシーの確認が必要。過去の断片ハイブはDriverStoreその他のDISM変更を復元できない。Windows領域全体の検証済みバックアップ、対象端末とパーティションの同一性、更新中のwatchdogと他のリセット元の排他、完了とアンマウントの信頼できる報告、および復旧環境での復元が必要。更新後の別の正当な書込みを全領域復元で巻き戻す危険があり、対象期間の排他も必要。

これらの条件を満たすバックアップ・導入・全領域ロールバック経路はまだ実装・検証されていない。既存のDISM無条件拒否を解除せず、旧 `/ForceUnsigned` スクリプトを安全な代替手段として使わない。タイムアウト、結果欠落、DISM実行中または状態不明でreset・復元しない。現在動作中のWinPEでボリュームが解放されたとは扱わない。
