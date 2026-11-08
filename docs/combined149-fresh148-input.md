# run149 fresh148採用入力

`tools/recovery/prepare-combined149-input.py --workspace ROOT --out NEW_ROOT_LOCAL_DIR`
で最新原41/native actual14instances/ログ65を照合し、採用入力を生成する。
全入力はroot内に解決し、fresh148 SHA/Recovery boot IDを固定する。旧146・予定147をactualとして扱わない。
親1116署名全member、HPv2/build1107、MSDC110全memberとproducer/main sealを照合する。
leaf1.10はsource74080073のみ固定し、main署名完了前に旧1.9や未署名SYSで代替しない。

これは媒体・発火policyを生成するコマンドではない。旧146 builderへそのまま渡す形式でもない。
媒体生成には以下の独立した修正・sealが必要であり、入力確認をもって媒体完成としない。

- 更新対象は親/thermal/MSDC。audio1034、WiFi1023、BT2013、GPU341ほかactual SYSを保持する。
- 旧serviceは11対象でindex10をrenderとして扱い、診断時にGPUエラー集約から除外している。
  MSDCを追加する場合、単にそのindexへ差し替えるとstorageエラーを失う。12件化なら配列・初期化・snapshot/readerも一組で変更する。
- `A04_FEATURE144`はbaseline WiFiをmt6630-runtime.sysへ戻す分岐とBT初回installを束ねている。
  新基点ではmt6630-combo-runtime.sysを照合し、存在済みROOT\\SYSTEM\\0001へ初回installを行わない。
- 旧146 policyはGPUclient1、audio/render DIF、旧61logs、fresh原39を必須とする。
  新149はGPUclient0、65logs、原41。旧validatorを単に番号置換して使用しない。
- native old_services18件に加え、receipt.new_serviceの旧CombinedFollowup146停止状態を保持する。
  new_serviceはvaluesのみでroot/subtree hashがない。旧18件のhashを流用して同一扱いしない。
- thermal測定IOCTL0x226810/28Bと状態IOCTL0x226814/64Bは別契約。後者で再測定しない。
- mainだけが同Recovery無変更資格を再確認し、最終署名tuple/service/mediaを独立sealして適用する。

実機/UART/媒体操作、runner操作、再buildはこの入力生成では行わない。
