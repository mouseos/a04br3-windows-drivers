# 不明なカード検出で旧要求を失効させる

SD7の既存検出はSDCDを100msごとに読み、0/1以外やACPI失敗を-1（不明）にする。
従来の1→-1→1はgenerationを変更しなかった。unknownが二つのlive()確認の間に回復すると、
旧要求/旧card_generationが再び有効になり得た。検出不能中の同一媒体性は証明できない。

この候補は1→unknownでgenerationを失効し、blockedとUNIT ATTENTIONを立てる。
単なるunknown→1は再挿入とせず、blocked/論理eject/absent_seenを解除しない。
本当に0が観測された後の1だけが既存の再初期化経路を開く。poisonedなwriteや旧RCA/容量を
検出値の回復だけで再利用しない。unknownをNO MEDIAや物理抜去へ読み替えない。
初めから空のsocketでは0→unknown→0もcard commandを開始しない。

失敗のためidle時にも保守的にI/Oを停止する仕様であり、同じカードが残っていても自動復旧を
保証しない。カード不在時のProbe反復、即時reset、任意給電、IRQ/レジスタ変更は追加しない。
未検出の短い物理抜挿しは100msサンプリングだけでは観測保証できない。
既存要求timeout/PIO poison/consumer join/lease retention、supplier失敗のsticky停止は保持。

Linux coreのcore.cの_mmc_detect_card_removedも、媒体確認の失敗で
removed状態を保持し、detect workで再評価する。Linuxのget_cd符号付きエラーを本Windowsへ
そのまま流用せず、ACPI結果の不明を独立状態として扱う。slot-gpio.cの200ms再評価は
カード検出switchの変化とプロトコル状態を同一視しない参考であり、Windows実機保証ではない。
XNU NativeMsdcは固定boot eMMCでありSDスロットの抜挿し実装として再利用しない。

INF2026.11.8.8/checkpoint revision8。MediaTek MT8167 MSDC1 microSD Hostを維持する。
未挿入カードのメーカー/型番は推測しない。SD7と新候補の実Windows抜挿し・読書きは未検証。
