# GPU368 / 実fresh202

202は200生成器の入力設定だけを変更する。既200入口の既定動作、署名GPU368、transport357、FW、compiler375 cacheは保持する。

`prepare-peripheral-followup202.py` は、実capture202／native inventory／同Recovery補足／retained boot witnessをhashで結合する。実inventoryは45 services、191 named values、15 nodes。旧USB taskのStartは実値4、新202サービスは実不在。200サービスは保持対象に加え、再armもmarker削除もしない。201 strict postの失敗を成功へ書き換えない。

SCMは既 `build-combined200-service.ps1 -CaptureNumber 202 -Mode Service`、媒体は `build-private-peripheral-batch-media.py --combined202-profile` を使用する。既CaptureNumber既定は200。署名SCMは1TUのみ、新native SYSTEMは同固定sourceのprepare helperで生成する。GPU／SDK／PCOの再buildは不要。

launcher363／present356を実同bootから保持する。globaldiag198の既署名EXEを新202の絶対log prefixで呼ぶ。各clientは同bootで1回。canonical policy hashと、既generatorのgpu_client_count=1適応後runtime tokenは別々に保持・検証する。

boot witnessは既0943の保存履歴に実201 seal/controller/media manifestをhash結合する。`physical_read_this_capture=false`。物理bootの再読取りは行わない。

成果物生成・署名成功・実機GPU成功は別。202profileでの実hardware結果は未確認。
