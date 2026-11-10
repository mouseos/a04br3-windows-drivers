# 親1104 / thermal1.8 の統合資格

正本1102 source4e253a5cを基点にthermal149e6b61とACCDET理由7f7ce587のcommit差分だけを合流。AFEその他のbranch祖先は取り込まない。親INF2026.11.8.1104、専用CAT。新thermal PIO GUID176ef135... / version3 / ParentBuild1104を親子共通headerで固定し、両TUを再compileする。署名thermal1103 tagの子を1104用へ再包装しない。

親8range所有、thermal子resource要求ゼロ、親Measureのready1/stage6/lease/StopIdle/poison/quarantine拒否を維持。旧1.7との混在禁止。ACCDET IOCTL54 socket16とIOCTL58 reasons68/schema1、GPIO19資格、不成立ERROR21を保持。新GPIO/PMIC/AFE writeなし。RTC/USB/SD/GPU/音声/HFP owner APIは基点のまま。UEFI125f phase1 actual0は未解決で、親資源競合修正を温度成功と称さない。

既存1101 cache helper13とthermal helper3をhash/source照合して再利用し親TU/子TUの2つだけbuild/sign。共通EDK/SDK再setup/実機操作なし。build-mt8167-thermal-parent1104.ps1の新規checkout-local出力とsigned-manifest.jsonを使用する。
