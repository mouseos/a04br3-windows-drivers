# GPU369 timingの通常Windows用入力準備

このbranchは既203媒体／fresh202 profileを**構造templateとしてのみ**参照する。
GPU driverのbuild sourceはa04基準42d8、正式compile15c64bf3、署名recipe2ffc1930。
本branchのf566はprofile-toolingの基点でありdriver再buildに使用しない。

## 準備入口

`tools/recovery/prepare-gpu369-timing-inputs.py --prepare-only`は、実署名15成果物、
独立review、main package/boot review、実boot98de／FD／clean Mu出自をSHA結合する。
候補packageのmanifest viewを作り、既202生成器が共用するstrict package validatorで
render/displayの両候補行を検証する。原署名manifest／成果物は変更しない。
viewのsourceは包装出自であり、UMD/nativeの実compile出自a04は別fieldで保持する。

出力は`input-lock.json`、`candidate-package-view.json`、`profile-delta.json`、
`pending-map.json`のみ。実行policy／header／SCM／nativeSYSTEM／WIM／ESPは生成しない。
`pending-map.json`は媒体CLIへ渡せる最終mapではない。

## 既profileから必要な差分

- DIF対象はdisplay＋render（mask1056）、順序display→render。
  既serviceのDI_DONOTCALLCONFIGMG|DI_NEEDREBOOTを維持し、live devnode restartを追加しない。
  旧renderがtransport v1参照を保持しているため、providerだけのlive置換をしない。
- 両候補INF/SYS/CATは369。新renderはtransport v2必須。transport357を保持しない。
  UMD/FW/client/launcherのbyteと非対象WiFi/USB/audio等は保持する。
- 旧boot0943を新boot98deへ変更する計画が必要。DTM0 firmwareとの組合せを必須にし、
  matched bootを適用するまでcandidate Windows bootへ進めない。
  GPT変更なし。新bootの適用済み／物理read済みとは扱わない。
- launcher363/present356を1回、globaldiag198を同じ診断bootで1回。
  helperは再buildせず新serviceの絶対log prefixを使う。負tag23を成功としない。
- canonical tokenとgpu_client_count=1へ適合するruntime tokenを区別する。

## main承認後に必要な実入力と最小finalize箇所

現在のWiFi試験後の実fresh、SYSTEM trio、native読取りreceipt、supplementのUMD/FW/log、
実selected tupleに対応する全package行、capture contract、旧log baseline、boot witness、
新serviceの実不在が必要。旧202の45services/191named等を現在値へ流用しない。
18services/125valuesも固定しない。旧taskを再armせず、markerを削除しない。
新service名・capture番号・実件数は未確定のまま入力に残す。

後段も既200生成器を共用する。369だけcandidate/transport/boot hookを追加し、
旧200/202の既定は変更しない。capture204..999の実contractから番号とserviceを導出する。
新しいcapture番号ごとのsourceファイル追加は不要。
SCM1TU、native7値のみ準備、media validatorも同じ実policyへ結合する入口を用意した。
VM/実機での新Windows実行確認は行っていない。

## 実入力取得後のコマンド

`finalize-map.pending.json`を新しいroot内mapへコピーし、実fresh/native/capture-contract/
supplement/mutable-baseline/actual-package-rows/boot-witnessのpathとSHAを入力する。
capture_numberとprofileは実capture-contractから導出でき、nullのままでよい。
boot witnessは現在の旧0943の保持証拠であり、新98deを適用済と偽らない。

```sh
python3 -B tools/recovery/prepare-gpu369-timing-inputs.py --finalize --workspace "$ROOT" --inputs "$ACTUAL_MAP" --out "$FRESH_POLICY_OUT"
```

native inventoryは既入口に`-TimingCaptureNumber $Capture`を追加する。
旧templateから名前だけを供給し、新service不在、全root fingerprint、全named実値を取得する。
sealは既入口`seal-gpu368-capture200.py --gpu369-timing --capture-number $Capture ...`を使用する。

承認後の既SCM1TU入口は`build-combined200-service.ps1 -Gpu369Timing -CaptureNumber $Capture ...`。
既native準備入口は`prepare-offline-system-service.ps1 -ServiceName MT8167CombinedFollowup$Capture ...`。
媒体CLIは`build-private-peripheral-batch-media.py --gpu369-timing-profile ...`。
媒体入口だけはmapのmain_execution_approved=trueも必要で、今回それを設定・実行しない。

WiFi1029を実freshが選択している場合だけ、mapへ
`retained_package_overrides.wifi={path,sha256}`を指定できる。
最終c119/source・d974/manifestの1029 rowだけを受け付ける。
その後、実配置SYS・実selected version/dateとの通常の照合を必須にする。
1027選択に1029 rowを指定した場合は失敗し、無視も自動fallbackもしない。
install mask1056にはWiFiを加えず、named値とrawSYSは新freshを保持基点にする。

main承認まで媒体生成・VM占有・端末操作なし。SDK/GPU/PCOの再buildもなし。

## USB後段interface

現候補のphase0/mask1056/実fresh fingerprintはUSB先行処理で変更しない。
既run_clientsの後にはpreserved()があるため、USB操作を単純に第3clientへ入れない。
Singerのactualfresh/GPU369へ再結合されたcomponentは、GPU診断＋最後の保持検査後の
有限phase1 tailとして次段で結合する。必要入力は署名EXE/manifestのpathとSHA、
固定argv、timeout、許可変更集合、独立result/receipt path。未取得値を合成しない。
このsource差分にUSB実行・新diagnostic clientは追加していない。
