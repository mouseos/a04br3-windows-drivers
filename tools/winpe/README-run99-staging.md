# run99 / fresh98 / render3.15

fresh98の明示manifest04ceac86とnative seven-node receiptを基点とする。
KMD b27164a1（標準X8 stagingとCPU-only PitchAlignedSize0）、UMD d653e495の境界traceを独立sourceでpinする。
PermanentSysMem/zero segment/MapFlagsは既存のまま。staging失敗原因を確定したとは扱わない。

単一GPUのみPnP更新、既存署名client f3afを1回、planned reboot最大1。
親/Audio1026・MSDC1.8・USB1005・WiFi1012・DOD1.6・POST bootとregistry policyを保持する。
SOFTWARE/trustは書き換えず、SYSTEM/DRIVERS trio・全捕捉driver・既存Engine固定fileを比較する。
旧専用service EXEも実byte比較後に置換する。旧run97データdirectoryは保持し、
新run99用MT8167RenderSingleGpuRun99へpackage/client/log/markerを分離する。
service名とEXE名はMT8167RenderSingleGpuを継続する。

native helperはfresh98 private copyだけをloadし、既存disabledの専用service7値を確認してStartを2へ再arm。
Engine履歴と専用serviceのschema2/count1/client1・固定MOU0006履歴をallowlist/type/bound付きで検査し、
patch前後とclean再loadでfingerprint一致を要求する。旧6e8b SYSTEMは再利用しない。
実機main所有。媒体完成・署名成功・実機D3D成功は別々に記録する。
