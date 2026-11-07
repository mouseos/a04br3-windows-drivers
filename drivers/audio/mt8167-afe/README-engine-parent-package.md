# EnginePcm親のDriverStore選択と再包装

Engine親の署名済みSYSを再compileせず、fixed-toneとは別DriverVerのINFで
packageを作る。実機・runner・service変更は主担当/Ramanの工程である。

## 回収物から確認できたこと

run27診断のSetupAPIはaudioをoem69.infへbindし、CM_PROB_FAILED_STARTを記録した。
System.evtxのaudio関連イベント3020–3022はservice登録/install成功までで、
開始失敗のNTSTATUSは不明。通常probeはendpoint0、80070490でPCM開始前に終了した。

回収済み`port/xnu/out/private-windows-rt-prebackup-run28`の実ファイルをhash計算した。

| 対象 | SHA-256 |
| --- | --- |
| 実配置parent | b64fe0af0ec1cb700159cf4c6b588eea4bd8c9591ad39bb031d2d37dbf92e201 |
| Engine署名bundleとrun27媒体のparent | 99338c1bbe30205b09af6c594a7c79cc4ef8a7c080a692330d36cdbb973f86ca |
| 実配置audioとEngine署名bundleのaudio | 2f56e9a4cb742c2d09284117d9e781493d827a2800a7925238bdc352b2b346ef |

旧parent hashはrun26r2 fixed-tone package manifestと一致する。
新旧parent INF hashは共にbce4eff2f119ee99ebde52045d343d510ca98011254f70dabb88b33278db269c、
DriverVerは11/06/2026,2026.11.6.1001で同じだった。
setupapi.offline.logの6635–6704行では新parentをoem65.infとしてimport/reflect/copyした。
setupapi.dev.logにはBEN0001の更新記録がない。

未replayのbackup SYSTEMをread-onlyで読むと、Select Current=1、
ControlSet001/Enum/ACPI/BEN0001/0のDriverはSystem classの0009、InfPath=oem50.inf、
DriverVersion=0.0.1.0、DriverDate=11-4-2026を参照する。
LOG replayをしておらず、このregistryやdisk hashをlive loaded imageの証明にはしない。
親の選択/配置に不整合があるため、新Engine親の実行結果としてCode 10を評価しない。
また、親不一致だけでCode 10の発生箇所やNTSTATUSは断定できない。

## SYS再compileなしの親再包装

このcheckout内の未使用出力directoryを指定する。

```text
python3 tools/audio/package-mt8167-engine-parent.py --signed-bundle <回収済みwindows-rt-engine-pcm-de214c8-e4dff2b> --output <このcheckout>/build/engine-parent-package
```

helperはsigned-manifest、parent SYS/INF/capability manifestのhashと、
de214c8 source/A04_SHARED_ENGINE_PCM=1を照合する。99338c1b...の署名済みSYSを
byte単位で保持し、新しいsource templateからshared-parent.infを作る。
Engine専用templateはprototype/shared-parent-engine-pcm.inf、
DriverVer=11/06/2026,2026.11.7.1005。runner現地11/06のpostdated拒否を避け、
既存packageよりversionを上げる。既定fixed-tone INFは変更しない。

出力はSYS、新INF、runtime-parent-manifest.json、package-manifest.json。
古いCATはコピーしない。raw runtime manifestは署名前のSYS hashを含む履歴として
そのまま保持し、現在のsigned SYS hashや新しいINF出自と混同しない。
新manifestはbinary source=de214c8、INF source=このcommit、各hashと
catalog-resign-requiredを記録する。これは署名完了manifestではない。

主担当はこの親packageだけInf2Cat /os:6_3_ARM /uselocaltimeとCAT署名・検証を行う。
SYS自体は再compile/再署名せず維持できる。audio SYS/INF、probe、USB/WiFiをこのために
再compileする必要はない。実配置時は既存devnodeの選択も別途更新する必要がある。
DriverVer bumpだけで既存のBEN0001が必ず選び直されるとは扱わない。

Ramanの固定一回service/主担当の標準PnP更新経路で、子の再生を始める前に親を更新し、
必要な再起動後に選択INF/version、親とaudioの実配置hash、CM状態を回収する。
既に資源をpinした子が動作する中での親の強制交換や、直接SYS上書きだけを正式更新としない。
旧packageの削除や任意実行brokerは不要。WiFiの旧SYS残留修正はTesla担当と分離する。

将来のEnginePcm新規buildも専用templateを選択し、親build manifestへそのDriverVerを記録する。
ReuseParent時は旧runtime manifestを改変せず、top-level manifestの
parent_package_driver_verに新しいpackage版を別記する。

## 次の最小観測

通常音声probe package 2026.11.7.3はendpoint不在でもMT8167/AFE devnodeを列挙し、
CM status/problem、DEVPKEY_Device_ProblemStatus、親instance/statusをread-onlyで記録する。
property取得成功・DEVPROP_TYPE_NTSTATUS・4 bytesの場合だけraw値を採用し、
未提供や型不一致は取得エラーとして残す。取得した値を過去run27の戻り値にはしない。
PCM送出・UART任意/disklog必須・既存tone非使用の契約は維持する。

ホストでの実ファイル再包装/hash照合とprobe ARM32構文/COFF生成は別の確認であり、
Inf2Cat/CAT署名、WDK link、親更新成功、Code 10解消、音声再生成功を意味しない。
