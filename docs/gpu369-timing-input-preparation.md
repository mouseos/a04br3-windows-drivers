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

後段は既200/202の生成器を共用し、369 manifest adapter／display更新対象／matched98deを
新入力設定へ渡す。媒体・private checkerも新profile識別へ結合する必要がある。
旧schemaを369として読み替えたり、368/旧boot保持assertを無効化して流用しない。
実counts、expected変数shadow回避、363/356 pin、canonical/runtime token分離を維持する。
この準備branchでは既200/202 profile・媒体CLIを変更していない。

main承認まで媒体生成・VM占有・端末操作なし。SDK/GPU/PCOの再buildもなし。
