# Native 1003: 実queue・scan・MTU・OIDの候補

これは68a6f16の残件を実経路へ接続するWiFi限定の変更。INFは
`11/06/2026,2026.11.6.1003`。1002固定SYSの再包装とは別候補であり、
WDK8.1最終link、Windowsによる受理、interface列挙、scan、接続・通信は未確認。
`INITIALIZATION-CONTRACT.md`の残件表は68a6f16時点の履歴として残す。

## 実装した範囲

- TXは64件のadapter-owned ring。worker所有の1件も容量に含め、NBL chainを
  分離して保持する。未送信の取消だけqueueから除去する。inflightはmatching
  TX_DONEまたは既存の失敗・epoch隔離を経て一度だけ完了する。TX_DONEと
  WTQCR/FFA page返却を混同しない。完了callback再入中もinflight枠を保持する。
- Send/Cancelのapi_refs、workerのidle event、Pause/Haltのdrainを連動。
  DISPATCH_LEVELでの即時／取消完了は対応flagを設定する。
- RXは独立した64組のnonpaged buffer/MDL/NBLを全確保後に広告する。
  一回のpollで最大64packetを受け、同期RESOURCES indicationが戻るまで保持。
  radio/key epoch失効後の旧bufferは通知しない。managementは実RX metadataと
  directed/broadcast filterを検査する。受信NBLをOSが保持する方式ではない。
- MTU2304、MPDU2336、Ethernet2318の実buffer、変換codec、長さ検査を揃える。
  Native非QoS dataはLinuxのTC1/AC1へ接続し、descriptor queue bitsをAC1にする。
  TC1の初期software予算は同型Linux defaultの36×13=468 pages。
  最大HIF2346bytesで19pagesを消費し、bus paddingは2560bytes。
  資源は実WTQCR AC1と共有FFAの両方が戻った分だけ回収する。
  Linux defaultを実機で測定済みのquotaとは扱わない。旧callerのTC0経路は保持。
- 4 SSIDをoffset/length検査して所有snapshotへ保存し、一回のgen3 scan command
  (CID3、4×36byte SSID、32channels、IE offset226)へ渡す。
  旧extended IE494 codecは互換用に保持するがNative1003では使わない。
  wildcardとspecific SSIDの混在は黙って捨てずNOT_SUPPORTED。
  特定BSSID/restricted/明示PHYリスト、接続中scanは引き続き非対応。
  完了通知はmatching scan event後のみ。cacheは明示FLUSH以外では保持する。
- excluded MACを4件実保持し、scan cache・AP選択・management受信へ適用する。
  現APが除外された場合は実radio resetとdisassociationを行う。
  desired/association BSSIDを接続先選択へ反映する。multicastは32件と実filter。
- hidden SSID directed probe、検証済み追加association IEの実frameへのappend、
  safe-mode時のRSNA-PSK/CCMP制限を実装。FIPS認証能力は広告しない。
- GEN/DOT11統計は実送信完了、受信通知、失敗、discard、no-bufferを計数。
  association統計は開始時snapshotとの差。未観測のretry/FCS/MIC/replayと
  OS handshake counterはDOT11_STATISTICS_UNKNOWN。ゼロで成功を装わない。
- NIC_POWER_STATEは実session stop/quiesceとcold startへ接続。鍵/PNはepochを
  越えて再利用しない。標準NDIS configuration APIでradio状態を保持する。
  保持OFF時は動的NVRAMからidentity/domainだけ読み、RFをONにしない。
  設定保存失敗はOID失敗とする（既に変化したRF状態をrollbackしたとは扱わない）。
- POWER_MGMT_REQUESTのnamed0(CAM)/8(FAST)/16(MAX)は実CID5/payload4byteへ接続。
  Linuxと同じnoNeedRespであり、成功は全HIF提出完了を表す。架空ACKは作らない。
  streaming中はCAMを適用し、解除時は保持要求を戻す。radio OFF中のpolicyは
  保持し、再起動・RESET後に再適用する。自律auto-PSは広告しない。
- Linuxの実beacon-timeout event0x13と最近2秒のpeer RX条件でpeer lossを処理。
  reason8はLinux同様除外。hostの無通信だけで勝手に切断しない。
  TX成功をRX観測時刻として使わない。

## 残る具体的な阻害・限界

Native `OID_DOT11_UNREACHABLE_DETECTION_THRESHOLD`の単位・既定値・wire意味を
取得できた一次資料で固定できていないため、supported listへ加えない。
Linuxの実beacon-loss処理を、この未実装OIDの成功に代用しない。

Native省電力値24および中間値からgen3の3profileへのmappingも固定できて
いないためNOT_SUPPORTED。named3値以外を推測で丸めない。
完全なExtSTA必須契約充足、modern standby、sleep/resume、WLAN列挙成功とはしない。
接続は既存の2.4GHz ERP20、OPEN/NONEまたはRSNA-PSK/CCMP、OS supplicantのみ。
WEP/TKIP/enterprise/PMF/HT/VHT/AMPDU/IBSSは追加しない。
PTKは実ADD_PKEY_DONE、GTKは既存gen3 noNeedResp提出条件を維持。
重複鍵のPN巻戻し防止とcontrolled-port検査も維持する。

## 一次ソース

同型公開Linux `CT3_GPL_LGPL_Src/kernel-4.4/drivers/misc/mediatek/connectivity/wlan/gen3/`:

- `include/nic_cmd_event.h`: CMD_SCAN_REQ_V2、CMD_PS_PROFILE_T、beacon-timeout event。
- `mgmt/scan_fsm.c` scnSendScanReqV2、`include/mgmt/scan.h`: SSID flagsとscan組立。
- `nic/que_mgt.c`: AIS非QoS/BE→TC1。
- `include/nic/nic_tx.h`、`nic/nic_tx.c`: TC1 count36、pages/frame13、AC1 map、
  WTQCR/FFA返却。`common/wlan_lib.c`: default Tc1Page設定。
- `nic/nic.c` nicConfigPowerSaveProfile、`include/wlan_oid.h`: CAM0/MAX1/FAST2。
- `nic/nic_rx.c` nicEventBeaconTimeout: firmware eventとrecent RX条件。

NDIS version/最低容量/構造の公式リンクはINITIALIZATION-CONTRACT.mdを参照。
Native省電力named値はWDK windot11.hと
[Microsoft mode status](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-_dot11_power_mgmt_mode_status_info)
を照合した。新しいWDIの契約をNDIS6.30へ代用していない。

## 主担当用の固定source build/sign

cleanなこのcommitのcheckoutで、既存のWDK8.1 ARM/clang/lld環境を使用する。
既存の親SYSは従来helperの3criticalflags、実hash、public interface blob一致を
満たす場合に再利用できる。親の再buildや実機操作はこの作業では行わない。

```powershell
& .\tools\ci\build-mt6630-runtime.ps1 `
  -OutputDirectory "$PWD\build\native1003-new" `
  -SharedParentDirectory "$PWD\build\existing-parent"
& .\tools\ci\package-mt6630-runtime-1003.ps1 `
  -BuildDirectory "$PWD\build\native1003-new" `
  -OutputDirectory "$PWD\build\native1003-signed-new"
```

出力dirは新規必須。既存のthumb2655開発鍵を使用し、SYS署名→新CAT生成→CAT署名。
1002専用scriptは変更しない。build manifestにsource commit/tree、3flags、
recipe/INF/SYS hash、実容量、未解決条件を保存し、署名manifestには元build
manifest hashと署名後SYS hashを保存する。実適用判定はINF1003と署名後SYSの
両方の一致が前提。旧SYSのCode31をこの候補の失敗として扱わない。

## host確認

所有ring、scan parser/実wire bridge、full-MTU NBL変換・HIF送信・matching完了・
独立credit返却、既存receive/HIF/data/WMT/calibration/diagnosticの限定回帰を行う。
ARM32/WinBlue設定で参照SDK headersによる変更TUのcompileも行う。
これはWDK8.1最終SYS linkと実OS上の動作試験の代替ではない。
