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

## WDK ARM runtimeとscalar codegenの修正

b6b715fの実WDK compile成功後、linkが `ntoskrnl.lib(helpexcept.obj)` の
`fphelper_r/helper_div` から参照する `__security_push_cookie` /
`__security_pop_cookie` 未解決で停止した（主担当回収stderr確認）。
この名前だけからWiFi sourceに浮動小数点演算があるとは断定できない。
64bit整数除算もARM runtime helperを要求しうる。自前のcookie/除算/FP成功stub、
user-mode CRTで置き換えず、WDK8.1 `km/arm/libcntpr.lib` を既存の
`ntoskrnl.lib` / `hal.lib` / `bufferoverflowfastfailk.lib` と一緒にlinkする。
同repoの既存GE8300 providerもこのkernel runtime構成を使う。
`-fno-stack-protector` はWiFi自身の生成設定であり、WDK archive内部のGS参照を
消すものではない。runtimeの本来の例外・cookie処理を保持する。

大きいbufferのcopy/clearとqueue処理がoptimizerによりVFP/NEONへ変換されないよう、
Nativeとcoreの全TUを `-O2 -fno-vectorize -fno-slp-vectorize
-mno-unaligned-access -mfpu=none -fno-stack-protector` で生成する。
vectorizerを止めるだけでなくVFP自体をtarget機能から外し、wide unaligned accessも
禁止する。既存のMMIO APIやNDISの所有・rundown処理は変更しない。
manifestにこれらflags、core/runtime recipe hash、使用kernel library名と
ntoskrnl/GS/libcntpr実hashを保存し、link mapも出力する。
host clang18/参照headersで旧flagsの生成assemblyを比較すると、ndis_runtimeは
44命令、native_contractは7命令、wmt_bridgeは420命令のVFP/NEONを生成した。
native_contractには `__rt_udiv64` 参照もある。これは保持runner objの逆アセンブルや
使用clang版一致を意味しないが、旧flagsで再linkするだけでは不十分という具体的根拠。
新flagsで全40 TUをcompileし、生成assemblyでVFP/NEON命令0を確認した。
参照headersによるhost compile/生成assembly検査は、WDK archiveを含む最終SYSの
link・実行検証とは別。helper名だけで最終SYSにFP命令がないとは主張しない。

### 保持済みobjから一回だけlink原因を切り分ける手順

主担当の既存runnerで実施する場合のみ。以下はb6b715fのobjを読んで別名の診断用SYSと
mapを作る最小再linkで、source provenanceを新commitへ付け替えたり、署名packageを
生成したりしない。既存objのcodegen flagsは旧設定のままなので、これだけ通っても
新scalar候補の完成ではない。objや親SYSを消さない。

```powershell
$old='C:\mt6630-native1003-b6b715f\src\build\native1003'
$km="${env:ProgramFiles(x86)}\Windows Kits\8.1\Lib\winv6.3\km\arm"
$names=@('ndis_runtime','native_control','native_contract','native_security',
 'station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl',
 'native_receive','native_hif_rx')
$objects=@($names | ForEach-Object {Join-Path $old "$_.obj"})
$objects+=Get-ChildItem (Join-Path $old 'a04br3-mt6630-core\*.obj') |
 Where-Object {$_.BaseName -ne 'native_tx'} | ForEach-Object {$_.FullName}
foreach($obj in $objects){if(!(Test-Path -LiteralPath $obj)){throw "Missing obj: $obj"}}
if(Test-Path "$old\runtime-link-only.sys"){throw 'Use a new diagnostic output name'}
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib `
 /opt:ref @objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" `
 "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" `
 "/map:$old\runtime-link-only.map" "/out:$old\runtime-link-only.sys"
if($LASTEXITCODE){throw 'WDK ARM runtime relink failed'}
```

正式候補は修正commitのclean checkout、新規output dirで前掲build helperを実行する。
scalar設定は全WiFi objへ影響するためNative/coreだけ再compileする。既存の検証済み
shared parentは `-SharedParentDirectory` で再利用し、GPU/親/SDKを再buildしない。
INFは1003を維持し、build/sign manifestのsource/flags/hashでb6b715f未link候補と区別する。

## 後続の一次資料復元とINF1004

署名済み1bd250c/1003を保持したまま、[NATIVE-1004.md](NATIVE-1004.md)に
到達不能閾値の実装、24/中間値の正確な扱い、新たに確定した未接続PSPの
RF停止要件と残件を記録した。上の「仕様未固定」は1003作成時点の履歴である。
