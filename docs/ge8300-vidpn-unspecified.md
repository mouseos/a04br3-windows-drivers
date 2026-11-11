# 固定POST出力の未指定変換

WindowsのVidPN rotation/scalingのNOTSPECIFIEDはminiportが具体値を選ぶ契約。
既存のIsVidPnPathFieldsValidはこの値を許すが、unifiedのA04PathFitsは
IDENTITY以外を拒否していた。固定800x1280のunified経路では未指定だけを
IDENTITYへ解決し、支持判定とcommit/update時の実保存値を一致させる。

UNPINNED、UNINITIALIZED、実回転、拡大縮小、異なる寸法の許可は広げない。
DOD経路と正当なPOST解放による停止条件も変更しない。

実369でハードウェアD3Dデバイス作成は成功したが、同adapterの出力列挙は
DXGI_ERROR_NOT_FOUNDだった。この変更がその原因を解消したかは未確認。
実際のpath値と同bootのPOST解放記録を照合し、正式増分ビルドと実機結果で
判断する。通常デスクトップ描画の成功をsource変更だけから主張しない。

## 固定パネルのtarget pinとPOST返却

`012acad3`起点の修正では、unified表示のpinned targetを
`BuildPanelSignal`の正規信号と比較する。active/total寸法、VideoStandard、
PixelRate、scanline、VSyncFreqDividerを意味で比較し、周波数は非ゼロかつ
非sentinelのUINT成分を64bitへ拡張して交差積を比較する。
既約分数でなくても同じ周波数なら受理し、reserved/paddingは比較しない。

`IsSupportedVidPn`は不一致を成功statusと`IsVidPnSupported=FALSE`で返す。
`EnumVidPnCofuncModality`はtarget pivotも検査し、不一致なら既存pinや
mode setを置換する前に`STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED`を返す。
source/targetが未pinの部分VidPnは支持判定を続けられるが、非空の通常Commitでは
sourceとtarget双方のpinを要求する。target未pinや不一致のCommitはframebufferの
unmap/map前に拒否する。既存のPathPoweredOff通知と空topologyの停止処理は維持する。
query/releaseエラーは失敗statusとして返し、正常なunsupported判定と区別する。

POST release成功時は、維持する唯一の出力をTargetId=0、AcpiId=0で返す。
これは`QueryChildRelations`のChildUid/AcpiUidと一致する。
Startで取得したTargetIdの未初期化値やAcpiIdを変更する処理は追加しない。
ACPI上の未知のchild IDは推測しない。将来出力やACPI対応を増やす際は、
QueryChildRelationsとPOST release双方の対応を更新する必要がある。

一次仕様:

- [VidPn支持判定とpinned source/target](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/determining-whether-a-vidpn-is-supported-on-a-display-adapter)
- [cofunctional列挙とpivot保持](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enumerating-cofunctional-vidpn-source-and-target-modes)
- [POST取得時に許される未初期化ID](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_acquire_post_display_ownership)
- [POST返却時のtarget/ACPI ID](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_stop_device_and_release_post_display_ownership)

既存host検証へ追加した契約テストは、次の1コマンドで実行する。

```
python3 tests/test-ge8300-vidpn-hwcap-output.py --wdk-shared <WDK8.1/Include/shared> --output <新規出力先> --target-contract
```

実IsSupported/EnumCofunc/Commit/BuildPanelSignal/QueryChild/POST release本体を
抽出し、WDK8.1の信号型と既存panel timingコードを使う。
mode-set API、mutex、framebuffer操作はhost fixtureで置き換える。
正規800x1280信号、分数の同値表現、reserved非依存、不正信号12種、
各pivot、pin保持、参照解放、query/release失敗、空topology、POST返却と
QueryChildのID一致をASan/UBSanで検証する。信号の構造全体をmemcmpする処理は
実装しない。fixture内のmemcmpはpin/pivotへの書込みがないことを確認するために使う。

host fixtureはWindows ARMのABI・driver build・実機動作を証明しない。
この修正で実233のACTIVE path=0が解消するかは未確定。
render側の能力フラグ、INF、allocation、EDK2は変更しない。
