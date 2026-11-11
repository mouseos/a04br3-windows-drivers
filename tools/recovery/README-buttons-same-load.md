# MT8167 KEYPAD / MT6392 Buttonsの同load観測

`read-fresh200-native-inventory.ps1 -IncludeButtonsDeviceBindings`を追加。
callerが所有する同一private SYSTEM rootを使い、Load/Unload/privilege/hive clone機構を変更しない。
既optional reader、service snapshot、POST保持、manifest/trio検証は不変。

固定service6件 Mt6392Keys/mshidkmdf/kbdhid/kbdclass/hidserv/HidBthの
ImagePath/Start/Type/ErrorControl/Group/ObjectName/DependOnServiceだけを読む。
`Enum\MT6392\Buttons`の最大8instanceから固定値とclass root選択を読む。
実keypad1.2（source7bb166c7、selected SYS a3041d0f…）のHID attributesはVID/PID0であるため、
`Enum\HID`は名前だけ最大512件を列挙し、VID_0000&PID_0000/REV_0001/Col01..04の
固定familyだけ、各8/合計32instance以内で値を読む。別HID familyの値は読まない。
動的driver suffixはGUID+4digitだけを許可し、registry linkは共通Native helperが拒否する。
保護keyのエラーやbound超過を無視して成功にしない。

Source root valuesはraw型/bytes/hexと未展開stringを保持。key欠落はpresent=false。
HID family/ParentIdPrefix候補の一致だけで親子関係、STARTED、press、配送成功を断定しない。
WDF既runtime timestampは既receipt.keypad_wdf_runtimeを参照する。
動的SCM/ConfigurationManagerやdevice APIを呼ばず、wake/入力注入/driver設定変更もしない。

fresh227はTimingCaptureNumber227、suffixなし、NewServiceName MT8167CombinedFollowup227。
AdditionalServiceNamesへservice221/225を渡す。manifest実SHA/source commit固定後、
battery/WiFi/graphics/display bindings/buttonsを一度だけ読み、失敗時もexportを保持する。
以前の225 SYSTEMやpartial223を再loadしない。source変更による再build/signは不要。
