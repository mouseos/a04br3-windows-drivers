# VidMm staging/indexを実linear apertureへ接続

実native libraryの基点4260e420を保持し、KMD915feffb系とは独立sourceでビルドする。
main REA093が実VidMm ReadSegmentSet0拒否を確定。PermanentSysMemを変更する修正ではない。

ownerはstaging(A8/X8)とindex16/32をmetadata resourceとして作成する。
Bindは既存frontend.resolveによる独立PFN pin、native lease acquire、実DMA SG/PFN同一性検査、
cached Normal RAM mappingを再利用する。leaseはresource単位で保持し、実PTEを書いて全1024entryを照合する。
単なるMAP_APERTURE成功stubやprivate bufferへのコピーは使わない。

linear mappingは最大8件、既存RT/vertex/他linearのVA・PFN・CPU範囲重複を拒否。
stagingは出力方向SG、indexは入力方向SGを使う。前後のdescriptor ABI/versionは変えない。
実2枚のPTが保持する4MiB/1024page apertureに対し、commit limitも1024pageへ拡張する。
元485pageはRT469page＋vertex16pageまでの制限だった。VA base/extent/DMA上限は不変。
RT/vertexとstagingを同時residentにできる実範囲だけを返す。

cold graph再構築で全linearを再attach。root公開前にはcache syncとexposure markerを設定する。
Unbindは既存の本当のstop/off/IRQ-drain/reset/platform VerifyOff/graph retirement後にのみleaseを解放。
失敗・曖昧なpartial PTE更新はpoison＋lease保持。全解除後のcold releaseでSG/refを均衡させる。
標準stagingをrender targetへ昇格せず、X8 renderingや新capabilityは広告しない。
UMDはmain所有で未編集。新UMD3f8291d0と後で独立pinして結合する。

host試験はactual native_mdl/target/linear sourceとOS/MMIO doublesを使用し、
大staging/1page index、両bind順、2PT跨ぎ、PTE/PFN一致、重複/上限拒否、内容保持、lease/SG均衡を確認。
実機未検証、描画成功とは扱わない。
