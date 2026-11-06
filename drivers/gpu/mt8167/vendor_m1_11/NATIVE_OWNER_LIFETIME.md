# Einstein向け: Windows native ownerのlifetime契約

対象: 主担当取り込みa5b0295（元885d38a）のprivate native backing。
本メモはソース上の所有権契約であり、描画・実機成功の証拠ではない。
完全TA/3D producerとqueue公開は依然未実装・禁止。既定OFFの
`A04_GE8300_VENDOR_NATIVE_BACKING`はcold backingの準備だけを許可する。

## 状態境界

| 状態 | 許される操作 | 保持・解放 |
| --- | --- | --- |
| staged、`hardware_owned=FALSE`、正常cold owner | 所有DMAへのknown factory stage、未使用entryの個別公開 | cold releaseの全検査・unmapが成功した場合だけ解放 |
| cold準備/cleanup失敗 | 所有権を保持してcleanup再試行 | 失敗が返っただけでは解放済みとは限らない。返されたownerを保存 |
| `quarantined=TRUE` | 失敗報告 | aliasのため全native DMAと親を保持。coldでもFreeCommonBuffer禁止 |
| `windows_start`がprocessor releaseを試行 | cold API禁止 | 実行結果を待つ前に`hardware_owned=TRUE`、`attempted=TRUE`を立てる |
| FW開始失敗、正常稼働、cleanup/forced idle/off後 | runtime ownerが既存transportを管理 | いずれも`hardware_owned`を維持し、native DMAを解放しない |

`windows_start.c::a04_vendor_m1_11_windows_start`は
`a04_rgx_execute_start`の**前**に`m->hardware_owned=TRUE`を立てる。
processor releaseを試した時点でGPU/FWへ露出した可能性があり、開始失敗をcoldへ戻す根拠にはしない。
参照ソースのvendor実装で、このフラグへFALSEを再代入する経路はない。
`windows_stop.c`の成功もフラグを解除しない。

`GpuRetireToCpu`はcontext cleanup→forced idle/offの成功時にCPU fallbackを許可するが、
ソースコメントどおりDMA releaseや新しいFW startの許可ではない。
D0Exit→再D0EntryのCPU復元をnative backing再確保・再stageの合図にしない。

## 所有者と順序

- providerは`vendor_lifetime`で既存操作を直列化し、`c->vendor_native`を保持する。
  packet/factory担当はこのlock、PnP lifecycle、DMA adapterの所有者を迂回しない。
- `native->render`と`render->native_owner`が相互所有関係。
  `render->firmware`と`firmware->render_owner`が親FWへの保持関係。
  nativeが残ればrender releaseはBUSY、renderが残ればFW freeはBUSY。
  providerのadapter返却はFW adapterが解放された後だけである。
- native cold releaseは、専用output PC entry→予約USC/PDS PTE→General arena PD entry→
  native FW bank/UFO初期化の順に取り消し、成功後にだけnative DMA・ownerを解放する。
  各mappingは自分の期待entry又は0だけを受け入れ、消去をreadbackする。
  他PC entry、他PTE、全catalogue snapshot、既存TQ code/payloadを復元・上書きしない。
- 公開前に所有bitを立てる。途中失敗でもmappingが一部公開された可能性を失わない。
  cleanupの失敗時は`native->render`と返却ownerを残し、親の破棄をブロックする。
  `hardware_owned=FALSE`だけを解放可能条件にしない。

## factory担当へ渡す資源

| 資源 | VA・容量 | 現実装での扱い |
| --- | --- | --- |
| private output | 0x8200000000、800x1280、pitch3200、4096000 bytes | 同render FWmemcontextのPC index520、2PT/1000ページ |
| USC / PDS | 0xba00010000 / 0xa800010000、各4KiB | 既知codeの有効32 / 0xa8 bytesをcold stage |
| 終端page | 0x65c0358000、4KiB | General arena offset0x158000、VDM+0/PPP+64の76-byte suffixだけ |
| binding | `native->clear_surface` / `native->bindings` | private CPU metadata。未確定recordへpatchしていない |

General arenaは0x159000 bytes。`a04_vendor_native_bytes`はこのarena量だけを返し、
output・tables等を含む総DMA量0x547000とは異なる。
`native->backing_staged=TRUE`はknown backingのstage完了であり、
完全packet、PR/通常background選択、CCB/KCCB公開、実描画完了の証明ではない。

現cold stageはFW開始前だけである。後から完全producerが到着しても、
`hardware_owned`を一時的に落としてcold stage/releaseを通してはならない。
完全資源・recordは最初のFW start前にownerへ渡すか、別途検証されたlive更新経路が必要。
現実装にはそのlive経路はない。単なるstub成功や空packetを公開しない。

## A5 sentinelと将来の証拠

主担当のsource修正は**output pixelだけ**を0xa5a5a5a5で初期化し、table shadowはzeroを維持する。
black clearのpacked colour0と全1024000画素の初期値を区別するため、
backing試験の初期画素確認もA5へ変更する。旧zero初期化sourceでは同試験が失敗する。

将来の完全producer試験では、初期A5保持の確認、実native TA/3D公開と各completion、
正しいDevice-memoryアクセス/orderingでの全画素比較を区別して記録する。
A5→期待blackへの変化は未実行でも一致する問題を除くための必要条件であり、
それだけでどのproducerが書いたか、TA/3D cleanup完了、DMA解放安全を証明しない。
ホスト試験でGPU描画成功とはしない。

## 検証・統合

本差分はa5b0295基点の**test期待値変更と本メモのみ**。
output A5 source変更は主担当の差分を使用し、こちらのcommitへ重複して含めない。
主担当のA5 source snapshotでownerホスト試験・ASan/UBSanを確認し、
旧zero sourceをnegative controlとして初期画素assertの失敗を確認する。
WDK build/実機/runner/REAは主担当の工程。本作業では操作しない。
