# LK panel timing handoff / Windows driver candidate

## ABI（UEFI担当と固定）

DIS0 namespaceの`DTM0()`（引数なし）は64byteのACPI Bufferを返す。
各値はlittle-endian DWORD。旧DMS0は変更しない。

|DWORD|意味|値／単位|
|---|---|---|
|0|magic|0x314D5444（DTM1）|
|1|version|1|
|2|bytes|64|
|3|flags|bit0 validのみ。他のbit禁止|
|4|panel_id|1 = auo_wxga_dsi|
|5,6|active width,height|800,1280|
|7,8|total width,height|960,1300|
|9|fps100|当該bootのLK videolfb raw。100倍Hz|
|10|fps_denominator|100|
|11..15|reserved|すべて0|

UEFIは実chosen/chosen@0のatag,videolfb、panelnameとfpsを検証してからvalidを立てる。
UEFIのvalidHOB欠落時はDTM0不在を許容する（明示的name-not-foundはnotready）。
データ未取得をrecordで表す場合はvalid=0。WindowsはSTATUS_DEVICE_NOT_READYを返し、60Hzで補わない。
未知version/flags/panel、geometry不一致、reserved非0、fps100が4500..7500外、表現不能な周波数は拒否する。
board HOB／immutable ACPI生成は別UEFI branchの責任範囲。EDK2共通変更なし。

## 源泉と計算

CT3_GPL_LGPL_Src/kernel-4.4/drivers/misc/mediatek/lcm/auo_wxga_dsi/auo_wxga_dsi.c:333–344:
800+4+132+24=960、1280+4+8+8=1300。
mt8167/videox/mtkfb.cのtag_videolfbにはfb_base(u64)、islcmfound、fps、vram、lcmnameが並ぶ。
fps単位は同sourceの2*100000/lcd_fps（2frame待機）から100倍Hz。
PLL245はpixelclockではない。

VSync=fps100/100 Hz、HSync=1300*fps100/100 Hz、PixelRate=960*1300*fps100/100 pixels/s。
約分後にcheckedな整数演算を行い、丸めない。5990なら59.90Hz／77870Hz／74755200pixels/s。
6008なら60.08Hz／78104Hz／74979840pixels/s。例の数値をdriverの固定入力にはしない。

実dxgkrnlのtarget AddMode（VA0x6b258、reject block0xa3edc..0xa3fbe）は、全5値がNOTSPECIFIED、
ControlInterrupt callback非NULL、AllowUnspecifiedVSync=0の組合せをC01E030Aとして拒否する。
DOD sentinel自体の不正ではない。run203当該adapter帰属／動的branchは別途未確定。
この候補はControlInterruptと実RDMA VSync経路を維持する。

## driver寿命と互換

transport v1のlayout／動作は不変。v2はv1をprefixとしてTiming callbackを末尾に追加。
新renderはversion2を明示要求し、旧357にfallbackしない。
TimingはPASSIVE_LEVELで参照保持中のlower ACPIへread-only要求する。
ACPI framingと64byte recordを検証してBDDへ値をコピーし、target／monitor双方で使用する。
modeのActiveSizeをPOST geometryと照合しTotalSizeにはporch込みの値を使う。
信号struct全体を0初期化してreservedと8.1 non-Miracast divider0を保持する。
IRQ enable／OVL／DSI設定やCPU framebuffer描画は追加しない。

## ビルド差分

base f566dba7（GPU368のproduction sourceを含む）。現在の368署名候補は変更しない。
bdd.hxxのclass layout変更があるため、表示C++全10TUをMSVC14.29で同一sourceから再compileする。
bdd,bdd_dmm,bdd_util,bltfuncs,blthw,mt8167_brightness,unified_display,memory,ge8300_trace,ge8300_present。
追加のC再compileはdisplay_adapter356.cとtransport356.c。
表示archiveを置換後にrender SYS／transport SYSの両方をLINK14.29で生成する。
旧transport357保持不可。全依存include/args/toolと既artifact SHAを検証せずobjectを混合しない。

変更していないnative67／UMD／PCO375／SDKの再buildは不要。
これらの再利用には既実artifactの出自を保持し、変更sourceから生成したと偽らない。
既fullsource wrapperのstrict compiler cache入口は変更しない。
Windows toolchainによるdriver buildとUEFI統合buildは未実行。host算術契約の成功は実hardware成功ではない。
新profile、nativeSYSTEM、媒体、端末実行はこのbranchの範囲外。
