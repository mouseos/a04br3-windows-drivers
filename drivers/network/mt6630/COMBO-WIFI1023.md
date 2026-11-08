# MT6630 WiFi1023＋BTclient2013 統合

WiFi1022 source0ea7a7a9を基点にCarson f27cbdd7のtransport-capability分離を統合する。1022はWLAN-onlyでbroker_abi/combo_broker自体が未収録のため、runtime-only patchが必要とする既存broker/単一worker/親epoch参照/BT NVRAM読取り/transportとshared HFP header宣言だけを依存として取り込む。親driver/HFP/audio全treeは取り込まない。共通祖先8fd9b62dからprovider接続7fileを3way合流し、最新WiFiのdata/QoS/安全境界/RXretry/assoccapsをbyte保持。旧1019全treeへ戻さない。

標準NDIS providerはmt6630-combo-runtime 2026.11.8.1023/checkpoint1023の一つ。WLAN-only1022 INFは歴史保存し、同じMT8167\WIFIに二重installしない。BTclient2013はCarson署名済みc74992e4をbyte保持しrebuild/resignしない。HCIBypass routeはWMT/STP/Merge/FC72/HCIidentity/live parent epochの実proofから返し、Audio Bind/latency/HFP peerを依存にしない。SCO/HFP実音声資格は緩和しない。same-workerを維持し第2のSDIO ownerなし。

Carson source/object/include cacheを照合し変更closure22TUだけcompile、21object再利用。最新WiFi private context/include変更があるため旧brokerの2TUだけでは足りない。BTclient・親・音声・GPU・SDK/WDK/PCOの再buildなし。host ASan/UBSanは既存production route predicate/RXretry/association encoderだけ。RF/HCI/association/通信/性能・音声は未実機確認、143/338freezeと実機非接触。root成果windows-rt-combo-wifi1023のHANDOFF/pinsをmainが後段batchへ独立sealして使う。
