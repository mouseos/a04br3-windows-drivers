# TAB-A04-BR3 MT6630 / MSDC2 Windows向け基盤

このディレクトリはWi-Fiドライバではない。無線・SDIOコントローラにアクセスせず、
未完成の所有権や電源処理が実機上で有効になることを防ぐ。`wdf_sdio_probe.c` は
常にPrepareHardwareを失敗させ、MMIO、GPIO、送受信、NDIS登録を行わない。
対応する機種ACPI候補は別UEFI worktreeの `WifiSdioCandidate.asl` で、_STA=0かつ
どのfirmware volumeにも組み込んでいない。`MOU0011` は内部候補IDであり製品IDではない。

## 確認した事実と限界

- 端末のLinux DTS `arch/arm64/boot/dts/mediatek/a04br3.dts` はMT6630コンボ、
  MSDC2 4-bit SDIO、PMU_EN GPIO4、RST GPIO7、Wi-Fi EINT GPIO5を記述する。
  `mt8167.dtsi` のMSDC2は0x11170000+0x1000、GIC SPI109 level-low
  （ACPI GSIV 141）。共有INFRACFG reset領域、pinmux、時計はこの専有領域に含まれない。
- 実機過去記録 `port/gsi/logs/ios-feasibility/wifi-worker/implementation-report.txt`
  はCIS vendor 037a、device 6630、2 functionsを観測。CT3 Linux `sdio_detect.c` も
  037a:6630をMT6630と列挙する。これは同一端末の過去記録であり、Windowsで再取得した
  値ではない。MACアドレス・校正値をソースやバイナリに含めない。
- 機種UEFI付属DTSでは固定regulator 3.3V、PMU4、reset7 active-low、外部32k RTC、
  100ms startup、30ms post-powerが記述される。これは構成されたDTS値で、回路実測や
  電源操作の安全性の証明ではない。Linux 4.14端末DTSの200MHz上限と当該UEFI DTSの
  50MHz上限は別構成で、いずれもWindows用の承認済み設定ではない。
- 過去XNU成果はCMD52/53とMT6630のWMT/firmware研究を示すが、WindowsのSDバス、
  IRQ、同時アクセス、低電力遷移やネットワークの動作を証明しない。firmware選択は
  silicon revisionとの一致、配布権、端末固有校正を別途確認する。

## Windowsの境界

Microsoftの [SDカードドライバスタック](https://learn.microsoft.com/en-us/windows-hardware/drivers/sd/sd-card-driver-stack) と
[SDバスインターフェイス](https://learn.microsoft.com/en-us/windows-hardware/drivers/sd/opening--initializing-and-closing-an-sd-card-bus-interface)
に従い、ホストコントローラ提供とSDIO機能クライアント、NDIS無線miniportを別の
責任として設計する。手元WinPEにはARM版 `sdbus.sys` と `ndis.sys` が存在するが、
MSDC2ホストが利用可能であることやMT6630用Windows ARM32 WLANドライバの存在を示さない。
MT6630向けの利用可能なWindows RT ARM32ドライバは今回の一次資料調査では特定できていない。
デバイス固有ネットワークIDやMT6630と無関係な既存ドライバを推測して流用しない。

## 実装部品と検証

`identity.c` は供給されたCISのMANFID tupleを境界・重複検査し037a:6630を判定する。
`sdio_args.h` はCMD52/53引数とR5エラーを純粋な計算として扱い、送信はしない。
`transport.c` はcallbackに対してCMD53を分割し、範囲外・FIFO非整列を拒否、
転送失敗後はpoisonする。Windowsバス・MMIO・firmwareに接続していない。
`ownership.h` はhost、共有clock/GPIO、コンボ機能、firmware、IRQの各条件と障害後の
poisonを管理する部品であり、真の排他やpower lifetimeの証明ではない。
`make build/mt6630_identity_test build/mt6630_sdio_args_test` の後に両テストを実行する。
ARMクロスコンパイルとASan/UBSanはホスト模擬で、無線操作・ネットワーク成功ではない。

実装にはWindowsのMSDC2ホスト／SDIOバス、共有クロックとresetのブローカ、GPIO/EINT5
割込み、電源・32kとBluetooth等の共同所有、firmware／校正の動的取得、NDIS無線
miniport、電源／PnP停止と安全な復旧、署名・実機検証が残る。条件が揃うまでSTAや
PrepareHardwareの安全ゲートを外さない。無線設定・MAC・国別RF設定を推測しない。

`direct.c` は直列化済みの所有リースと同期callbackを前提にCMD52・R5を扱う。
世代変更・通信失敗・R5エラーでは所有状態をpoisonし、以後の要求を拒否する。
callbackの期限内完了、実際の排他と中断処理は呼び出し側の責務であり、
モック検証はWindowsバス接続や実機上の安全性を保証しない。
`power_plan.c` は許可された操作を呼ぶ前提の状態計画のみで、電源を操作しない。
`ndis_candidate.c` はWDK 8.1向け登録のビルド足場で、初期化を必ず拒否し、
Native 802.11・firmware・送受信機能を持たない。INFもない。

`msdc2_pio.c` は UniXNU-a04br3 ローカル未追跡ファイル `A04BR3SDIOPIO.h` のMSDC2 PIOを参考にした
参照ファイルのSHA-256は `c0b519f9a7595621eb95aa02f2c16d65c9893d144127dbdde449e2d8d39b50d7`。
CMD52/53部品で、順序保証付きI/Oと事前設定済みコントローラを呼出側が提供する。
既存MSDC2候補への配線・実機の所有はなく、IRQ停止、512バイトブロック設定、
同期直列化およびマッピング寿命は利用前提である。タイムアウト等でpoisonする。
`power_bridge.c` は外部brokerの排他リース、安定確認と待機時間を受け取り、
reset解除前に再確認する。電圧・レール・クロックは操作せずbrokerも未実装。
NDISのsendは将来の初期化変更時に要求が宙に残らないよう失敗完了するが、
有効なコンテキストの生成も送受信もなく、初期化は引き続き常時失敗する。

`calibration.c` と `nvram.h` は、起動時に供給される端末のNVRAM領域から
境界・bitmap・全体およびレコードのチェックサムを検証してWIFIレコードを読む。
Windowsの領域探索・読取providerは未実装で、実端末の校正値は同梱しない。
`wlan_firmware.c` は候補の実測revision、SHA-256 callback、MTKW形式とCRCを
検証する。SHA-256 provider、ファイル取得とfirmware転送は未実装。テストの
ハッシュcallbackは合成データ用で、署名や真正性の検証ではない。
`windows_msdc2.c` は独占資源と有効リースを前提とするWindows MMIO写像の
未接続部品。排他broker、IRQ停止、D0と取り外しの直列化が必要であり、
現行の `WifiPrepare` は呼ばず、常時失敗を維持する。物理的なsurprise removalと
並行するMMIOの安全性は保証されない。NDISとWindowsバスには未接続。

`lifetime.c` はCIS確認済みのSDIO制御要求を直列化された所有者へ通し、
停止時に要求受付を閉じてからIRQ/DPC排出、I/O排出、写像解除、リース返却を
順に呼ぶ。失敗後に同じ後始末を自動再実行しない。呼出元は排出対象が同じ
直列化ロックを待たないように設計する必要がある。これはWindowsのD0・IRQ
ハンドラや実brokerへの接続ではなく、無線firmwareやNDISの準備完了も示さない。

`wdf_wifi_parent_probe.c` は共有親のWi-Fi interfaceを問い合わせて型を確認する
オフライン試作で、取得・MMIO・IRQ登録・NDISを行わずPrepareHardwareを失敗させる。
共有親もリースを常時拒否し、現在のACPIに親子接続はない。実機試験用SYSではない。

`enumerate.c` と `control_start.c` は条件付きでCMD0/3/5/7・CMD52を実行し、
取得したCISの037a:6630を確認してSDIO制御と停止処理へ接続する。OCRは実際の
電源レールについて呼出元が確認した範囲との共通部分だけを使い、電圧を操作しない。
512-byteブロック転送は機能別FBRの設定と読戻しを確認するまで拒否する。
実際の電源・pin/clock設定、Windows PnP/D0/IRQ、NDISへの接続は未実装。

`wmt_bridge.c` はSDIO寿命管理に既存WMT/STP・WLAN HIFの処理を接続する。
実機からのrevision照会、検証済みbundleのWMTパッチ送信とACK/credit確認、
WLAN firmwareの形式・ハッシュ検証と送信、単一消費者によるread-clear statusと
FIFO受信を扱う。bundleの所有・SHA-256と起動設定は呼出側の責務であり、
校正送信、実IRQ、Windows PnP、Native 802.11/NDISには接続していない。
実機への送信実績はない。参照元は未追跡XNUファイルでハッシュを
`vendor_xnu/provenance.json` に固定した。`make test-mt6630-firmware` の
Linuxソース照合は `A04BR3_BOARD_SOURCE` 指定時に実行される。
統合リンク確認SYSは開始を常に拒否し、未署名・INFなしで配備できない。

`native_scan.c` は `DOT11_SCAN_REQUEST_V2` の可変長配列を境界検証してコピーし、
`native_scan_bridge.c` は外部から与えられた実際の規制・channel方針を使って
firmware scanを開始する。対応するSCAN_DONEが届いた後だけSCAN_CONFIRMを
通知する。対象は単一SSID・wildcard BSSID等の限定された部分集合で、現在の
NDIS OID handlerやWindowsのwork itemには未接続。SCAN timeout/中断時の失敗通知も提供するが、BSS一覧は未実装である。

WLANのEthernet HIF送受信部品は既存Linux gen3実装を参考にしたローカルXNU由来で、
認証・鍵・暗号・リプレイ検証を行う状態所有者が別途必要。Microsoftの
[Native 802.11受信規約](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-dot11_extsta_recv_context)
は802.11 packetと無線OOB情報を要求するため、このEthernet viewを直接NDISへ
渡さない。MPDU形式とPHY/RSSI等の実情報、NET_BUFFER_LIST寿命、暗号処理の
対応確認が必要。現段階はNDIS送受信や実IRQ/PnPに接続していない。

`native_tx.c` はNative 802.11送信のうち、非QoS・非断片のToDSデータと
RFC1042 LLC/SNAPを、接続中AP・ローカルMACを確認してEthernet HIFへ変換する。
それ以外の形式は拒否する。NDISのNET_BUFFER_LIST・送信OOB・鍵設定には
未接続であり、WPAやQoSを含むNative 802.11送信全体の実装ではない。

`native_nbl.c` は単一NET_BUFFERの送信を安全なスナップショットへ取り込み、
実TX_DONEでのみ成功完了する。取消時は排出済みを前提に失敗完了し世代をpoisonする。
`native_receive.c` は実MPDUと同じ受信のPHY・周波数・RSSI・速度等が揃った場合だけ
ExtSTA OOB付きで同期通知する。現在のHIFはEthernet表示でありMPDU/OOBを供給しない。
両者とも実NDIS handler未接続。station/auth/CCMP key bridgeは実状態・実イベント・
信頼できるsupplicantを前提とし、ACKや鍵の成功を合成しない。実運用の鍵配送、
再鍵、OS認証接続は未実装。

Linux構造体比較は `A04BR3_GEN3_SOURCE` に端末gen3ソースのディレクトリを指定して
行う。指定しない場合は当該一次資料照合テストのみskipする。

`native_oid.c` はPASSIVE_LEVELの直列化されたwork itemから実際の
`OID_DOT11_SCAN_REQUEST` をSDIO scan処理へ渡す。入力長不足のBytesNeededを返し、
送信成功後にOIDを完了し、別途firmwareのSCAN_DONEでSCAN_CONFIRMを通知する。
統合リンク確認では `A04_NDIS_OID_BRIDGE=1` によりMiniportOidRequestへ配線するが、
MiniportInitializeは従来どおり必ず失敗する。他のOIDは未実装。
`native_oid_async.c` は高IRQLのOIDをNDIS work itemでPASSIVE_LEVELに送り、
実行前の取消と終了時の排出を扱う。D0とデバイス全体の排他・rundown、
実機での競合検証は未実施。

`ndis_candidate.c` の初期化はNDIS adapter contextの確保・登録属性の設定と、
統合ビルドでのOID work item状態の初期化を行う。初期化失敗時とHaltでは
OID完了通知の復帰まで排出した後にcontextを解放する。ただし一般属性・
Native 802.11属性、共有親からの所有権取得、ハードウェア起動は未実装であり、
初期化は引き続き失敗する。NDIS送受信・D0・IRQの接続も未完成である。
この処理はMicrosoftのMiniportInitializeExとMiniportHaltExの資源寿命規約に
従い、端末Linux gen3の `os/linux/gl_init.c` のprobe失敗時の後始末を参照した。

統合ビルドでは初期化時にNDISからPDOを取得し、既存の共有親Wi-Fi
インターフェイスを `IRP_MN_QUERY_INTERFACE` で問い合わせる。取得できた参照は
失敗時・停止時に返す。問い合わせはリース取得や電源・MMIO操作を行わず、
親PDO接続と実機上の応答は未検証である。一般属性・ExtSTA属性の正しい
能力値と必須OIDが揃うまで、問い合わせの成功を初期化成功とは扱わない。

`native_hif_rx.c` はLinux gen3の受信記述子から、未変換・未暗号化・非QoS・
非断片の2.4GHzユニキャストFromDS MPDUに限り、実RCPIとchannelを抽出して
ExtSTA OOB通知へ渡す。PHY/rate indexは実際にadvertiseされた表を呼出側から
供給し、未知値や未観測値は拒否する。HIFからraw MPDUが実際に届くこと、
規制channel・rate表、NDIS receive handlerとの接続は未検証・未完成。

`nvram_sector.c` は検証済みの同一物理ディスク上のNVRAM領域について、
同期512バイトセクター読取りを既存解析器の任意バイト読取りへ変換する。
領域のディスク範囲・サイズ・整数境界を検査する。Windowsのディスク探索、
GPTの一意性検証、I/O提供、停止時の直列化は未接続であり、この部品だけでは
実機から校正値を取得できない。合成NVRAMによるホスト試験のみである。

`windows_nvram.c` は、システムボリュームが単一ディスク上にあることを確認し、
Windowsのディスクinterfaceとstorage device numberを照合した後、GPT layoutから
一意の `nvram` を探して読み取る。I/OはNDIS work itemで実行し、校正は検証後のみ
callbackへ渡し、作業メモリを消去する。対応するSystemRootの名前空間は
`\\Device\\HarddiskVolumeN` 形式に限定し、不明な構成や複数候補では失敗する。
診断専用 `A04_NDIS_NVRAM_PROBE=1` ではNDIS初期化時にこの処理を実行した後、
従来どおり初期化を拒否する。INFや署名済みパッケージはなく、実機では未検証。
ディスク識別、非同期停止、校正反映、共有電源・SDIO・NDISの実動作を保証しない。

`board_sequence.c` はローカルXNUのSDIO起動手順を移植した未接続部品。TOPCKGEN、
GPIO4/7、GPIO68–73、pull/IES、MSDC2 reset/clockを順に扱い、呼出側による共有資源、
combo、RTC、IRQの排他的所有確認がなければ開始しない。途中失敗はpoisonし、
リース解除や電源復旧を推測しない。現行の共有親はD0とWi-Fiリースを拒否するため、
本コードは実ハードへ到達しない。EINT、資源broker、FW取得・開始は未接続。
起動手順は公開済み [mouseos/xnu_kernel_benesse_a04br3 f5d43bf](https://github.com/mouseos/xnu_kernel_benesse_a04br3/tree/f5d43bf1677955e7e367545b9bbbd6449776cdda/iokit/Kernel/arm/sdio)
の `A04BR3SDIO.cpp` と `A04BR3SDIOPads.h` にも一致し、両ファイルの
SPDXはBSD-2-Clause-Patent。前者の全体SHA-256は
`baf7ed5d72055e3f6ec20034caf16314120aa223c8806075588a1d06d5d8074f`、
後者は `884aeb21190d9689b1e8cdb6f990d378be5665939b22e780709c48eb4c131bdd`。
最初に参照した別版の前者は
`c7fa60811d1b44e354d538be54e0e0e8399f51cd292e550b9f39dfeeb9949de3`。

`bringup.c` は同一世代の共有リースを呼出側が確認できる場合に限り、上記ボード
起動、SDIO列挙、CISの037a:6630照合、制御I/Oの寿命初期化を連結する。OCR範囲と
必要な停止コールバックは最初のMMIO前に検査し、成功するまでカード情報を公開しない。
開始後の所有権喪失や列挙失敗はboardとownerをpoisonし、リースや電源を勝手に解放しない。
ボード手順中も各レジスタ操作・待機の前後でリースを再確認する。実際の非取消リースや
物理除去との排他をこの判定だけで保証しない。

ホスト統合試験では合成CISを使いボード起動→列挙→CMD52→停止と、無効OCR、CIS不正、
リース喪失を確認する。現行のWindows共有親・NDISからこの経路はまだ呼ばれず、FW・
校正・接続、実機の電源とIRQ、Windowsでの署名受理は未確認。開始拒否を維持する。

`resource_lease.c` は外部の実資源providerを呼ぶ直列化済み所有管理である。
combo、firmware、rail、RTC 32k、GPIO/clock、MSDC2、IRQの順に取得し、
取得成功後だけ `a04_wifi_bringup` の `same_lease` に使える世代を発行する。
停止時はquiesceで受付を閉じ、IRQとI/Oを排出する。呼出側がMSDC2をunmapした後、
releaseで逆順に各providerへ返す。unmapのない場合だけstop一括APIを使う。
途中で失敗した操作は部分的に実行済みかもしれないため自動rollbackせず、
残る資源を保持してFAILEDにする。返却はproviderのリース返却であり、共有レールや
32k出力を無条件に切断する指示ではない。呼出側は全操作とPnPを同じ排他機構で
直列化し、停止とsurprise removalの寿命を保証しなければならない。
DTSが示すRTC 32k・GPIO4/7・MSDC2の接続とXNU board sequenceがこの分割の根拠で、
資源の物理的な存在や独占状態をソフトウェアフラグから証明するものではない。
実provider、IRQ接続、親のD0、firmware取得、NDIS初期化には未接続である。

`windows_board.c` は親のWi-Fi interface v2とMSDC2写像をボード手順へつなぎ、
親の世代を検査しながらGPIO/TOP操作とMSDC2操作を分ける。共有レジスタの
read-modify-writeは親がtouchと同じmutex内で実施し、対象offset・mask・値を
XNU由来のボード手順に限定する。列挙後のWMT version照会までの呼出しも提供する。
この接続の試験はモックのみ。親は引き続きD0とAcquireを拒否し、実リース、
32kと電源の保持、割込み、firmware取得およびNDIS初期化は実装されていない。
これは実機へ導入する候補ではない。親PnPと子の排出を直列化しても物理的な
surprise removal中のMMIO例外を防ぐ証明にはならない。

`windows_firmware.c` はPASSIVE_LEVELで固定パスのWMT bundleとWLAN firmwareを
書込み・削除の共有を許さず開き、長さ制限付きコピーを保持する。WMTには呼出元が
出自を固定したSHA-256を要求し、WLANは実際に問い合わせたWMT revisionと既存の
既知ハッシュを確認する。両方の形式を事前検証した後に限り同期転送を呼ぶ。
どちらも別途供給が必要であり、WMT hashをユーザー入力や未検証のファイルから
採用してはならない。ハッシュ一致は配布権やデジタル署名の証明ではない。
現状このAPIを呼ぶ運用NDIS経路も実親leaseもない。SDIO初期化に使用するGPIO4の
電源投入後待ちは、UEFI配下の機種DTS `startup-delay-us = 100000` に合わせた。
同DTSはRTC `ext_clock`、GPIO7 reset、MSDC2 IRQ、EINT5を別資源として記述し、
XNUの実装はRTC出力の維持を前提にしている。RTCのWindows所有者・共有者とIRQ排出の
保証が未確立のため、D0とAcquireの拒否は維持する。試験署名はCI専用の自己署名で、
Windows RTの信頼・PnP開始・実機動作を意味しない。
