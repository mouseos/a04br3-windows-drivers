# ４ CPUでの早期watchdog引継ぎ試作

MMA8652 WinPEの失敗例では、ESPのBOOT.WIM SHA-256
`a8a227259d9089d3425369838ad478bb55f9273dfab62b72de4b211312ed7a47` を
読み出して確認した。`a04br3/toprgu/` にパッケージはあるが、
`Windows/System32/drivers/mt8167-toprgu.sys` とSYSTEMハイブの
`ControlSet001/Services/Mt8167Toprgu` は存在しない。ドライバをコピーしただけで
オフライン導入されていない。対象起動のUARTではExitBootServices成功後、
A04RGU開始マーカーがなく、次のpreloaderがWDT HW reboot、BOOT_REASON 9を
記録した。31秒の猶予中にTOPRGUが始まらなかったことと整合するが、証拠だけで
未導入を唯一の原因と断定しない。過去の別OS起動には4 CPUでA04RGU開始・継続
給餌の記録がある。3 CPUのMMA8652成功は別構成の結果。

Windowsのboot-startはドライバのロードを早めるが、PnPデバイスの資源付与とD0
到達を保証しない。INFはStartType=0、System Bus Extender、MOU0005のPnP
ドライバ。WinPEでは起動後のdrvloadより前に、DISMで対象WIMへパッケージを
オフライン注入する。対象RTが署名を受け入れ、PnPが31秒以内にD0へ到達する
ことは主担当の実測が必要。[Microsoft: boot-start](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/installing-a-boot-start-driver)、
[ロード順](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/specifying-driver-load-order)。
既存 `tools/ci/install-toprgu-wim.ps1` は入力WIMとSYSのハッシュを検査し別WIMへ
DISM注入する。ドライバストアへの追加はD0到達や署名信頼の証拠ではない。

この試作はD0直後や3 CPU構成でTOPRGU/UARTを触らない。4 CPUがオンラインなら
論理CPU1を対象とするDPCを予約し、そのDPCがCPU1のMPIDR affinity 1上で実際に
動いて初めてWindowsの所有を確定する。元の4 CPU BootShimでは物理CPU1は最後の
給餌後にworkerへ戻らずWindows parkingへ移るため、この機種固有前提の下で同じ
物理CPUでWindowsコードが動くことはworkerの離脱を示す。論理番号が別の物理CPUに
割り当てられた場合やCPUが減った場合は給餌を停止し、回復watchdogに任せる。
EL3・別CPU・別ドライバが独立に給餌しないことは別途実機で確認が必要。
停止時はWDFタイマーを同期停止してからbackup timerを解除・DPC排出する。
D0外の無効化と復帰は既存試作であり、スリープや復旧の安全性を保証しない。

ホスト試験: `make build/toprgu_handoff_test && ./build/toprgu_handoff_test`。
実機試験では固定したUEFI・WIM・署名・ACPI資源、最初のブートと次のBOOT_REASON、
`A04RGU CPU1 ownership acquired CPUs>=4` と周期kickを照合する。署名、PnPの
早期到達、論理CPUとMPIDR、期限、電源遷移、watchdog復帰は未実証。既存の安全な
3 CPU復元経路を保持し、未知の停止時に繰返し起動させない。

2026-11-04のホスト検証では、上記元WIMと本試作SYSのハッシュを指定して
`install-toprgu-wim.ps1` を実行し、DISMが`mt8167-toprgu.inf` version 0.0.8.0を
ドライバストアへ追加した。ただし一覧のBoot CriticalはNo。オフライン追加の成功は
起動時のデバイスバインドや31秒以内のD0到達を意味しない。生成WIMは未署名
ドライバを含み、対象RTが受理するか未確認。変更前WIMとは別ファイルとして保管する。
