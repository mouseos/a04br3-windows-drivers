# MT6630 association能力の構成（1021上の後段1022）

station_runtimeがAP beaconのCapability Infoをそのままassociation requestへ渡していた。stock Linux gen3 mgmt/assoc.c assocBuildCapabilityInfo:156はESSから端末能力を構成し、privacyと実PHY/preamble/short-slot契約だけを設定する。APが示すQoS/APSD/spectrum/block-ack/PBCC等をstation能力としてechoしない。

本subsetはERP20、station commandのQoS/HT/VHT/AMPDUが無効。既存BSS RLM byte82がAP short-slot bit10で生成される契約に合わせ、associationもESS＋選択privacy＋同じshort-slotだけを送る。short-preambleは有効化producerが未実装なので広告しない。AP観測capability自体は変更せず、RSNA/RSN IE/OS暗号選択・peer MAC・auth/channel grant/STA event成功判定は保持。privacy矛盾、IBSS、非ESSは拒否する。

変更は新pure helperとstation_runtimeの実association呼出しだけ。チェックポイント/INF1022。Windows driver専用sourceで、vendor_xnuや本機XNUコード、親共有電源、音声1033、143採用MEDIA修正、GPU、媒体へ変更しない。1021再送ownerを保持する。host ASan/UBSanは実association encoderとHIF submissionへ渡されるcapability/RSNを確認し、AP成功や実通信を合成しない。

新hostで既署名1021のobject/source/include/hashを照合し、依存2TU（checkpoint/station_runtime）のみ再compile、39object再利用。別版を無根拠に量産するためでなく、端末が実装しない能力をAPへ広告する具体的欠陥の修正。実AP association/暗号通信はmainが既存profile条件で後段確認する。物理AP/SSID/鍵/ユーザー操作は要求しない。
