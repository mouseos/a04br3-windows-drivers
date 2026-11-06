# WinPE固定対象バックアップ・復旧の試作

`offline-backup.c` は更新プログラムではない。SYSTEM、SOFTWARE、DRIVERS、COMPONENTSとそれぞれの
LOG/LOG1/LOG2・TM関連ファイル、config/TxR内の対象ログ、TOPRGU・共有親・BU21025のSYS、およびoem33.inf・oem35.infを対象とする。DISM、Driver
Store、デバイス列挙・PnP設定、他の変更ファイルを復旧するものではない。
搭載Windowsへの適用には使わない。実機のNTFS整合性、EFI空き容量、電源断後の
FAT・NTFSとストレージドライバのflush動作、WinPE署名受理は未確認。

C: はdisk 0のoffset 469762048・length 15147204608のNTFS、E: はdisk 0の
offset 184549376・length 268435456のFAT32で、ブートファイルを持つ既存ESP
という固定条件とWindows側のdirty bitを確認する。dirty bitが無いことは
NTFS全体の整合性を証明しない。容量は `BackupRead` で代替ストリーム・セキュリティを含む各入力を一度計測し、
FATクラスタ境界に切り上げた合計に8 MiBを加えて判定する。保存時にも同じ
ストリームのハッシュを照合し、変化や容量不足があればREADYを作らない。既存の `E:\A04BACKUP` がある場合は上書きしない。完成前の残骸も
自動削除しない。圧縮・スパース・暗号化・オフライン・ハードリンク・再解析
ポイントの対象は拒否する。管理者権限とbackup、restore、securityの各特権が必要。

バックアップ時は、対象を排他的に開き `BackupRead` のバイト列を個別保存し、
SHA-256と属性・時刻を記録する。ファイルflush、読戻し検証、ESP flushの後に
`READY` を書き、そのflushと再読込み検証を行う。復旧時は全アーカイブと
`READY` を検証してから、`BackupWrite` を使って一時ファイルへ再現し置換する。
各対象を再読込み・比較し、途中で失敗した場合は `READY` を残す。復旧終了は
ボリュームのdismountやNTFS整合性を意味しない。

排他確認は一時的なものであり、他プロセスや未知の名前でロードされたhiveを
完全に排除する仕組みはない。実行環境側でアクセス停止と全hiveのunloadを
保証する必要がある。EFIとWindowsは同一eMMC上にあるため、媒体自体の故障に
対する独立バックアップにもならない。電源断に対する永続性は両ファイルシステム、
ストレージドライバ、eMMCのflush実装の実測が必要。

ARMバイナリは `tools/ci/build-offline-backup.ps1` でビルドする。Windows上の
合成テスト `tools/ci/test-offline-backup-synthetic.ps1` は対象とESPを一時
ディレクトリへ置き、identityとvolume flushを模擬する。これらは実機の安全性を
証明しない。限定されたhive更新経路は下記の通り接続済みだが、実機上の署名受理と
正常dismountは未検証である。

2026-11-06の保存済み統合v2 ESPイメージは `fsck.vfat -vn` の集計で
516190クラスタ中505685クラスタ使用、空き10505クラスタ（約5.13 MiB）だった。
実機の現在値は未測定で、このイメージから実機の空き容量を断定しない。
このバックアップ先を含むESPイメージの丸ごと再書込みはjournalを消すため、
復旧が不要と確定するまで行えない。dirty bitが立った場合は本試作は復旧前に
停止する。別途、NTFSを安全に扱う復旧手順が必要である。

未記録のTM・TxRファイルは、更新開始を示す有効なINTENTがない場合は削除せず停止する。INTENTがある場合は更新中の生成物とみなし削除するため、他プロセスによる同領域への書込みを防ぐ運用が必要であり、その排他は強制されない。
SYSTEM/SOFTWAREとTxRのファイル数は固定対象を含め99個までで、対応外の名前や
ディレクトリ、リンク、TxRディレクトリ欠落は拒否する。ディレクトリ自体の
属性・ACLはジャーナル対象ではない。WIM再起動後の `--probe` は保存先と対象
ボリュームの識別、ハッシュ、サイドカー一覧を検証し、変更は行わない。

小型ESPの派生生成は `build-maintenance-esp.py` で行う。生成物は原本WIMとESPを
別々に保存し、残したWIMレコードの内容・属性・セキュリティ記述子と、FAT内の
残存ファイルを照合する。言語資源、WinSxSのManifestCache、NVIDIA関連ファイル
などを除くため、起動・DISM・PnP・署名受理は改めて検証が必要である。保存済みOS原型には `SYSTEM{...}`・`SOFTWARE{...}` と `TxR` の関連ファイルが
存在する。これらを含む。実機上の起動・電源断復旧は未完成である。

`preserve-esp-journal.py` は、完成済みジャーナルを含むESPの保存済みイメージから、
別の新規候補イメージへ復旧ファイルを移し、READYと各itemのハッシュ、その他全
ファイルの保持を検査する。出力は通常ファイルのみで、実機を読み書きしない。
保存元イメージが実機の最新状態であること、取得から書込みまで他のプロセスが
変更しないこと、電源断中にも別の起動経路が残ることはこの処理だけでは保証できない。
よって検査済みイメージだけを根拠に実機ESPを上書きしない。

`flash-esp.py`、`flash-efi.sh`、`restore-toprgu-diagnostic.sh --execute` は、
recovery上でESPを一時的に読取り専用マウントし、`A04BACKUP` がないと確認できた
場合、またはREADYのハッシュと一致するCOMMITを確認できた場合に全体書込みへ進む。未完了・状態不明の場合やマウント・確認に失敗した場合は停止する。書込みの直前にも再確認するが、
他プロセスによる確認後の変更や電源断を防ぐ排他制御ではない。別の書込み手段は
このガードの対象外。`--verify-only` はローカル入力の照合だけを行う。

現在のWinPEツールはシリアルを含む形式3・4・5を再生する。形式4は固定スロットにoem33.inf・oem35.infを含み、新規の形式5はDRIVERS・COMPONENTSの本体・LOG・LOG1・LOG2およびそれらのTMファイルを追加する。存在しないhiveは不在として記録する。既存形式3・4は従来のスロット解釈で復旧する。過去の形式1・2は
対象ボリュームとの結び付きがないため自動再生せず、保全したまま拒否する。
`preserve-esp-journal.py` は形式1〜5のファイルを別イメージへ保全できるが、
保全成功は旧形式を現在の実機へ安全に復旧できる証拠ではない。

## 限定hive更新トランザクション

`apply-offline-hives.cmd` は事前照合、バックアップ、INTENT記録、SYSTEMの既存TOPRGUサービスのStart変更とSOFTWAREへの証明書登録、C:のflush・dismount・解除、COMMIT記録を順に実行する。更新失敗時はバックアップを再生してから解除し、失敗時はジャーナルを保持する。3つのSYSのハッシュが指定値と一致し、既存サービスが条件を満たす場合だけ実行する。SYS・INF・CATの導入、PnP構成、実機の署名受理は行わない。Windows合成試験のボリュームidentity、flush、解除は模擬であり、停電後のdirty NTFSは復旧前に拒否する。COMMITの直接実行はC:不在を検査するが、別手段で解除された場合のflush・dismountを証明しない。

2026-11-06の監査では共有SYSが未存在だった。復旧時、INTENTがないジャーナルに対して元は不在だった固定対象が出現している場合は、一切の再生に先立って拒否する。INTENTが検証された場合だけ、更新中に作成された対象として不在状態に戻す。この判定は他プロセスとの排他やDriver Store・PnP全体の復旧を保証しない。

`offline-baseline-guard.c` は対象のNTFS範囲・シリアル・dirty bit、既存TOPRGU/BU21025 SYS、oem33/oem35 INFのハッシュと共有SYS不在を照合する。各ファイルの読取中は排他的に開くが、対象全体のスナップショットやPnP、CAT、署名受理、後続処理までの排他を証明しない。`offline-signature-check.c` は候補のTOPRGU・共有親・BU21025の3 SYSについてWinPE上の一般署名検証を試し、失敗は拒否し、全て成功した場合もRT側ポリシーが未証明と報告する。Driver Storeの変更範囲・回復と搭載RTの署名受理は引き続き未実装で、この部品を更新許可として使わない。

`prepare-update-candidates.py` は実績WinPE WIMから共有親・タッチのINF/SYS/CATを抽出し、同じWIMのTOPRGU SYSと740117c署名パッケージを照合する。TOPRGU、共有親、タッチの署名者は別々である。`verify-transaction-candidates.ps1` はWDK signtoolで各INFとSYSのCAT所属を記録し、自己署名ルートが信頼されない場合を実機受理と混同しない。`prepare-transaction-trust.py` は固定ハッシュと所属記録を照合し、3組の証明書・SYSピンを生成する。`offline-hive-apply.exe --payload-check` はX:上の候補SYSと3証明書のピンを照合し、元SYSが未交換の状態でも使用できる。PnP・Driver Storeの更新や完全な復旧が揃ったことは意味しない。

`offline-pnp-baseline.exe` は呼出側がRAMコピーとしてロードした `HKLM\\A04AUDITSYS` のControlSet、TOPRGUサービス、MOU0004/MOU0005の選択INFと共有サービス不在を検査する。元hiveを自らロードしない。RAMコピーの排他・最新性は呼出側が確認する。`offline-signature-check.exe` は候補3 SYSをWinPEの一般署名ポリシーで検査し、成功時はRT側CI受理を未確認と明示する。ユーザーが指定した実機試験に向けた事前検査であり、実機受理を成功扱いしない。

`preflight-offline-update.cmd` は基準SYS・INFの照合、hiveのRAMコピー、PnP参照の確認とコピーのunload、候補3 SYS・証明書ピンとWinPE署名の確認を順に実行する。どの段階も搭載OSへの適用を行わない。PnP検査が失敗してもRAM hiveのunloadを試み、unload失敗を独立して報告する。成功は非原子的なRAMコピー、Driver Store全体の復旧、RT署名受理、OS更新の成功を意味しない。

`offline-stage-drivers.exe` は、有効なINTENT、現状基準、RAM hiveのPnP基準、候補SYS・証明書ピンおよびWinPE署名の各ガードが成功した場合に限り、3 SYSを固定の `.a04restore` 一時パスへ新規作成する。候補の読取り中に排他し、コピー前後のSHA-256、各ファイルとC:のflushを確認する。既存SYSは置換しない。一時配置途中の停止後は、INTENTを検証した復旧処理がこれらのパスを取り除き、元ファイル・不在状態を再生する。RAM hiveのロードとunloadは呼出側の責任である。これは完成したDriver Store/PnP適用経路ではなく、更新やOS起動の成功を意味しない。

`offline-publish-drivers.exe` は有効なINTENT・基準・RAM hiveのPnP・候補ピン・WinPE署名を確認し、事前に配置した3個の一時SYSをすべてハッシュ検証してから、TOPRGU、共有親、BU21025の順で固定SYSパスへ反映する。既存2ファイルについて旧セキュリティ記述子を再設定し、制御フラグとSDDLを読戻し照合する。各反映後にC:をflushし、ハッシュを検証する。途中失敗・停止時はジャーナルから3 SYSと一時パスを復旧する。共有親の元状態は不在である。PnP・INF/CAT・Driver Storeの更新、コミット、dismountはこの部品に含まれない。Windows合成試験で各反映後の中断・復旧を行ったが、WinPE実機でのACL、flush、電源断後の復旧は未検証。単独で搭載OSへ適用しない。

保護DACLの一部では、Windowsの置換・再設定後に自動継承の制御フラグが元と一致しなかった。この場合は`DRIVER_PUBLISH_SECURITY_CHANGED`を出して失敗し、ジャーナル再生が必要となる。保護DACLを含む合成対象では元のSYSとACLへ戻ることを確認したが、実機のACLと停電後のNTFSは未確認。
