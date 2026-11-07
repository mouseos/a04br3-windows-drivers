# run49：NUL-country recordの動的decode修正1008

## hostで確定した原因

実配置1007一致のrun49はNVRAM read/checksumまでNTSTATUS0、規制decodeで-1。
主担当がRecoveryからread-only回収したprivate 5MiBコンテナを、同じportable NVRAM/
calibration/regulatoryコードへ入力し、旧decoderのASCII国コード必須チェックでの拒否を
host上で再現した。recordはNUL-country、map0/index0。版peerは両0でsource互換条件を満たす。
個体512B/MAC/TX power/校正値/入力hashは公開source/配布imageへ含めない。

修正後は同じprivate入力でparser/decode/校正command encoding検証がすべて成功。
これはhost codecの検証で、firmware送信/NDIS開始/WLAN scan成功ではない。
FW read/bootやSystemRoot経路を再試験・再変更しない。1007 SYS/INFは保持する。

## 一次sourceとの対応

CT3 vendor `7c4782127d4ea11965be00ce1343056be9bfb769`：

- `include/CFG_Wifi_File.h`：512B、part1 own/peer=0/2、part2=256/258、TX valid196、
  regulatory map202/index203/subband204。実headerをhostでcompileしoffsetを静的照合した。
- `include/config.h`/`os/version.h`：driver own0x0200/peer0、SW version check有効。
  `wlanLoadManufactureData`はrecord peer1/2がdriver ownより大きければ拒否する。
  ownの未知の完全一致を要求せず、宣言されたpeer互換条件を使う。
- `mgmt/rlm_domain.c:rlmDomainGetDomainInfo`：map1の有効index、map2のcustom、それ以外は
  country検索。未一致（NULを含む）はREG_DOMAIN_DEF_IDX21。元countryはそのままcommandへ。
  countryをJP/DF/別個体値へ置換しない。driverはsource tableの2.4GHz ERP20だけを採用し、
  channel14/5GHzを広告しない。reserved map enum、壊れたgeometry、ERP channel無しは拒否。
  count0のsubbandはsource通りdisabledで、全体失敗にはしない。
- `os/linux/gl_init.c`はrecordのcountry2byteをそのまま読み込む。ASCII必須条件は無い。
- 既存XNU `A04BR3WLANRuntime.h`もNUL-country/default domainを明示している。
  Windows側の拒否はrecord不正やFW boot失敗ではない。
- `wlanLoadManufactureData`のown-version1分岐は0x24を送らない。他の互換recordは
  動的TX powerを送る。2G AC設定は実headerでdata144..154/valid155、CMD_TX_AC_PWR_Tは
  band1B+全11B=12B。旧valid154/payload11Bを修正し、存在しないopcodeを追加しない。

## Native802.11 country契約

本driverはIEEE802.11d multi-domainを実装していない。
Microsoft `DOT11_EXTSTA_ATTRIBUTES`仕様通りuNumSupportedCountryOrRegionStrings=0/pointerNULL。
SUPPORTED_COUNTRY_OR_REGION_STRING OIDは空list、DESIREDの初期/RESET値は3byte全0。
SETはその値だけを受け入れ、未実装のdomain変更を成功扱いしない。
NVRAMのNUL-countryを不正なISO country stringとしてNDISへ広告しない。
このOS MIBと、動的NVRAMからFWへ渡す元country/regulatory domainは別の契約。

一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-dot11_extsta_attributes
- https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-supported-country-or-region-string
- https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-desired-country-or-region-string

## bounded checkpoint

schema1/1624B/64event/10flushを保持し、個体byte列は記録しない。
Stage69 CAL_RECORD_VERSIONSはOBSERVATION、raw=part1 own|part2 own<<16、
detail=part1 peer|part2 peer<<16。これは版でありNTSTATUSや成功flagではない。
Stage66 CAL_REGULATORY_DECODEは実backend戻り値、detail:

|bits|内容|
|---|---|
|0..7|reason:0 OK/1 argument/2 version/3 TX valid/4 MAC validity/5 map/6 band/7 geometry/8 ERP無し|
|8..11|選択:1 country table/2 index table/3 custom/4 source default table21|
|12..15|countryの分類:1 NUL pair/2 uppercase pair/3 その他。実2byteは保存しない|
|16..23|recordのmap enum|
|24..31|recordのtable index|

イベント増加を抑えるため、runtime boundary Stage67は失敗時だけ記録する。
成功stageを0で補完せず、未観測として扱う。成功時のheader1event追加と従来runtime
success1eventを入れ替え、Native attrs失敗後cleanup/returnまで最大64eventを維持する。
read側の具体NTSTATUS、decode reason、bridge runtime/submit失敗は区別を保持。

## private host再現と署名候補

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror \
  tools/diagnostics/mt6630-nvram-inspect.c \
  drivers/network/mt6630/calibration.c drivers/network/mt6630/regulatory.c \
  -o build/mt6630-nvram-inspect
build/mt6630-nvram-inspect PRIVATE_NVRAM_IMAGE
```

入力はrb open、partition境界/長さ/checksumを同じparserで検証する。
出力はresult/版/分類/map/encoding検証件数のみ。入力/record書出し機能は無い。
実recordをfixture化しない。合成fixtureではNUL/default、table21、peer互換拒否、
disabled/invalid subband、AC valid offset/全11B、legacy own1の条件を確認する。

固定sourceを新runner専用dirへ取得し、`build-mt6630-runtime.ps1`、
`package-mt6630-runtime-1008.ps1`でbuild/link/SYS署名→CAT再生成署名→回収hash照合。
親は実署名hash/clean出自/3critical flags/iface blob一致を確認して再利用し、再buildしない。
INF/checkpoint buildは1008。次の主担当一括試験で配置hash/selected版/世代を照合し、
NDIS attrsとWLAN interface/scanの実結果を確認する。単独flash試験は行わない。
