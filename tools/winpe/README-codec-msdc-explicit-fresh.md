# codec1026 / MSDC quiet: main返却の最新freshへ明示結合

実機所有はmain。run88完成まではsource/draftのみ。サービス、実行可能header、native SYSTEM、媒体は生成しない。
旧run85 controllerはこのbranchでは無条件停止する。過去の署名service/媒体は再適用しない。

profileは`codec1026-msdc-quiet-explicit-fresh-v2`。
番号固定のpathを探す処理はない。mainが最新として返却したmanifestの明示path/full SHA256だけを使う。
呼出側による「最新」の指定を前提とし、ファイル名や更新日時から最新と推測しない。
既知のfresh82/84/86はpath/hashで拒否する。全file hash/bytes、captured-unmounted、read-only、errorsなしを照合。
さらに保持render3.12の実SYS hashが一致しなければ拒否するため、旧GPU3.10 captureへのfallbackはできない。

mapの必須field:
- schema=1、transaction=上記profile
- backup_manifest / backup_manifest_sha256（draftでは両方null）
- bindings: {path,sha256}（draftではnull）
- boot: {path,sha256}（mainのPOST boot artifact manifest）
- packages: capture、emmc、parent1015、render312。それぞれ {directory,manifest_sha256}。

GPU3.12保持sourceは9f31ddcf89fbe6d5c3a7b5abcceadb223212d378、SYSは
adf92be2b28f2b353eb0a247620cc23e8d9051841bea9aacbd2522ae08525d4b。
POST bootはsource e3b18c0ff1e35dfae54f00c54b6ae2203313c4d3、image SHA256は
9dc9f7f104280e8e9b9b51082de596035c173451c0b39229479a0196340882eb。
boot manifest/imageの出自を照合し、codec媒体からboot/GPTを書かない。artifact照合だけで実搭載bootを証明したとは扱わない。
実搭載boot/GPTの保持はmainの返却記録と、次に委任された試験の前後観測で個別に確認する。

bindingsは同manifestに結び付くnative replay結果。source_manifest_sha256とSYSTEM/LOG1/LOG2の
source_files {sha256,bytes}、load/unload/privilege restore=0、errors=[]が必要。
実HardwareIDを持つ親/Audio/eMMC/USB/WiFi/DOD/renderの7件を一意に照合する。
ClassGUID、Service、InfPath、DriverVersion、DriverDateDataを保持し、日時や選択版をSYS候補版から推定しない。
MSDCは配置SYSのsource f8f54c9（候補0.0.1.7）と、実選択INF0.0.1.0/date11-2-2026を分離する。
更新対象のbaseline版/dateはreceiptの実値をcompileし、保持4件も実date/版をruntimeで照合する。

`tools/recovery/read-codec-baseline-bindings.ps1`は既存native helperを再利用し、
返却済みhiveのprivate copyだけをreplayして7件を読む。実機へ接続せず、サービスや選択設定を書かない。
mainの従来3node receiptだけではeMMC/Audio等が不足するため、そのまま結合せずこの7件のreceiptを用意する。

通常結合時は`codec_msdc_transaction.py`にmap/workspace/outputに加え、
`--fresh-manifest` / `--fresh-manifest-sha256` / `--binding-receipt` / `--binding-receipt-sha256`を全て明示する。
mapとCLIが一文字でも異なれば拒否する。draftはこれらの引数と結合fieldを受け付けない。
WDK入口も`build-codec-msdc-explicit-fresh.ps1`へ変更し、同じ4つの明示値を必須にした。

run85の欠落DODコピーを拒否する事前検査、旧installed SYS保存、標準延期PnP、planned reboot1回、
親32flags/RTC borrow保持、ADC/VUL/IRQ2/PortCls/KS、MSDC error/init/checkpoint/DMA poll保持は引き継ぐ。
この変更でdriver sourceは変更しない。旧fresh84版READMEと署名候補は履歴であり新入口として使用しない。
