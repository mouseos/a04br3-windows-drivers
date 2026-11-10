# HCD1018 EP0 NAK / STALL 修正候補

旧1017はH_NAKTIMEOUTをSTALL/H_ERRORと一括失敗にし、EP0 STALLもUSBPORTへXACT_ERRORで返す。一次根拠はCT3_GPL_LGPL_Src commit 7c4782127d4ea11965be00ce1343056be9bfb769、kernel-4.4/drivers/misc/mediatek/usb20/musb_host.c::musb_h_ep0_irq と mtk_musb_reg.h。LinuxはNAKをflow control、STALLをEPIPE、H_ERRORをEPROTOとして区別する。既Windows state machineへの独立修正でありLinuxコードの転載ではない。実機発症を確定した主張ではない。

IN/OUT/status NAKでCSR0をclear→同token再arm、FIFO/actual/pending/stage不変、既absolute期限を延長しない。SETUP NAK/矛盾/未知errorは拒否。STALLを既A04_PIO_STALL→USBD_STATUS_STALL_PIDへ返す。StopAndProve/Abort detach/role/OC/VBUS guardは保持。

親1117基点b2c1a313にleaf/testsのみ署名1017 source abb2db82から継承。親1117/1120/GPU/FW、POWER/DEVCTL/PHY/clock/FIFO配置、資源claimの変更なし。XNU A04BR3MUSBPIO.hはdevice-role/EP1限定でありホストや給電成功の根拠ではない。

## 再現

Linux: clang -fsanitize=address,undefined -I drivers/usb/mt8167-usb20 tests/usb/ep0_nak_test.c drivers/usb/mt8167-usb20/ep0.c drivers/usb/mt8167-usb20/ep0_pio.c -o <root内out>/ep0_nak_test。そのEXEと既ep0_pio_testを実行。

WDK: tools/ci/build-mt8167-ep0-1018.ps1 -TaskRoot <専用dir> -KitRoot <既kit81> -UmInclude <既UM> -SignTool <既tool> -Inf2Cat <既tool>。TaskRootにはclean source、新規build出力、reuse-objects.json/reuse/*.objとbaseline-inputs.json/baseline/mt8167-musb-hcd.sysが必要。凍結1017の6objectsとsource/header closureを照合、変更headerの全4TUをARM再compile。6本WDK OS/MMIO seam試験、既identityでSYS/CAT署名・trust/membershipを確認。SDKsetup/親build/鍵exportなし。

## 未確認

実USBPORT登録、hub/class列挙、実NAK/STALL復旧、BOT転送、suspend/resume、並行/長期動作、OTG ID/VBUS/OC、5V/給電改善は未確認。給電案件は未完了。PC接続device-roleをhost成功としない。run183 PC-role記録もhost実証ではない。

将来mainだけが正規host条件と既親leaseを確認して、列挙→制御NAK/STALL→次class要求→転送/解除、deadline/Abortを個別に評価する。現媒体へ自動採用しない。workerは実機・role切替・物理操作依頼を行わない。

## 今回の署名候補

署名sourceは32444ffe760798576eb7d94e58ccd6b0a924145f。runner42729039661の専用C:\mt8167-musb-ep0-1018で、clang20.1.8＋既WDK8.1 cacheを利用し4TU再compile/6objects再利用、6WDK模型試験、SYS/CAT署名・trust/CAT所属を確認した。全importsは1017と同じで実RT8.1 exportsに存在する。ARMNT/native subsystem6.0は凍結1017と同じ。実機成功ではない。

Windows cloneはgit -c core.autocrlf=false cloneで行う。INFだけは既.gitattributesのeol=crlfを維持。初回既定cloneはC source改行変換をsource hash guardが拒否しbuild-rejected-crlfへ保存、その後source-lfのclean checkoutで完了した。guard削除なし。専用toolは既Inf2Cat tool-only57filesの小closure復元、SDK setupではない。失敗を含むログと固定inputs/reuse closure/source bundle、package/evidence ZIP、verify.pyをroot内の専用artifact/log directoryへ保存した。
