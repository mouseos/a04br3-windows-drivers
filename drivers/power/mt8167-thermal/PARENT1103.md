# Parent1103：thermal resourceを親へ一本化

基点は親1102 source4e253a5c。8range ACPI _CRSがTHERMAL1100d000/AUXADC11003000を親へ割り当てる一方、旧ChildRequirementsも同じ2rangeをDeviceExclusiveで要求する。run123ではthermal SYS/選択INF一致でもCMproblem12、Startedfalseだった。この二重要求を除去する。

機種UEFI/ACPIの全8rangeは変更しない。親は既存validatorでraw/translated8rangeを検査し、THERMAL/AUXADCのtranslated addressを保存する。thermal子PDOのrequirementsはゼロ、子Prepareはraw/translated各ゼロだけ受理し、MMIO mappingを一切しない。既存RTC子と同じresource-less型だが、新たなthermal制御は同じ単一親ownerに限定する。

新GUID `176ef135-78da-48bf-ab65-6145b73889da`、version3、ParentBuild1103を親と子だけで使用する。MeasureはContext/owner_active/raw[3]/stageのみ、caller address/mappingを受け取らない。旧v2 header/GUIDはsource上で変更せず、現PDOでは新v3へ切替える。**親1103とthermal0.0.1.8は同時更新必須**。旧thermal0.0.1.7との混在は不可。他のinput/audio/USB/RTC/GPU/SD query interfaceや資格flag、ACCDET1102 read経路は変更しない。

Strict PIO record(ready1/stage6/v2/72byte/attrs6)、当boot校正、clock/reset readback、IRQ/AHB/ADC停止、単一lease/rundown/StopIdle/期限/poison条件を保持する。新親はそれらのadmissionを満たした後だけ自分の割当rangeをmapする。lease前の拒否は未使用mappingだけをunmap、正常quiesceは既存成功経路でunmapする。post-touch不確定なら旧ownerと同じく参照/StopIdle/mappingを保持してquarantineし、他ownerを巻き戻さない。子へmapping寿命を転送しない。

UEFI125fのphase1/assert STA0は別の未解決事象である。Linux thermal sourceはSET+0x120/CLR+0x124/STA+0x128、bit0で一致。Linuxprobeはclock_on→reset、resumeはreset→clock_onで、STA即時反映をpollしていない。gated assertSTA=0の観測から別aliasや正しい待ち時間/順序は断定できないため、本候補ではfirmware制御/順序/条件を変更しない。ready0なら新親も温度を出さない。resource競合除去候補のbuildは温度実測成功ではない。

増分build: tools/ci/build-mt8167-thermal-parent1103.ps1。既存1101 helper cacheの13parent＋3thermal objectをsource/COFF hashで照合して再利用し、親TUと子TUだけcompile/link。既存BCBF鍵で2SYS/2CATを署名する。SDK/WDK再install・他driver/UEFI再build不要。実機/媒体適用はmain/Menciusのみで、現freezeを変更しない。
