# MT8167 thermal/AUXADC proofの一回消費

run151の実status64Bはcal/query成功、Measure C00000A2、parent stage2、owner0を示した。実boot1fefのVariableRuntimeDxe依存ソースはruntimeのvolatile更新・削除をEFI_WRITE_PROTECTEDで拒否する。親1116のNV1指定はHALの前段検査を通すが、このUEFI規定を変更しない。attrs6を7へ変えず、共通EDK2を変更しない。

機種SSDTのSHR0にATCV=1、serialized ATCO、namespace内ATCL latchを追加する。ATCOは最初だけ1、以後0。親FDO/PDO再作成でもnamespaceは残り、OS rebootで再作成した場合だけ新起動のproofを利用できる。MMIO/IRQ、校正値、既producer/record v3は変更しない。

親は既72B proof/attrs6/ready等の検証後、lower ACPI stackへIOCTL_ACPI_EVAL_METHODを送り、ATCV/ATCO応答の型・長さ・値を検証する。PASSIVE、parent mutex前、2秒timeout。不明・失敗・消費済みは閉じたまま。親内cacheとthermal/HPのadcPurpose排他、one-shot/quarantine、live停止状態の再確認を保持する。新親＋旧firmwareはmethod不在で失敗し、新firmware＋旧親も従来の削除失敗で閉じる。

必要な組合せは新ATCO firmware＋親1117。leaf1.10は不変更。build-mt8167-parent1117.ps1は既13 helperをsource closure/hash固定しparent1TUだけcompileする。

iasl実コンパイルとacpiexecのATCV/ATCO/ATCO/ATCVで1/1/0/1を確認した。これはAML interpreterの結果で、Windows ioctl・実ADC温度・実機完了を証明しない。GPU342のrun153へは途中混載しない。
