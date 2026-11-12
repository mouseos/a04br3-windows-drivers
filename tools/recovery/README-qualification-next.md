# 実239から実241への第二WinPE資格確認

`qualify-unattended-winpe-next.py`は別entryです。旧`qualify-unattended-winpe.py`の237向けCLI・結果・once latchを変更しません。実239と同boot GPU/USB補足を明示pinし、更新なし資格確認ESPを一度stageして実241をRO回収します。実機・VM操作はmain専有です。この版は独立レビュー後に採用してください。

第一試験のhelper実行不可・query9020は不合格のままです。mainが失敗後に回収する実239は次試験のbaselineであり、第一試験成功の証拠にはしません。元run240準備は保持・実行禁止です。第二資格確認が合格し実241を受領した後のGPU工程はnative241/SCM241/run242/postcapture243です。実239や実241のhash/receiptを予測・改名しません。

## CLI

以下の変数はmainが実体から設定します。ROOTはTAB-A04-BR3の作業root、NEXTはこのcheckoutの絶対pathです。旧live checkoutやcontrollerのfileを変更しません。

```sh
python3 -B "$NEXT/tools/recovery/qualify-unattended-winpe-next.py" \
  --workspace "$ROOT" \
  --qualification-manifest "$SIGNED_QUALIFICATION_MANIFEST" --qualification-sha256 "$QUALIFICATION_SHA256" \
  --lease-manifest "$LEASE_MANIFEST" --lease-sha256 "$LEASE_SHA256" \
  --atomic-manifest "$SIGNED_ATOMIC_MANIFEST" --atomic-sha256 "$SIGNED_ATOMIC_SHA256" \
  --prior-manifest "$ROOT/port/xnu/out/private-windows-rt-prebackup-run239/manifest.json" --prior-sha256 "$FRESH239_SHA256" \
  --prior-supplement "$ROOT/port/xnu/out/private-windows-rt-prebackup-run239-gpu-assets/manifest.json" --prior-supplement-sha256 "$SUPP239_SHA256" \
  --prior-usb "$ROOT/port/xnu/out/private-windows-rt-prebackup-run239-usb1121-exe/manifest.json" --prior-usb-sha256 "$USB239_SHA256" \
  --out "$ROOT/port/gsi/logs/ios-feasibility/windows-rt-unattended-qualification239-next-runtime" \
  --validate-only
```

採用後にmainが`--validate-only`を外して一度だけ実行します。validate-onlyはhostの全入力照合だけで、device subprocess・out/lock/latch作成は行いません。実239未着やpin不一致なら拒否します。

## 入力と署名

primaryの全42files、SYSTEM trio、各backup path/size/hash、同239/Recovery bootのGPU/USB補足と全実fileを検査します。USB witnessの欠落は元manifestのmissing情報として維持し、存在に書き換えません。実行時には既capture verifyとRecovery.identity()/require_unmounted()で、実239取得時と同一serial/root/boot UUID/Windows geometryを再確認します。途中でWindowsへ戻ったsnapshotは使用できません。

qualification manifest/ESP/WIM、lease manifestと全実package、production atomic helperを照合します。Turingの独立署名gate commit `b2b04771`を親に含み、その`load_atomic_helper()`で実production EXE・署名report・BCBF公開CER・署名log・非署名payload不変を検査します。controller自身もCER/署名logを開始時pinへ含めます。署名済みEXEを含む新qualification WIM/ESPはmainが別途生成し、その実manifestを渡してください。署名を加えただけの旧ESPを新媒体と扱いません。署名gateの再実装やhelper Cの変更はありません。

serial共通flockは旧controllerと同じ名前を使用し、別outの並行実行を拒否します。qualification SHAと実239 SHAに対する共有once latchを、remote staging前にopen(x)/fsyncします。失敗しても削除しません。実241/補足/USB namespaceは開始時とdd直前にも検査します。ESP書込み前guardはawk不要のread-while方式です。Windows/boot/GPT更新は行いません。資格確認ESPの書込みとRecovery `/tmp` helper配置は行います。

## 時刻と合否

hostのreboot要求からfastboot確認までは2400秒上限です。合格には保守的なhost下限1790秒も要求し、1秒等の早すぎるresetを拒否します。boot/lease/reset後transportを合わせた生存監視と早期reset防止であり、kernelの1800秒をhost時計で直接測定する判定ではありません。旧1900秒上限は復活させません。下限を満たさなくても安全なtransport確認後のRO回収は行い、不合格を維持します。後続Recovery/root待機は別枠150秒、RO回収は各helper1800秒上限です。reboot/Recovery移行/root要求はそれぞれ一度だけで、timeout/非zeroのack不明を理由に再送しません。

CRCでquery0・期待1800秒・通常Windows更新要求0を確認し、同試験窓のlease compile marker、唯一のCRC終端、その後のdriver `A04RGU WinPE lease expired; SWRST requested`、その後の最初のBOOT_REASON=4を順序付きで要求します。別bootの理由4へ飛び越して合格させません。queryやCRCが不合格でも、product A04BR3のfastbootを確認できればRecoveryへ一度移行し、RO241を回収します。

対象3keyは値を含む全出現を数え、各keyが期待値で厳密に一度だけ現れることを要求します。query0と9020の併存、同じ成功値の重複、未知値、key空白や値欠落も不合格です。CRC整合だけで意味の矛盾を受理しません。

現署名helper Cの`--check-lease`はstdout markerとexit codeのみで、実IOCTLのelapsed high/lowは内部判定に使用するだけです。この版はCを変更せず、数値をreportへ出せるとは主張しません。resultでは`kernel_elapsed_low/high=null`、`kernel_duration_measured=false`を維持します。合格は署名済み1800秒runtime policy、実query、driver期限到達event、後続reset、Windows/補足file不変の統合検証を意味し、1800秒の測定精度を実証した意味ではありません。

driverはSWRSTレジスタ書込み後にexpired markerをTraceするため、resetが先に成立してmarkerが届かない可能性があります。その場合はRO回収しても資格確認は不合格に保ちます。host時間や別のboot理由だけで補いません。

## 回収結果

実241 primary/GPU/USBは同Recovery bootへ結合し、前後42filesだけでなくGPU/USB補足のpresent/missing集合と全size/hashも比較します。旧239/旧試験resultを上書きしません。

* exit0: `reset-recovery-and-capture241-complete`。上記の統合検証に合格。
* exit2: `capture241-complete-qualification-failed`。RO証拠は回収済み、資格確認不合格。
* 例外による非zero: `stopped-preserve-evidence`。部分出力・latchを保持し自動再実行しない。

いずれも`GPU_success=false`です。helperの署名受理、期限reset、実241回収完走はmainの実機検証が必要です。

## host検証

```sh
python3 -B -m unittest discover -s tests -p qualification_next_test.py -v
```

device subprocessを全置換し、query9020でもRO回収すること、expired/CRC/boot順序の拒否、host1秒・1790秒未満の不合格と1790秒境界、host2200秒の受理、実CRC decoderを使う矛盾key/重複値の拒否、各遷移要求の再送なし、旧証拠保持・既241拒否を確認します。実機・VMを呼びません。
