# インストール済みINFの所在解決

`SetupGetInfDriverStoreLocationW` はINF内容を検索せず、system INFまたはDriverStore内INF名を受け取る。managed data directoryのstaged source INFは入力契約の対象外で、run129では1168を返した。

別のdevice information setで同じinstanceを開き、`DI_FLAGSEX_INSTALLEDDRIVER`付き`SPDIT_COMPATDRIVER`を構築する。唯一のinstalled driverのversion/dateを再検証し、`SetupDiGetDriverInfoDetailW.InfFileName`をlocatorへ渡す。可変HardwareID末尾の不足時だけ、文書で保証される固定fieldを使用する。DIF/SetSelectedDriver/registry書込み/ディレクトリ全走査は行わない。

staged INF、store INF、published INFのexact hashとWindows\INF直下oem leaf検査は維持。既存の署名/CAT/SYS/UMD/FW/CM確認も不変。optional1168扱いは既存2locator APIの戻り値だけであり、新しいinstalled-driver list取得エラーには適用しない。PB0x254にinstalled-only取得結果を追加、既存PB意味は不変。

`tests/installed_inf_name_test.py --scratch <root内path>` は実helperをASan/UBSanで実行し、唯一性・version/date・access error・境界・全分岐のcleanup・可変末尾不足を確認する。既存selected_inf_gate_testも維持。実RT上の解決成功は未検証であり、旧Run130 policyのcompile fixtureは配備用ではない。

一次資料:
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupgetinfdriverstorelocationw
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w (DI_FLAGSEX_INSTALLEDDRIVER)
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdriverinfodetailw
