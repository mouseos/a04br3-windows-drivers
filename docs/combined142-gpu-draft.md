# fresh142 次GPU媒体の雛形（未arm）

run141のsource38a5a8ea・媒体・259pinsはfreezeし、本branch/worktreeでのみ次profileを準備する。実機owner main、worker実機操作なし。fresh142、run141機能別結果、Huygens/Mencius最終結合GPU署名物は未受領であり、この雛形はarmもbuildも行わない。

予定service MT8167CombinedFollowup142。renderだけ再DIF、親1115/audio1033/HCD1014の実採用状態・boot1fefを新captureで照合して保持する。予定restart1、最終GPU診断は1回だけ最後。周辺clientはrun141の機能別証拠から必要分だけ選定し、現時点では空配列。旧335clientを代用実行しない。

`tools/recovery/prepare-peripheral-followup142.py --workspace ROOT --out ROOT/port/xnu/out/NEW-DRAFT`は非armのmap/validationだけを出力する。--finalizeとload_map/generate/assert_pathsは常に拒否する。build/mediaの既存CLIにはこのdraftを接続しない。header/EXE/nativeSYSTEM/media生成なし。値を埋めただけでarm可能にはならず、受領後に最終契約とvalidatorを明示実装する。

次に必要な証拠はfresh142の全files/hash/size/Recovery boot ID・新native読取り・前snapshotとの比較、run141の機能別結果、最終GPU package/source/client/ABIとmain seal、最新GPUlogsの実baseline。旧hiveを新captureとして再利用せず、actual stateに従う。更新driver runtime出力と静的保持対象を区別し、WDF時刻差・HP未接続・PCrole・ObservedResultを機能成否へ一括変換しない。

巨大転送やSDK/WDK/PCO再setupは行わない。新host上の保存済みclosureをhash/size照合し、必要な新署名package・receiptと変更SCM TUだけ小deltaで扱う。今回SSH処理も実行しない。
