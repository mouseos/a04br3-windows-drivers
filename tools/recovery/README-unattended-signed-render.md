# 署名renderのWinPE lease回収入口

`run-unattended-signed-render.py`は次run専用の入口です。既engineとcapture helperを変更せず、同じ引数を受け取ります。

```text
python3 -B tools/recovery/run-unattended-signed-render.py \
  --signed-render-capture N --workspace ROOT \
  --seal SEAL --seal-sha256 SHA256 --output OUTPUT --validate-only
```

実行時は`--validate-only`を外します。このwrapperと同じcapture先を使うwatcherを同時起動しないでください。既runtime、primary capture、`-gpu-assets`補足先があれば拒否します。既存captureを改名して再利用しません。

書込み前に元sealのSHAと全pinを確認し、既engineの`validate_seal()`で既admissionを維持します。pinned media manifestの`winpe_lease`は`WINPE_LEASE_EMBEDDED_V1`、1800秒、`force_swrst=true`、通常Windows無変更を必須とします。lease package manifest・全4outputs・lease loader・このwrapperもsealにpinしてください。`tools/winpe/winpe_lease_package.py.load_package()`で実SYSのbounded marker、署名報告、SYS/INF/CAT/CER byteを検証し、embedded SYS hashとsourceを照合します。sealにも`winpe_lease`がある場合はmedia記載と完全一致を要求します。新wrapper・lease loader・補足helperは開始時hashも記録し、待機後に元pinと共に再確認します。最終WIM内byteの抽出照合はmedia生成側の責務であり、wrapperはそのmanifestとWIMのseal hashを確認します。

`--validate-only`はdevice subprocessを実行せず、出力dirも作りません。実行時は既engineをsubprocessで一度だけ起動します。engineの通常動作には既契約どおりESP更新・通常起動等があります。wrapperが追加する失敗回収処理はboot/GPT/ESPを書き換えず、UART reset・通常起動・再flash・engine再実行もしません。

正常診断後の自動回収も必須です。実capture Nは237以降、policyに`unattended_collection_reboot={schema: SIGNED_RENDER_UNATTENDED_COLLECTION_V1, capture: N, bounded_winpe_required: true, require_atomic_publication: true}`と既`collection_reboot.CONFIG`、sealにも同CONFIGが必要です。`collection_reboot.py`もsealにpinしてください。既`admit()`とengine側の署名service compile/source検証を維持し、engine起動時に`--collection-reboot`を付けます。SCM packageはmain側で`-CollectionReboot`付きcompileが必要です。

engine rc0でも、固定次capture番号に対応する`captureN-and-diagnostics-ready-for-main-review`、同transaction/seal、collection移行証拠、primary完了manifestと診断indexのhash/unmount、各回収fileの実byteを確認します。`terminal-windows-held`や欠けた出力は未完了として非zeroで停止し、無条件に正常終了しません。このrc0検証はhostの保存fileだけを読み、追加device操作を行いません。

有期限mediaには`atomic_publication.schema=WINPE_ATOMIC_PUBLICATION_V1`も必須です。payload copy件数は1以上、SYSTEM publication件数は1、`deadline_query_required=true`、`hardware_verified=false`を要求します。atomic manifestとproduction EXEもsealにpinしてください。`load_atomic_helper()`で実production byteとlease-query marker等を検証し、`EXE_sha256`と`media.files['atomic-stage-replace.exe']`を同じ実EXEへ結合します。この検証は待機後にも繰り返します。全payloadのatomic helper化と実running kernelの期限・所有照会、残120秒未満での開始拒否はmedia生成側・production helper側の責務です。host検証を実kernelでのlease稼働成功とは扱いません。

非zero終了かつ元resultが同transaction/sealの`pipeline-stopped`で、`prior_status`が`prepare-observing`または`prepared-volume-released`の場合だけ回収へ進みます。boot/GPT書込みやboot書込み試行がある結果は拒否します。CRC不明でも新しいROcaptureは許容しますが、no-change・commit・GPU成功を推測しません。

engine終了後、最大1900秒で同serialのfastbootまたはRecovery/rootを待ちます。fastbootではproduct `A04BR3`を確認し、`oem reboot-recovery`を一度だけ要求します。timeout/非zero応答でも再送せずRecoveryを観測します。直接Recoveryではserial照合後にrootを一度だけ要求し、既backendの`Recovery.identity()`によるserial/root/boot UUID/固定partition geometryと`require_unmounted()`を必須にします。一時的なprobe timeoutは待機上限まで再観測します。

Recoveryのidentity・未mountを既helperで検証し、sealの固定`capture_output`へprimary、同名`-gpu-assets`へGPU補足をRO回収します。補足は`--gpu-runtime-assets --include-system-dxgi`を使用し、同Recovery boot・SYSTEM anchor・snapshot・RO mount・unmount検証は既helperに委譲します。必要なntfs helperはRecoveryの`/tmp`へ配置します。primaryと補足の転送時間は1900秒のtransport待機とは別枠です。

wrapperのstateとログは`OUTPUT/unattended-recovery/`です。元sealとengine resultを上書きせず、元resultのhashとengine exit codeを保持します。回収完了時のwrapper終了code 0は証拠回収の完了だけを示し、engine/GPUの成功を示しません。stateの`mutation_outcome`は`unknown`のままです。部分captureや失敗ログを残した場合は再実行せず、mainが確認してください。

1800秒SWRSTの実機到達、復帰、回収完走は別途実機確認が必要です。旧稼働mediaへこの入口だけでleaseを追加する機能はありません。
