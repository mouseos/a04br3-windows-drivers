# Native RT/SRVの同一backing利用（候補）

baseはshared/main194/Herschel統合c30b8ca1。current367・source3d・サービスprofileは変更しない。
prior-findingsの第2項を優先し、同一allocationをRTとSRVで使うproduction経路を追加する。
第3項のうち既存native encoderが扱える整数viewportもUMDへ接続する。
Windows build・実GPU・DWMの成功は未確認。

## 成立させる契約

- DEFAULT BGRA8 Texture2D、single mip/slice/sample、CPU accessなし。
- 併用はlogical extent=physical extent=800×native height、pitch3200。
  unifiedは800×1280、render-onlyは800×600。小さいlogical imageをpadded RTの正規化UVで読む経路は拒否。
- GEO_USAGE_RT_TEXTURE2D=6がPBE出力とSMP入力の両roleを持つ。descriptor bytesをrole間で書き換えない。
- shared v3のresource/allocation descriptor照合を維持し、RT|SRVの共有create/openも同じ実allocationを使用。
- RTV/SRVを別resourceへbindして順序を守って利用する。同一allocationへの同時read/writeはUMDとKMDの実解決時に拒否。
- sourceの失敗を成功stubやCPU clearで置き換えない。caps、feature level、shader witnessのallowlistは広げない。

## Native tile/cache/layout・寿命

RTのMDLをresourceが所有する。allocation.bindingは現在のtargetを借用し、binding_borrowedを記録。
他RTはlinear resident inputとして自身のVidMm aperture offsetに残す。
selected target/vertexはgraphのlinear listへ重複登録しない。
既存のPA/CPU alias、範囲、MDL保持、PTE readback検査を維持する。

切替はwait_boundary_locked→実stop→通常IRQ/OFFを含むretirement→cold graph生成の順。
旧targetの同じ保持MDLを別targetのSTRIDE samplerで読む。別allocationや画素の複製は作らない。
PBEのlinear storeとSMPのstrideは両方3200byte/row。
HWRT/tile dataはretire後に作り直す一方、active pixel backingはpreserveして残す。
CPUが書くのは既存のallocation末尾guardだけで、active画素のclear/resolveではない。

finish_publishedの実TA/3D/UFO/cleanup→SLC flush/invalidate→GP sync→MDL cache ownershipを維持。
stop失敗・graph再生成失敗は資源を保持してadmissionを閉じる。
UNMAPはstop/retire後にborrow pointerを外し、resource所有MDLを一度だけreleaseする。
allocation destructorは借用bindingを独断で解放しない。
普通のRTもこのresource所有経路を使うため、新branchではnative/KMD/UMDの全依存TUを同一sourceで再compileする。
旧367やwarm197の部分object/UMDを組み合わせない。

native finishの800×600固定pixel/guard境界も実frame geometryに修正。
1280行backingの有効画素をguardとして誤判定しない。drawでmatching_pixelsを捏造せず、
実fence/cacheと末尾guardを検証する既存方針を維持する。

## 一次資料との対応

Imagination Mesa 251b89ed9f7d501cea150591237d47dcc5cec0d9:

- src/imagination/csbgen/rogue/pbestate.xml: MEMLAYOUT.LINEAR=0、WORD1のmemlayout[33:32]、linestride[49:34]は2pixel単位。
- src/imagination/vulkan/pvr_arch_tex_state.c: linear2DをTEXTYPE_STRIDEとし、STRIDE_IMAGE_WORD1へstride-1を格納。
- src/imagination/vulkan/pvr_arch_cmd_buffer.c:pvr_setup_viewport: viewportのscale/center。
- 既存native_clear_surface.c/native_texture2d_state.cのproducerを変更せず、同一pitchのdescriptorが一致することをhostで検証。

Android GPL Linuxのm1.8ED4333936/services/server/devices/rgx/rgxpower.cは
RGXPrePowerState→RGXStopという通常停止の資料。別DDK版であり同一binaryの証明にはしない。
既存native_owner_factory_runtime.cのm1.11通常retirementをそのまま利用し、force-unload等の迂回を加えない。

UniXNU-a04br3 4a62c22fa18674033525946246e94b5806d5fc12の
package/powervr/kernel/pvr_hwrt.cはHWRT/geom backingの所有寿命を分離し、
package/powervr/kernel/pvr_cccb.cはKCCB fenceを待ってからslotを扱う。
docs/powervr-fence-timeline.mdもsubmit返却・UFO観測・WaitFence・実表示を区別している。
今回もGPU job完了とresource/graph退役、DWM表示成功を同一視しない。

## 関連DDIと残件

整数viewport（x/y>=0、width/height>0、800×600内、depth0..1）をtyped draw/pairから
既存PPP encoderへ渡す。UMD/nativeが同じ純整数profile predicateを使う。
GPU RT→stagingの全面copyは新dual-role RTにも接続する。

blend、任意scissor、部分copyは未実装で拒否を維持する。
blendは単なるUMD snapshot変更ではなくUSC側blend program/出力maskが必要。
scissorはbooleanだけの現ABIにrectがなく、部分copyにもsrc/dst originとextentのwireがない。
それぞれwire・capture・validator・native producer・寿命を一体で追加する必要がある。
今回の限定RT/SRVとviewportをDWM一般対応や全DDI完成とは扱わない。

## 検証とbuild

少数のhost検証でproductionのtyped descriptor predicate、shared create/open、engine job参照、
native target選択/unbind/graph list、PBE/SMP descriptor、viewport encoderを使用。
OS/ハードウェア操作だけmockし、両geometryでASan/UBSanを適用。
実scheduler同時実行、Windows DDI link、実cache/IRQ、実GPU renderingは未検証。

fullsource origin/native mapは過去行を保持して更新する。
全fresh warm entry build-ge8300-shared-surface-warm.ps1を使用し、PCO旧f4は
既許可のSHA固定prebuilt toolとしてのみ保持可能。旧includeClosureRecorded=falseの限界は不変。
PCO/SDK再build fallbackは追加しない。旧固定aa661 UMD/resume helperはsource照合で拒否する。
