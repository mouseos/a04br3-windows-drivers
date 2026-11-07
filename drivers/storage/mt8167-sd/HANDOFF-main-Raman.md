# main/Raman向けSD power source引継ぎ

## 適用先と禁止事項

run42（親1008 / Audio1006 / WiFi1005 / DOD1.5 / USB1003）へ混ぜない。
SD powerもthermalも実機未実証。既存tupleを置換せず、次の独立tupleだけに統合する。
SD既存署名候補の受領は、新電源契約の実機確認を意味しない。
既存eMMC/bootstorage、cache10303、通常UART trace抑制は本作業へ含めない。
親canonicalの丸ごと置換や、既存manifestにflagを後付けするbinary再利用は禁止。

## 固定sourceと順序

同一repo `https://github.com/mouseos/a04br3-windows-drivers`。

1. SD本体 `50d8eca75fa8da937f05bf2d6a932bbfc33f0920`。
2. NoFP/NEON/正規libcntpr helper `f729a77d73dc84894e7e543ade2b20e89c5ab880`。
3. SD power実装 `8f0a66c0598493125db4b074a04d571833f5056a`。
   このcommitの基点8a1807bは上記SDと統合親7df6bfaのmerge。mainで基点merge全体を
   canonicalへ上書きせず、既存branchに不足するSD pathsと親hookだけをsource統合する。
4. Audio `ecaa7b70248230dee2977e1d9057260f400287be`の親1008修正を必ず保持する。
   AFE sentinel `base==0xffffffff`で`GUID_BUS_INTERFACE_STANDARD`を
   `SendQueryToParentStack=TRUE`で実busへforwardする変更（wdmguid.h含む）。
   fake DMA/identity translationは追加しない。Audio1006自体もその固定sourceを保持する。

既にSD本体/helperを持つbranchなら8f0aのみをcherry-pickし、競合は下記hook単位で解決。
新checkoutなら上記SD sourceを取り込み、ecaa sourceをmergeしてから固定commitにする。
同じINF DriverVerで別SYSを作らない。親1008は既存ecaa署名binaryに専用、Carverの
thermal/BQ親1009とは別に、SD明示on親は11/07/2026,2026.11.7.1010を予約する。
SD INFも新tuple用2026.11.7.3を使う。既存署名済みpackageは上書きしない。

## 親write setとhook

- `drivers/bus/mt8167-shared/wdf_parent_probe.c`:
  `A04_SHARED_SD_POWER`既定0、guard内だけのsdPower contextとheader/include。
  既存ParentLock/reference helpers後に`power_parent.inc`を接続。
  親interface publishに`ParentSdPublish`を追加。
  ordered D0Exitはadmissionを閉じ同一lock下でSD heldを確認、未releaseなら失敗を返す。
  Stop/Unmapではheld/poisonedを成功扱いせず、未解決mappingを破棄しない。
  既存Audio/USB/WiFi/touch/buttonsのcallback・ABI・flagsは変更しない。
- `tools/ci/build-msdc-parent-probe.ps1`:
  新flagを環境から読む。on時だけSD power_owner.cをNoFP/NEONでcompile/linkする。
  親manifestへflag、ABI version、provider source hashesを記録する。
  ecaaの既存全parent flagsをbaseline manifestから読込み、追加差分はSD 0→1だけとする。
- 新規実装は`drivers/storage/mt8167-sd/power_owner.[ch]`, `power_parent.inc`,
  `power_interface.h`, `power_client.c`。SD windows.c/hとhelper、INF/ASL/READMEがconsumer側。

## 必要flagsとmatched条件

親providerは`A04_SHARED_SD_POWER=1`を明示。offは0をmanifestへ明示する。
`A04_SHARED_BUTTON_D0_EXPERIMENT=1`と`A04_SHARED_APMIXED_RESOURCE=1`必須。
既存`A04_SHARED_QUERY_EXPERIMENTAL=1` / CHILD_PROBE=1と全ecaa親flagsを維持する。
SD tupleでthermalを有効化しない（`A04_SHARED_THERMAL_DIAGNOSTIC=0`）。
Audio1006/USB/WiFi等に関係する既存flagsを「SDのみ」の最小設定に縮めない。

固定ecaa親1008 manifestの全キー/値を比較し、許容差分を
`A04_SHARED_SD_POWER`の追加onだけに限定する。flags以外にsource/INF/出力hashを固定する。
SD consumer v1を旧親へ載せてmissing interfaceを成功化しない。
親は新sourceから再build必須、既存SYSを新INFで再包装するだけの統合は不可。

## ABIと資源所有

GUID `f318c6da-d307-41ae-8916-a04413670001`, kernel interface v1:
`Acquire(token*)`（無変更claim＋S0 StopIdle）→SD自身のMSDC DMA idle確認→
`Enable(token)`（固定3.3V rail）→`Health(token)`→
IRP rundown/DAT0-CMD13/private PIO reset完了後のみ`ReleaseQuiescent(token)`。
失敗時token非0は保持/quarantine。未知状態でreleaseやpower-offを成功化しない。
UEFI-readyでclock既onのhandoffに限定。gated cold controllerをungateして未知DMAを
再開する処理は未対応。実配線以上の周波数/成功を広告しない。

親の**同じ**PWRAP bus/poison、ParentLock、既存mappingを借用する。追加ownerなし。
VMC0504 bit12/QI15・052a bit4、VMCH0506 bit14/QI15・052c bit7のみ。
TOP24 bit18 / TOP3c bit29(inverted)は専用SET/CLR、thermal bits1/30をRMWしない。
live voltage/PLL/mux retuneなし。元stateとの差分のみ復元、bounded timeoutと失敗伝播。
BQは別I2C2 owner、thermal側のADC資格/clock gate判断は別途未完了であり本SDが証明しない。

S0 DxはStopIdle reference、system S1..S5はPowerRelationsでsupplier先ON/後OFF。
_Interface参照や_DEPだけを電源保持証明にしない。_DEPは列挙依存の補助である。

## ACPIと出自

Carver固定UEFI source `f56b2c2c5cccb24084f6459665d4e792d33bc80c`:
`Platforms/Benesse/a04br3Pkg/WindowsArm32/Acpi/SdPioCandidate/MT8167-SD.asl`。
artifact: `port/xnu/out/mt8167-sd-acpi-f56b2c2c-preparation/manifest.json`。
元ASL SHA256 `4b394dd193adfb57b5e0663236c168d5d04e901ee9b123b132da106969250892`、
元AML SHA256 `a90b1aecca22e1d462837c1fbf16a015ed25dceb09d976ccd1f7b1d7117118c2`。
8f0a ASLは`External(\\_SB.SHR0,DeviceObj)`と`_DEP`を追加しているので別hash/出自にする。
親SHR0/BEN0001/UID0はCarver sourceの根拠。実採用FDが同namespaceかmainで照合する。
ASL/AML/iasl結果/元manifest hashをtupleへ入れるが、DSC/FDFへ勝手に接続しない。
CD20/pin/mux read-only proofはrail leaseを証明しない。VMC/VMCH保持と混同しない。

## build・署名・判定

SD側は`tools/storage/build-mt8167-sd.ps1`（ARM32/KMDF1.13、全7 TU NoFP/NEON、
正規WDK8.1 libcntpr）。親は上記matched全flagsでparent helperだけを実行する。
Audio/KS/USB/WiFiの不要な再buildを行わない。各SYS/INF/CAT署名とCAT membership検証、
source/script/全flags/全成果物hash・回収ZIP照合をtuplemanifestへ残す。
実機はmainのみ。Source作成・参照compile・実WDK link・署名・実機動作は別判定とする。

8f0a時点: ARM32公開参照header compile、strict core compile、PS構文、iaslは通過。
新電源sourceの実WDK/署名/Windows SD動作やsleep/resumeはこの時点では未確認。
以後のmatched build実績は別tuplemanifestで示し、この履歴を実機成功に書き換えない。
