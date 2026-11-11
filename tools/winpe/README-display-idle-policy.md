# 操作ボタン未対応時の開発用画面消灯抑止

A04_DEVELOPMENT_DISPLAY_ON=1を明示した更新サービスだけに適用する。既定は無効。
これはボタンドライバやGPU描画の完成を意味しない。

処理中は既存のSystemRequiredに加えDisplayRequiredを要求し、終了時に両方をclearしてhandleを閉じる。
更新後の診断phaseでは、現在のactive schemeのvideo idle timeoutだけをAC/DCとも0秒（自動消灯なし）へ設定する。
元のscheme GUIDと2値はその実行のlogs/display-idle-before.binへ保存する。待機・休止設定、他scheme、輝度、明示的なスリープは変更しない。

System32のpowrprof.dllから正規Windows APIを取得し、read→原値保存→active一致→write→active一致→apply→readbackの順に処理する。
途中失敗時は元AC/DC値を復元し、active schemeが同一なら再適用する。失敗状態とrollbackの状態は別々に記録し、復帰を推測で成功扱いしない。DLL準備等の初期失敗も共通の記録経路へ集約する。
独立したGPU診断を消灯抑止の失敗で省略しない。イベント0x2b0はAPI処理結果、0x2b1は失敗時の復元結果、0x2b2/0x2b3はreadback、0x2b4/0x2b5は任意DisplayRequiredの取得/解除状態であり、画面の点灯を直接証明するものではない。0x2b6はreadback取得済みmask（AC=1、DC=2）であり、未取得の初期値0を正常値と解釈しない。

これは現在のschemeの保存値を変更する。元へ戻す場合は、保存したGUIDとAC/DC値を同じPowerWriteACValueIndex/PowerWriteDCValueIndexへ渡す。そのschemeが現在もactiveならPowerSetActiveSchemeで再適用する。別schemeへの切替を検出した場合は、そのschemeを上書きして再有効化しない。

根拠はWindows 8.1 SDKのpowrprof.h/powersetting.h/winnt.hとMicrosoftのAPI仕様。

- https://learn.microsoft.com/en-us/windows/win32/api/powersetting/nf-powersetting-powergetactivescheme
- https://learn.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-powerreadacvalueindex
- https://learn.microsoft.com/en-us/windows/win32/api/powersetting/nf-powersetting-powerwriteacvalueindex
- https://learn.microsoft.com/en-us/windows/win32/api/powersetting/nf-powersetting-powersetactivescheme
- https://learn.microsoft.com/en-us/windows-hardware/customize/power-settings/display-settings-display-idle-timeout （0秒は自動消灯なし、Windows Vista以降）
