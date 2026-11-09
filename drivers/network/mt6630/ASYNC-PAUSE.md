# MT6630 shared-worker Pause completion

NDIS6.0以降の標準MiniportPause/NdisMPauseComplete契約に従うsource候補。
provider1026/source99ef927dのRuntimePauseは、NDIS dataと独立BT broker/OIDが共有する
idleイベントを無期限に待つ。pausedでも実HCI fileがあればpoll_dpcはworkerを投入する。
PnP callbackをこの制御処理全体のidleに同期させる必要はなく、NDISデータ排出後に
Pauseを完了し、BT/OIDを継続する必要がある。

本差分はRuntimePauseをPENDING/SUCCESSへ分け、既存spin lock内で排出を確認する。
最後のworker/API退場で、busy/API refs/send/send queueがすべて空の時だけ完了をclaim。
NdisMPauseCompleteはlock外、以後adapter contextを触らない位置から一回だけ呼ぶ。
RXは既存の同期RESOURCES indicationでworker終了前に返る。新sendは従来通りPAUSED拒否、
paused中に新data RXをindicateしない既存guardを保つ。

構造体は変更しない。既存paused fieldの0を実行、1を停止、2を「停止・完了待ち」に使う。
他TU/既存snapshotはtruthyとして読むので、field offset/sizeof/公開schemaの変更なし。
2から1へのclaimは同一lock内で行い、並行退場・追加BT pollで二重完了を作らない。
現行RestartはNDISのPaused後に呼ばれ0へ戻す。新work item/worker allocationはない。
HCI file/SCO電源保持、物理epoch、reset/電源OFFの拒否、HFP/PCM資格を変更しない。

run167は更新3010/flush0/SCMstop後の9F/3であり、GPU350未ロード。
PDO/IRP値からownerを特定できず、当該Pauseに実IRPが滞留した証明はまだない。
従ってこのsource不備の修正をrun167根因確定や9F解消とは呼ばない。
Haltの実物理停止/参照保持、BTの実radio/HFPは別の契約であり成功にしていない。

`python3 tests/mt6630_async_pause_test.py`はproduction Pause/claim/api_leave関数を抽出して
限定したホスト境界を実行する。NDIS/電源遷移の実行ではない。
source-only依頼のためWDK compile/link/signと実機試験は未実施。
将来の署名候補はownerがprovider版を割当て、ndis_runtime.cの1TUだけを増分buildする。
新import NdisMPauseCompleteはNDIS6.0公開APIだが、本作業では実RT8.1 ndis.sys exportの
照合までは行っていない。既存43objectのうち他42TUはheader/source不変、cache再利用時も
元manifestのclosure/flags/hashを確認する。client2017・親1117・GPU351/169を変更しない。

一次契約:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nc-ndis-miniport_pause
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nf-ndis-ndismpausecomplete

## provider1027 integration

Carson source2df626895513d734ba5af87837d9e0c5bcf81530 is retained as the
origin of async Pause. Drain also explicitly tests tx.pending, both for immediate
SUCCESS and the one-time completion claim. Current normal worker aborts/completes
TX before exit, but an invalid/faulted backend must not let an unreturned NBL
become a successful Pause. This is fail-closed ownership checking, not a proved
9F fix. The production extraction test covers this case under ASAN/UBSAN.

An actual installed-Windows partition backup (20261106) contains ARM NDIS.SYS
6.3.9600.16408, SHA256 3c1b6c26e7cda87ff075e751a65d972fc273fed08cc106d8b0145958a5339120.
Its non-forwarded export NdisMPauseComplete is ordinal237/RVA0x15ec9.
This export-table check used no disassembly and no live device access. Current
run170 device-byte equality is not re-queried by this source owner.

The busy/API drain boundary still serializes with the shared BT/OID worker;
asynchronous Pause frees the PnP callback but does not bound a stuck physical
I/O or an indefinitely busy control worker. Do not claim it alone resolves9F.
Client2018 has independent source aa4233fd and remains byte-identical.
