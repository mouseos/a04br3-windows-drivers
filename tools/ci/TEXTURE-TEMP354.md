# GPU354 SAMPLE temporary-register contract

run173/PID2612の同boot/module353、ticket(slot0/gen9/fence12)、cookie3112824852はR52 graph成功→R53 coldprepare C00000BB→R54 phase0、R55返却flags4/submitbit無し。graph失敗やtexture PT不成立と断定しない。

実R64..68のspan長/hash/witnessとR69の両desc hashは固定tokenized入力再現と一致する。しかしR69 state/shader metadataは実1116225057、旧Linux host再現2070910439で異なる。UMDの状態/各spanはtyped_draw.cで決まり、VS witness検証でVS metadataは拘束され、SAMPLE producerはPSのshareds12/constant_mask1/他0を生成する。PS tempsだけが旧再現0であり、6にするとR56..69全fieldとcookieが実recordへ一致する。これはsource再構成でありraw kernel packet回収とは呼ばない。FNVは認証/一意性証明ではない。

独立根拠: 同固定tokenized shaderを既PCO compiler cacheで実生成し、adapter TUにsource IR printだけを追加すると、同88-byte USCのENTRY main temps6、register r0..r5、shader stats temp count6を得る。compiler本体再build無し。一次source third_party/mesa-pco/mesa/src/imagination/pco/pco_ra.cはfunc->tempsからshader->data.common.tempsの最大値を報告し、compiler/pco_texture2d.cはこの値をUMDへ返す。旧Linux adapterの報告0は実IR6と矛盾し、RT target metadata資格として使えない。そのhost cache/adapterの不一致原因自体は未確定で、Windows producerが不正なcodeを作ったとは主張しない。

native_texture2d_program.cの`temps==0`必須、native_draw_job.cのsampled/combined重複guardが、実PS temps6を正確にC00000BBへ拒否する。Windows coldprepareはこのreal native builder帰還をそのままR53へ返し、その時点は冷状態code/device-memory write前。graphのprecondition不足ならINVALID_DEVICE_STATE、無効VA aliasならINVALID_PARAMETERであり、今回のBBと混同しない。

354は既88-byte SAMPLE profileに必要なtemps6を単一header定数で厳密に要求する。0/1..5/7以上は受理しない。bytecode88/token/constant span/shareds/sampler/witness/owner/alias/retirement/IRQ制約は削除しない。提出されたコードのみをコピーし、既PDS builderへ正しい必要temp数6を渡す。UMD/FW/client/engine/owner layoutは変更しない。353のIRQ continuation修正は保持。

tests/test-ge8300-texture354.pyは固定client由来blobと実metadata一致のsource再構成を使用し、旧353のBB負対照、新354のreal native builder通過、および88 byte改変/不正temp bounds/alias/truncationの拒否をASan/UBSanで検証する。PCO生成headerは既host buildから参照するだけでcompilerを再buildしない。実PT/graph allocation・device-memory readback/FWstart/submit/IRQ/画素成功はhost testの対象外で、実機はmainの次通常batchで評価する。9F未解決。
