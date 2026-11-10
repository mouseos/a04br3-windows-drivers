# 同一loadの保存GraphicsDrivers表示設定cache

基点は公開reader f638aa952df85416c2d32261ab195cdb5530a174。既`read-fresh200-native-inventory.ps1`へ`-IncludeGraphicsTopologyCache`を追加した。実post214を取得・通知された後の一回に含める。現時点で214manifest/hive/取得結果は作成していない。

新規Windows PowerShellプロセス内で、実取得済み入力だけを指定する。

```powershell
& "$src\tools\recovery\read-fresh200-native-inventory.ps1" `
 -WorkspaceRoot $work -Manifest $Actual214Manifest -ExpectedManifestSha256 $Actual214Sha256 `
 -SourceDirectory $Actual214SystemTrioDirectory -OutputDirectory $NewOutput `
 -PolicyPath $PinnedPolicyReference -ExpectedPolicyFileSha256 $PinnedPolicyReferenceSha256 `
 -TimingCaptureNumber 214 -IncludeWifiCheckpoint -IncludeGraphicsTopologyCache
```

`-IncludeWifiCheckpoint`の既契約どおり、trioは`files/Windows/System32/config`、同captureの3SYSは隣接Driversへ置く。既selected GPU/WiFi・StartGap359・CPは維持する。CCD追加はcallerの既loaded rootを読むだけで、追加loadやregistry書込みAPIはない。

追加出力は同receiptの`graphics_topology_cache`。対象は`ControlSet001\Control\GraphicsDrivers\Configuration`と`Connectivity`のみ。両rootを最初にbreadth-first queueへ入れ、root含む全128keys/depth8（root=0）を上限にする。pendingを含む総予定keysを数え、budget超過なら子名を列挙する前にbranchをskip。value namesも128を超えるkeyは値をskip。上限は固定で、外部引数による拡張や任意registry path指定はない。

内容を取得する値はmoduleの固定allowlistにあるREG_DWORDだけ。PrimSurfSize/ActiveSize/Positionのcx/cy、Rotation/Scaling/ScanlineOrdering/PixelFormat/Stride、SourceId/TargetId/PathActive/Flags、存在すればAdapterLuidのDWORD両半分とrefresh/VSyncのDWORD numerator/denominator。raw hex/uint32を保持しPositionのみint32も返す。非DWORDはname/kindだけ記録し、string/binary/QWORD内容は読まない。全registry dumpやネットワーク項目取得はしない。

recordには固定rootからのrelative key path、depth、present、許可DWORD、非DWORDのskip、上限理由とchildren_skippedを保存。上限で`complete=false/truncated=true`、statusはbounded-partial-cache-observationとなり、欠落を0や成功へ変換しない。read errorは親inventoryのerrorsへ伝え、cleanupを維持する。

これは保存cacheでありlive QueryDisplayConfig/GetDisplayConfigBufferSizesの代替ではない。複数過去構成を現在activeと見なさず、PathActive等も保存値のまま返す。BasicDisplay/GPU所属はkey名だけで推定しない。許可DWORDに実adapter IDがなく、既selected/StartGap等とも相関できない場合は所属不明。`live_CCD_queried/active_topology_verified/adapter_owner_verified`はfalseのまま。向き・寸法も保存値として報告し、実bootとの相関を別途要求する。

mock-only試験sourceはtests/graphics_topology_cache_readonly_test.ps1。case-insensitive名、負のPosition、非DWORD/非allowlist内容を読まないこと、両rootと128keys境界、過剰childrenを列挙しないこと、depth8境界を確認する。今回PowerShell実行環境へは接続しておらず、この試験や変更後readerのWindows実行は未実施。source diff checkのみ実施。実214のnative実行はmainの明示指示後だけ。
