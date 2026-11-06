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

`windows_diagnostic.c` は実リースと写像を保持する呼出元向けに、ボード初期化、
実機WMT版の照会、固定ハッシュで検証したファームウェア投入を順に接続する。
OCR・WMTハッシュ・起動設定は検証済みの呼出元が供給する。失敗時は中途半端な
資源返却や再試行をせず回復判断に残す。成功時の停止は既存lifeのIRQ排出、I/O排出、
写像解除、リース返却順序を使用する。現状の親D0/Acquireは拒否し続け、実RTC保持、
GPIO・IRQ provider、NDIS初期化への接続は未完成である。この関数も実機未検証であり、
ファームウェア投入成功はネットワーク動作を意味しない。

診断開始後にlifeが初期化済みの場合、ファームウェア投入やブリッジ初期化の失敗後も
stopから既存のIRQ・I/O排出と資源返却を試みられる。life初期化前の失敗は従来どおり
自動返却しない。返却の可否は実providerの契約に依存し、失敗時はFAILEDのままとなる。


機種用 Linux 4.14 の `drivers/mfd/mt6397-misc.c` は MT6392 を明示的に
扱い、RTC PDN1 bit 8、RTC CON bit 5、WRTGR、BBPU CBUSY を用いる。
`A04_MT6392_RTC32K_DIAGNOSTIC=1` の PMIC 補助関数は、共通 WACS2 lock
と他の RTC writer の停止を呼出側が保証する前提で user bit を保持する。
既存 bit は所有せず拒否し、失敗時は poison する。解放時は自分の bit
だけを消し、他の利用者や firmware を考慮して RTC 出力は停止しない。
まだ親 lease や NDIS から呼ばれず、外部 writer の排他、物理出力、
Windows 電源遷移は未確認。既定ではコンパイルされない。

`A04_SHARED_WIFI_IRQ_DIAGNOSTIC=1` は機種 DTS の MSDC2 SPI 109 に
対応する GSIV 141 を子 PDO の排他的な level IRQ 候補として追加する。
別の combo GPIO/EINT5 ではない。Windows の要求記述子は極性を指定できず、
実際のルーティング・active-low・ISR/DPC と停止時の排出は未確認。
共有親の Wi-Fi Acquire/D0 拒否は引き続き残る。


`A04_WIFI_SESSION_DIAGNOSTIC=1` は、参照を保持する呼出元から親 interface を
受け取り、実 lease の取得と世代確認、masked IRQ の provider 準備、MSDC2
写像、既存 firmware 診断を順に呼ぶ接続部品。停止は IRQ 排出、I/O 排出、
写像解除、親 release の順で、失敗時は残る資源を隔離する。初期化途中の
不確定状態は自動 rollback しない。親 lease は RTC・combo・IRQ 等の実所有を
保証しなければならず、現行親は Acquire を拒否する。`prepare_masked_irq` と
排出 callback の実 Windows provider、親による RTC 保持接続、ISR/DPC は未実装。
本接続は既定ではコンパイルされず、host mock と ARM32 リンク検査のみ。

WDK の [interrupt enable/disable](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/enabling-and-disabling-interrupts)
では D0 停止時に PreInterruptsDisabled、EvtInterruptDisable、D0Exit の順で
呼ぶ。[EvtInterruptDpc](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfinterrupt/nc-wdfinterrupt-evt_wdf_interrupt_dpc)
は複数 ISR が単一 DPC に統合され得ると規定するため、無効化だけでは DPC・
work item の排出を示さない。Linux MT8167 の MSDC_INT は SDIOIRQ bit 7 を
W1C とし、PIO の command completion bits と別々に扱う必要がある。物理 IRQ
配送・極性や surprise removal と MMIO の競合は未検証。

`A04_WIFI_IRQ_DIAGNOSTIC=1` の `windows_irq.c` は、排他的に割当てられた
MSDC2 の raw GSIV 141 と translated IRQ を受け取り、IoConnectInterrupt で
ISR を登録する。ISR は INTEN bit 7 をマスクし、SDIOIRQ bit 7 だけを W1C
して DPC を投入する。DPC は DISPATCH_LEVEL の通知のみであり、PIO は行わない。
再 arm は呼出元が明示する。停止は KeSynchronizeExecution でマスク・受付停止し、
IoDisconnectInterrupt、KeRemoveQueueDpc、KeFlushQueuedDpcs の後に参照を消す。
I/O work item の排出は別途必要。raw vector、排他、translated affinity、level
を検査するが、実際の極性や配線、surprise removal と MMIO の安全性は未確認。

両診断フラグの `windows_session_irq.c` は session の接続・排出 callback を
この provider へ接続する。MSDC2 を写像する前に source がマスク済みである
根拠は別の `pre_mask` provider が必要で、存在を仮定しない。親の Acquire は
なお拒否し、RTC の取得・解放は親 lease に実接続されていない。ISR/DPC・
firmware 診断と実 IRQ の同時運用も未実機検証で、インストール用ではない。

`windows_irq_work.c` は DPC 通知を一回限りの WDM work item に渡し、IRQ と
DPC の停止後に完了イベントを待って work item を解放する。複数通知や処理
失敗は fault として隔離し、再 arm による再利用はしない。firmware の実処理は
呼出側の callback が担う。通知前に割込み source の排他・マスクを実証する
provider、親 RTC lease、実機配送は未接続のため、本構成を実行可能とは扱わない。


診断セッションはホスト範囲を写像した後、排他的 IRQ provider が INTEN の
SDIO bit をマスクし接続してから PIO を有効化する。起動時の pre-mask 外部
callback は不要になった。中途失敗時は写像を隔離し自動解放しない。
`windows_irq_firmware.c` は一回限りの work item を既存 WMT bridge の WLAN
read-clear status 読取りへ接続する。競合時に待たない共通 try-lock を要求し、
状態を保存するが、処理済みでない status は acknowledge しない。現在は
この work item を実行する子ドライバ・親の RTC lease・親 Acquire は未接続。
診断専用で実機・Windows IRQ 配送と firmware 状態は未検証。

`A04_MT6392_RTC32K_DIAGNOSTIC=1` の共有 PWRAP owner は RTC の取得・解放を
親の同一 mutex 内で実行する。これは他の親クライアントとの直列化であり、
EL3・firmware や別の RTC writer の排他を保証しない。Wi-Fi の親 Acquire は
依然拒否し、この補助関数は親 lease に接続されていない。

Linux CT3 4.4 `wmt_plat_alps.c` の RTC 初期化と `wmt_detect_pwr.c` は
`RTC_GPIO_USER_GPS` を要求する。Windows が使用する user bit 8 は独立した
予約であり、既存 GPS bit の消去や所有権移転を意味しない。Linux 4.14 の
`mt6397-misc.c` は MT6392 に適用され、PDN1 と RTC_CON を WRTGR で確定する。
Windows ARM32 機種 UEFI（調査時 HEAD `e8af0fb0`、作業ツリーの完全な出自は
別途確認）では `A04BR3RecoveryDxe` の ExitBootServices callback が WACS2 を
経由して RTC_PDN1 bit 13 を設定する構成がある。これらはコード上の事実で、
今起動中の構成や EL3 が RTC・PWRAP を解放した証明ではない。

共有親の PWRAP owner は診断 RTC lease の保持中、または途中失敗で状態が
不確定な場合に lease を記録し、`ParentUnmap` はその間の PWRAP 解除を拒否
する。実際の PnP resource が保持される保証や物理除去の安全性ではない。
親 Acquire、外部 writer の排他、firmware と GPIO の引継ぎは未確立のため、
Wi-Fi 診断は引き続き開始しない。

RTC lease が残る場合、PWRAP owner の明示 stop は拒否して online を維持する。
取得中に WACS が poison された場合は lease を不確定として保持する。これは
不確定な取引を完了済みと扱わないための局所的な寿命処理であり、UEFI・EL3・
他のファームウェアからの所有移譲の証明や、親 Wi-Fi Acquire の許可ではない。

`tools/winpe/mt6630-pnp-uart.c` は子 PDO `A04BR3\\MT6630SDIO` の存在と PnP
状態を照会し、TOPRGU の既存 UART 送信口へ `RTC=UNKNOWN SESSION=UNOBSERVED`
を含む行を渡す。インスタンスが複数の場合は曖昧として拒否する。UART の
IOCTL 成功は送信受付であって、ホスト側受信やセッション起動の証明ではない。
子ドライバから session を起動する経路、RTC 引継ぎと実 firmware 実行は未完成。

firmware 診断はボード初期化前に固定パスから WMT と WLAN の snapshot を
取得し、WMT の固定 digest を照合する。取得・照合に失敗した場合、board
初期化と SDIO 列挙を行わない。実機 identity に依存する bundle・WLAN の
選択とハッシュ検証は列挙後、転送前に行い、snapshot は終了時に消去する。
これは親 Acquire、IRQ の接続、WDF 子ドライバや停止時の資源隔離を完成させる
ものではなく、実機試験も行っていない。

共有親 interface の Acquire 失敗は、lease やハードウェア変更が残らない契約。
現行親は操作前に無条件拒否する。session はこの拒否を REJECTED として区別し、
成功応答後の無効な generation や失敗は QUARANTINED のまま保持する。
WDF 子による参照寿命・開始・停止は未接続で、上記の区別自体は実機動作や
PnP resource が quarantine 中に保持される保証にはならない。

`A04_WIFI_WDF_LEASE_DIAGNOSTIC=1` の `wdf_sdio_probe.c` は、PrepareHardwareで
親interfaceを照会し、D0EntryでAcquireを試す限定診断である。現行親は副作用なく
拒否する。予期せずAcquireが成功してもMMIO、IRQ、SDIO、firmwareには触れず、
有効なgenerationをReleaseQuiescentに返してD0Entryを失敗させる。成功時の
無効generationやrelease失敗は状態不明として親オブジェクトのinterface参照を
意図的に保持する。これはPnP資源の保持やleaseの復旧を保証せず、契約違反時に
参照が残る。WDFはD0Entry失敗後にD0Exitを呼ばないため、ReleaseHardwareで
正常に返却済みまたは拒否済みの参照だけを解放する。PrepareHardware失敗後も
ReleaseHardwareが呼ばれる。既定OFF、INFなし、無線機能なし。D0Entry成功や
実機動作の検証ではない。強制削除と実際のWi-Fi起動の安全性は未完成。

出典: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nc-wdfdevice-evt_wdf_device_d0_entry と https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nc-wdfdevice-evt_wdf_device_release_hardware

WDF のハードウェア診断経路は `A04_WIFI_WDF_HARDWARE_DIAGNOSTIC`、
`A04_WIFI_WDF_LEASE_DIAGNOSTIC`、`A04_WIFI_SESSION_DIAGNOSTIC`、
`A04_WIFI_IRQ_DIAGNOSTIC` の明示的な有効化を必要とし、既定では無効。
`wdf_hardware_config.h` の検証済み設定も既定で無効である。出典を確認した
WMT digest、OCR、基板設定がない場合は親取得前に拒否する。親が取得を拒否
した場合は work item を解放し、MMIO・IRQ・firmware のアクセスを行わない。
取得後は session を通じて firmware snapshot、masked IRQ、起動、IRQ の一回の
観測へ進み、D0 終了では IRQ、work item、MMIO、lease の順で停止する。
起動途中の失敗、停止失敗、突然の取り外しでは IRQ と work item を切り離し、
安全性が確認できない lease・親参照を保持する。親参照は PnP リソースの保持を
保証しないため、この異常経路を実機で安全と判断してはならない。割り込みは
一回観測後に再有効化しない。現在の親は lease を付与せず、実機での動作、
割り込み配線、電源・停止・削除時の安全性は未検証である。NDIS 機能はない。
`make test-mt6630-wdf-hardware` は模擬した WDF、session、IRQ による試験である。

統合診断パッケージは hardware・session・IRQ のコードをコンパイルし、SYS と
INF/CAT をテスト署名する。`A04_WIFI_WDF_CONFIG_VERIFIED=0` と、実際の共有親の
Acquire 拒否は変更しない。WMT の digest は固定 vendor revision
`0c756f413e7011231c8fb793af0930311023f68d` の二つの patch から生成した bundle
`4af7996b89ef110d8f270877f61aaf0b90fde21b6c5b0ed23ccbd32c2989dcee` に固定。
これは入力の識別であり、電圧・起動設定・所有権の証明ではない。
`tools/winpe/prepare-mt6630-firmware.py --output <empty-dir>` は固定元の SHA-256 を
確認し、別途ステージする二つのファイルを生成する。実行環境の `Windows\System32\drivers\a04br3`
に置く想定だが、実機の変更や配布許可を意味しない。firmware は CI パッケージに含めない。

`mt6630-pnp-uart.exe` は既存 TOPRGU UART 所有者を通して 96 バイト以内の行を
二つ送る。子 PDO の PnP status/problem と firmware ファイルのサイズ・Win32 エラー
のみを示す。ファイルサイズはハッシュ照合やロード成功の証拠ではない。UART の
IOCTL 受理もホストでの受信を証明しない。session・親 lease・IRQ 状態は PnP の
値から推測しない。kernel の DbgPrint は UART に自動転送されるとは限らない。
WLAN snapshot も固定 SHA-256 `07fe1a225e6b9de6b310894db3b40bcc1cb66107b913e4c43eb6a62089aada22`
と照合し、boot 前に再照合する。固定ハッシュの一致は入力の同一性の確認であり、
実機の revision、送信許可、firmware 稼働の確認ではない。

### 所有権引継ぎの未解決条件と拒否診断

既定 OFF の `A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC=1` は親の Acquire 拒否時に、
未証明の firmware・EL3、RTC、clock、GPIO、combo、IRQ、PnP の各領域を
DbgPrint に出す。レジスタ読書き、RTC 取得、lease 発行や証明済みという記録は
行わない。DbgPrint が UART へ配送される保証はない。

実際の引継ぎには、UEFI とそれ以前の writer が対象資源と処理を停止したこと、
EL3 等の並行 writer がいないこと、RTC の user bit と clock・GPIO の他利用者を
共有親が仲介できること、MSDC2・combo・firmware・IRQ の占有、D0・surprise
removal 時の I/O・IRQ 排出と PnP 資源寿命が、同一起動・同一世代について確認
できる搬送路を要する。ExitBootServices の到達や ACPI の列挙、設定フラグ、
固定文字列や自己申告だけでは証拠にならない。引継ぎ途中の障害は不確定として
保持し、証拠のない解除や別の利用者の RTC bit 消去を行わない。

現行の機種 UEFI には EBS 時の RTC_PDN1 bit 13 更新経路があり、EL3・外部 writer
の停止は確認できない。UEFI から信頼できる Wi-Fi 所有移譲を示す経路も存在しない。
そのためこの変更は実 handoff や親 lease を実装したものではなく、親 Acquire は
従来どおり拒否する。共有親の署名済み診断ビルドは D0 も既定で無効であり、
実機 PnP・署名受理・UART 配送はいずれも未検証。

### combo 停止確認と lease 解放

CT3 の Linux 4.4 `drivers/misc/mediatek/connectivity/common/common_detect/wmt_detect_pwr.c`
では MT6630 系の電源投入時に `RTC_GPIO_USER_GPS` で RTC 32 kHz を要求し、
PMU・reset を順に操作する。同ファイルの停止では LDO・reset・PMU を下げる。
`common_main/platform/wmt_plat_alps.c` も同じ RTC user を要求する。Linux の
手順は Windows の排他所有、ファームウェア停止、基板上の信号到達を証明しない。
`port/gsi/kernel-4.14/drivers/mfd/mt6397-misc.c` では RTC PDN1 の user bit と
RTC_CON の共有状態を mutex で更新する。Windows の WACS lock は EL3 等との
排他を保証しないため、そのまま引継ぎ証明には使えない。

session 停止は IRQ と非同期 I/O の排出後、combo と firmware の停止を確認する
`quiesce_combo` callback が成功した場合だけ MMIO unmap と親 release に進む。
現行 WDF 接続には確認可能な provider がないため、その callback は拒否し、
停止状態を隔離して lease と写像を保持する。host の注入 callback による成功・
失敗の試験は実デバイスの停止根拠ではない。RTC の取得や放棄も有効化していない。

### Linux と機種 UEFI の書込み経路（ソース調査）

- CT3 Linux 4.4 `drivers/misc/mediatek/connectivity/common/common_detect/wmt_detect_pwr.c`:
  power-on で任意 LDO、RTC 32 kHz、PMU、reset を扱い、power-off では任意 LDO、
  reset、PMU の順に low を要求する。機種 `port/gsi/kernel-4.14/arch/arm64/boot/dts/mediatek/a04br3.dts`
  は PMU=GPIO4、reset=GPIO7、Wi-Fi EINT=GPIO5 を記述し、combo LDO pin は記述しない。
- 同 Linux `common_main/platform/wmt_plat_alps.c` は RTC を `RTC_GPIO_USER_GPS` で
  有効化する。`include/linux/mfd/mt6397/rtc_misc.h` では GPS user は 9。別の
  Wi-Fi user bit や他の user を Linux 側から引き継いだと仮定しない。
- Linux 4.14 `drivers/mfd/mt6397-misc.c` は同一 mutex で RTC_PDN1 と RTC_CON を
  read/modify/write し、WRTGR を発行する。ローカル mutex は Linux 以外の writer、
  または UEFI/EL3 との排他の証拠ではない。
- `uefi-a04br3` の `WindowsArm32/BootShim/EarlyRecovery.c`、`WindowsArm32/PrePi/Pi.c`、
  `Library/A04BR3PlatformSecBoardLib` は各 build flag に応じて SEC/PrePi で RTC や
  spare register を操作する。`A04BR3TestCycleDxe` は `A04BR3_TEST_CYCLE=1` の
  場合、entry と ExitBootServices callback で RTC_PDN1 bit 13 を更新する。
  `ThreeCpuDxe/A04BR3BootTraceDxe.c` にも条件付き bounded recovery の更新がある。
- UEFI `DisplayInitDxe` は PWRAP WACS2 を使用し、条件付き `TouchPowerDxe` は
  BeforeExitBootServices で VGP 電源を WACS2 で更新する。各イベント通知の存在、
  同期、終了順序だけでは未知の EL3 writer や物理 GPIO の排他は証明されない。
  上記はソース上の経路であり、現在端末に書かれているビルド設定の観測ではない。

診断 session の IRQ と非同期 I/O が排出され、親の実 lease と board ready が
有効な場合、`a04_wifi_board_poweroff_signals` は GPIO7 reset low、GPIO4 PMU low
を順に要求する。GPIO の所有権が無効、起動途中、surprise removal、書込み失敗
では停止または poison し、自動で次の操作に進まない。書込み受付は pad 電圧、
RTC 停止、WLAN/Bluetooth/GPS の活動停止を証明しないため、現状は成功しても
`quiesce_combo` が失敗を返して lease を保持する。親 Acquire と UEFI 移譲は
未成立であり、この条件付き power-off は現時点では実デバイス上で呼ばれない。

### 実行時NVRAMの転送段階

Linux機種ドライバ [a04br3-linux-drivers](https://github.com/mouseos/a04br3-linux-drivers)
の `3d2c74e252d6f34a5980deda96bdc04064108d74`、
`pwrseq/a04br3-mt6630-pwrseq.c` に合わせ、起動はreset GPIO7を先にassertし、
PMU GPIO4を落として250ms待つ。その後PMU有効化・100ms・reset解除・30msの順とする。
停止時もreset、PMUの順で落として10ms待つ。Linuxでの150/200msの差の観測は
Linuxソースに記載された根拠であり、Windows実測ではない。RTC32Kとcombo全体の
排他的所有は依然呼出側の前提である。

`windows_nvram_reader.c` はNDIS work itemから独立したPASSIVE_LEVELの読取り処理。
システムボリュームの単一disk extentに一致するdisk interfaceを選び、GPTの`nvram`
領域を探索する。固定disk番号、バックアップファイル、個体MAC・校正値の定数は使わない。
領域・bitmap・全データとWIFIレコードのチェックサム検証は既存`nvram.h`を使う。
失敗時は出力を消去する。従来のNDIS work itemもこの同じ実処理を呼ぶ。
WinPEのRAM disk等、システムボリュームから対象diskを特定できない構成は拒否する。

`windows_calibration.c` はこの動的読取りを`a04_wmt_bridge_load_calibration`へ渡す。
WLAN起動済み・同一の有効leaseを要し、初期化時の送信資源返却を観測してruntime
budgetへ切り替え、元の512バイトを変更せず送る。根拠はLinux gen3
`common/wlan_lib.c` の `wlanLoadManufactureData` 手順9と
`include/nic_cmd_event.h` の `CMD_ID_SET_NVRAM_SETTINGS=0x48`、
`CMD_NVRAM_SETTING_T`。転送は同期HIF送信で、完了後に作業バッファとHIF payloadを
消去する。失敗時はbridgeとleaseをpoisonし、二重送信と自動再試行を拒否する。

`A04_WIFI_CALIBRATION_DIAGNOSTIC=1` をcoreビルドへ明示した場合だけ、既存の
`windows_diagnostic_start` がfirmware起動後にこの処理を実行する。既定OFFで、
親Acquire・board verified設定のゲートは変更しない。
`calibration_submitted` はHIFへの送信受付だけを表し、firmwareの適用ACKではない。
Linux手順のTX power・EFUSE・国別domain等の残り、NDIS登録・認証・通常通信、
PnP/電源遷移と所有権引継ぎの実機検証は未完了。これだけでRF送信を許可しない。

`make test-mt6630-runtime` は合成NVRAM→実parser→HIF wire payload、二重送信拒否、
途中失敗と消去、Windows読取りadapter、診断開始失敗時の停止を検証する。
`make test-mt6630` は従来の部品試験も実行する。これらはホスト上の模擬I/O試験で、
Windows WDKビルド・SYS署名・実機無線動作を証明しない。
INF表示名は `MediaTek MT6630 Wireless LAN SDIO Adapter`。
