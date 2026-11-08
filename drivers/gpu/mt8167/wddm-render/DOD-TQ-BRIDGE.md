# DOD → single render owner TQ接続境界（未有効化）

このbranchは公開single-checkout320の4812ac61を基点とする独立準備であり、run111/native e0614136/package33853e9c/runtime0f24ca1fを変更しない。通常の標準API/UI全描画成功を宣言しない。

## 確認した欠落

現DODのge8300_present.cxxとge8300_display_resources.cxxは\Device\A04GE8300を開き、旧KMDF providerのCOPY/VENDOR_FENCED_COPY/SHARED_WIDE_FRAME等を発行する。render miniportのdispatch_io_requestにはその入口がなく、owner_interface.hは未公開draftである。従ってrun25の旧provider実成功をrender miniport置換後のGPU Present成功とは扱えない。既存CPU Presentは別経路として保持する。

## 実装した境界

- dod_tq_interface.h: kernel専用query/interface GUID、固定32x32/4096byteのprivate bounceのみ。MDL/物理アドレス/POSTアドレス/任意GPU packetは受け取らない。receiptは実task/flush/CPU visibilityを必須とし、scheduler completion/NoDMA/scanout readyを表さない。
- dod_tq_bridge.c: miniport埋込参照、exact ABI query、同期callbackとstopのmutex直列化、stale interface参照がある間の再start拒否、interface参照によるremove阻止。backend未登録ならQUERY_INTERFACEはNOT_SUPPORTED。NULL backendや成功receipt欠落を成功へ補わない。
- miniport.c: query/stop/power/removeの境界を接続。source/children/caps/IRQ処理を変更しない。**native TQ backend未登録のためdevice interfaceを公開していない。**
- DOD: A04_GE8300_DOD_RENDER_TQ_BRIDGE=1の明示opt-inで、唯一のdevice interfaceにqueryし1個の参照を取得。private8KB bounceで送受信し、実receiptの成功後だけCPUがPOSTへ32行を書き込む。不在/非対応/失敗なら既存CPU redrawへ戻る。GPUはPOSTを参照しない契約なので、private GPU DMA timeoutとPOST CPU redrawは競合しない。旧KMDF経路へ接続を偽装しない。

## 未完の具体条件

現native fb217359のgraphはnative_ownerを持ち、旧TQ scratch専用graphではない。旧windows_copy_scratch_freezeはexposedをstickyにし、releaseはexposedを拒否する。旧render_copy_buffer_fencedは専用source/destination/sharedのDMA backing、固定152byte USC asset、task_frozenと実start onlineを要求する。このassetは公開sourceに配布されずhash検査のみ存在する。

従って単に旧copy関数を現graphへ呼ぶ、同一lockだけ足す、exposedを消す、mask/fenceをNoDMAとして成功させる実装は行わない。残る実装は、既存単一owner内でのTQ graph/backing/asset供給、render admissionとの相互排他、実IRQ/cache完了、失敗quarantine、実off/reset/retireとの統合、およびbackendがcaller bufferを一切retainしない契約の成立である。成立後だけGeoDodTqStartとIoRegisterDeviceInterface/IoSetDeviceInterfaceStateを結ぶ。第二IRQ/provider/parent leaseは作らない。

これは有効化済みGPU Presentではない。build成功やhost modelをGPU実成功としない。mainのtimeout rootcause修正を先取りしてnative graph/lifetimeを改変しない。

## 検証

production dod_tq_bridge.cをpthread mutex/WDM型shimでhost compileし、backend無し拒否、receipt欠落、PENDING、IRQL/APC/overlap拒否、copy中stopの待機、旧参照付きrestart拒否と最終reference解放を検証する。mock backendはtransport検証だけでGPU完了証拠ではない。

```sh
cc -std=c11 -Wall -Wextra -Werror -pthread -Itests/dod_tq_bridge tests/dod_tq_bridge/test.c drivers/gpu/mt8167/wddm-render/dod_tq_bridge.c -o BUILD_IN_ROOT/host-test
BUILD_IN_ROOT/host-test
```

公開fullbuild手順はbuild-wdk81.ps1にbridge TUを追加してlink対象を保持する。DOD consumer有効compileは実WDK ARM C++でも別途確認する。署名candidate/媒体は作成しない。
