# m1.11 native renderのCPU構築部

このディレクトリのnative実装は、既存modern FWIFから独立している。通常のドライバ開始・DOD経路には接続していない。MMIO、DMA割当て、FWへの公開、実機送信は行わない。

## 実装した契約

- `native_resources`: 固定freelist（init=max、grow=ready=0）、物理4KiBページ番号表、任意の再構築用mirror、親render targetと2組のHWRTを構築する。FWのstate/counterは同版KMのZERO_ON_ALLOCに従う。PMページは全数を割り当てる仕様であり、KMのlowest-entry省略最適化は使用しない。PM領域はfreelist当たり最大16MiB。2つのlistの物理ページ集合が互いに重ならず、他のGPU/FW資源とも重ならないことを割当て側で保証する。
  `a04_m111_layout_auxiliary`はcaller所有のGPU allocation内に、128-byte/64-byte alignmentのdummy region headerと2048-byte/16-byte alignmentのdummy stencilを分ける。使用量は0x880 bytes。TA+0x78にはdummy region GPU VA、3D+0x128にはdummy stencil GPU VA|1を設定する。allocation/mappingと初期内容はこの関数の責任外である。
- `native_record_fields`: libsrvが更新する確定済みfieldのみをpatchする。未確定registerやpaddingへ書かない。PBEは`+0x68`の8組×2個の64bit CSR、background bankは`+0xf8`と`+0x110`の各3個の64bit wordとして独立にpatchできる。値の生成とアドレス所有権は呼出側の責任である。
- `native_ccb`: 40-byte header、TA/3D/PR opcode、UFO fence/update、4KiB ringのpadding/wrapを構築する。payloadはそのままコピーするだけで、正しい描画packetと認証するAPIではない。KICKはDM2/DM3の別contextとHWRTの別cleanup参照を使う。
- `native_pair`: 固有PR payloadを使わないTA/3Dの同期列を組む。TA/UPDATE=224 bytes、FENCE_PR/3D/UPDATE=472 bytesの各グループを先に予約し、paddingはグループより前に置く。8KiBの呼出側workspaceを使用し、失敗時は両CPU shadowとresultを変更しない。FW context/HWRT/UFOの重複、payloadのHWRT/UFO不一致を拒否する。Z/S/MSAA backing objectとno-overlap syncを持たない部分集合に限定する。runtimeの排他、Device memory転送、FW公開は行わない。

すべてordinary CPU shadow memory向け。Device memoryへ直接使わない。失敗時は出力を変更しない。成功はCPU側の構築完了を意味し、DMA backing、依存資源、queue lifetime、FWによる実行を保証しない。

## 根拠

資源とCCBはImagination/MediaTek `m1.11ED5425693`の以下の関数に基づく。MIT/GPL二重ライセンスのMITを選択し、元のlicenseは`A04BR3RogueDDKABI.h`に保持している。

- `services/server/devices/rgx/rgxta3d.c`: `RGXCreateHWRTData`、`RGXCreateFreeList`、`RGXGrowFreeList`、`RGXCreateRenderTarget`、`PVRSRVRGXKickTA3DKM`。
- `services/server/common/pmr.c`: `PMRWritePMPageList`。
- `services/server/devices/rgx/rgxccb.c`: `RGXAcquireCCB`、`RGXCmdHelperAcquireCmdCCB`。
- `services/server/devices/rgx/rgxfwutils.c`: `AttachKickResourcesCleanupCtls`。
- `services/include/rgx_fwif_km.h`: client opcodeとCCB header。

board `libsrv_um.so` SHA256 `d32a4a224d06302d9bec0a66cbb7d17dec663d3985f1c8592f5bb77353ca1d4b`の`RGXKickTA`（0x126354）と3D field更新（0x12877c）、`libGLESv2_mtk.so` SHA256 `d793f3896fb02490adf7b83481e032e0f0cd86158f7e7ded9cc938cc61e6ddb4`の0x1612a0、0x1c2174、0x20ff6c、0x15e9c8、0x11372cでrecordのサイズと格納位置を照合した。生ログと完全REA証拠はプロジェクトのlog領域に置き、ここには入れない。

## 検証

リポジトリrootで`make test-gpu-m111-native`。物理ページ表とmirror、resource初期値、失敗時の出力保持、未確定byteの保持、CSR pair配置、CCBのbyte長・opcode・同期順・wrap・満杯拒否を検査する。試験payloadはtransport検査用の合成bytesであり、実機に送ってよい描画fixtureではない。

## 接続前に残っていること

1. TA/3Dの未確定registerと末尾について初期化元と意味を確定する。`native_record_fields`をzero配列へ適用して完全packetと呼ばない。
2. USC/PDS/VDM/PPP/EOTをnative recordに合わせて生成し、GPU VA・FW address・物理ページの所有権と全資源の範囲を検査する。PBE CSRの2wordとEOT emit wordは別物である。
3. GPU側のPM/TPC/MTA/region/VHeap backingとFW native bankをcold状態で割り当て、mappingと参照を公開する。builderの入力値はallocationの存在を証明しない。
4. CPU側の両ring構築には`native_pair`を使い、runtimeではhardware ownerの排他とqueue容量確保の下で一組として公開する。PR固有payload、Z/S/MSAA backing object、no-overlap syncが必要な場合は対応するfence/update/cleanupを追加する。PRと3Dの両方をkickしてPR固有payloadがない場合、KMはPR taskを省略して通常3D taskを使う。
5. 既存`command_busy`、hardware ownership、poison、cleanup、DMA barrier、FWCCB回収を保ったruntime接続を実装する。TA/3D completionを別々に確認し、実行が停止した資源を解放しない。

CPU側構築部のみが完成している。完全描画packet、ドライバへの接続、実機3D成功は未達である。

## 単一RTのGPU storage配置

`a04_m111_layout_target_storage`は同版`RGXAddRenderTarget`（0x124a34）のMSAAなし・単一RT分岐を実装する。各軸16-pixel tile数を切上げ、region headerを`align64(tilesX*tilesY*5)`ずつ2組確保する。TPCは両軸tile数以上の最小2冪をsideとして`side*side*8` bytes、64-byte境界へ切り上げた位置に続くVHeapは0x600 bytes。RTCはこの分岐にはない。800x600ではregion計0x4a80、TPC/VHeap計0x8600 bytes。領域の不足・重複・40-bit範囲超過を拒否する。MTA/MLIST、初期内容、実allocation/mapping、完全HWRTの生成は別工程である。

到達目標はまずnative TA/3Dによる実GPUのclearまたは三角形描画。Windows D3D UMDと完全WDDM render miniportはまだなく、DOD転送成功は3D対応を意味しない。

`a04_m111_layout_pm_mlists`は`RGXCreateGlobalPB`のMLIST容量式と`RGXAddRenderTarget`の2組配置を実装する。固定PB 16KiB〜16MiBの部分集合では各MLISTが4KiB、計8KiB。同版UMはHWRT作成bridgeのmacrotile引数へliteral 0を渡すため、単一RT builderでこの0を受け付ける。これは未確定fieldのゼロ埋めではなく、観測した引数の再現である。実GPU backingと初期内容はここでは生成しない。

## clear依存PDSコード

`native_clear_pds`はboard GLESの0x2025c4/0x2047e4/0x202600から、単一sample・追加task flagsなしのUSC taskと背景PDSの生成を実装する。背景は8 dwords data＋1 dword code。taskは先頭64 bits、codeは+32の0xf4000002。vendorが書かないdata words2〜7は保持する。EOT側のDMAなしdataはtaskの2 dwords。USC heapは既存0xba00000000から1GiB内、temporary countは4単位切上げで最大252。USC本体、uniform data、pixel-event PDS code/register bindingはこのAPIの入力・責任外であり、完全clear shader/packetではない。

SetupBGObjectは0x1fe43cのCreateSpecialObjectPixelShaderを経由してPVRUniFlexCompileToHwでUSCを生成する。0x2047e4のPDSコードが確定しても、compilerが返すUSC bytes、temp数、constant bindingを推測で埋めない。
