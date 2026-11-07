# MediaTek MT8167 周辺機器の一括延期更新

これは明示EnginePcm専用のsource候補。旧tone、通常Engine単発診断、GPU専用serviceは既定のまま。
run29のGPU更新で実機確認された `DI_ENUMSINGLEINF | DI_DONOTCALLCONFIGMG | DI_NEEDREBOOT`
による固定INF選択→`DIF_INSTALLDEVICE`→正規再起動を、次の5対象だけへ適用する。
この周辺機器版のWDK8.1リンク・署名・Windows実行・実機動作は未確認。

| 順序 | 固定hardware ID | package role | INF最低版（dateは全て11/06/2026） |
| --- | --- | --- | --- |
| 1 | `ACPI\BEN0001` | parent | 2026.11.7.1006 |
| 2 | `MT8167\AFE` | audio | 2026.11.7.1004 |
| 3 | `MT8167\USB20` | usb | 2026.11.6.1003 |
| 4 | `A04BR3\MT6630SDIO` | wifi | 2026.11.6.1003 |
| 5 | `ACPI\MOU0007` | display (brightness DOD) | 0.0.1.4 |

GPU provider MOU0006、eMMC、TOPRGUは対象外。既存touch/buttons/audio/USB/WiFiを保持する
単一Engine親profileを必須とし、子Startの一時変更、親live交換、remove/reenumerate、
`UpdateDriverForPlugAndPlayDevices`はこの明示モードでは呼ばない。
APIの文書契約: [SP_DEVINSTALL_PARAMS_W](https://learn.microsoft.com/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w)。

## 固定署名入力とビルド

新combined親/USBは `63cbd9f`（Carver `5290ebb`の統合済み変更）を含むsourceから
別途WDKでbuildする。新batchはEngine親INF1006とUSB INF1003を必須とする。旧親1005/993候補は保持するが選択しない。
既存Engine親manifestの全flagを復元し、USBは
`A04_MUSB_HOST_CORE_EXPERIMENT=1` / `A04_MUSB_HID_CONTROL_EXPERIMENT=1`を指定する。
正式standalone helperは実INFを出力へコピーし、実INFのDriverVerをmanifestへ取得する。
親/USB双方に非VFP/NEON codegen flagsとWDK `libcntpr.lib`を追加した
`cb8e662` / `8d0f8ec` を統合済み。音声2f SYSの再compileは不要。USBだけ新しくして旧993親と組み合わせた成功を推定しない。
WiFiはTesla `b6b715f88a3ebd2ffceda72f32ae0ec72422447b` / INF1003以降の新SYSを用意する。
旧d751c67/1002はこのモードでは拒否し、旧候補自体は変更しない。

作業ルート内のprivate JSONに下記schemaで実値を記録する。省略・placeholderを残した入力は使えない。
`packages`は表の5roleすべて、`clients`は下記固定2名すべてを必須とする。

```json
{
  "schema": 1,
  "backup_manifest_sha256": "<fresh captured-unmounted backup manifestの実SHA256>",
  "packages": {
    "parent": {
      "directory": "<root内の最終署名package directory>",
      "manifest_sha256": "<signed-manifest.jsonの実SHA256>",
      "source_commit": "<wrapper source_commitの完全40桁hex>",
      "certificate_thumbprint": "<このpackageの実cert SHA1>",
      "driver_ver": "11/06/2026,2026.11.7.1006"
    }
  },
  "clients": {
    "mt6630-wlan-probe.exe": {"path": "<旧固定署名WLAN clientのroot内実path>", "sha256": "<実SHA256>"},
    "mt8167-audio-engine-probe.exe": {"path": "<d0eb署名Engine clientのroot内実path>", "sha256": "<実SHA256>"}
  }
}
```

audio等も同じ5fieldを明示する。旧Engine bundleのようにwrapper `source_commit`が無い場合は
`inf_source_commit`をpinする。INF/SYS/CATはflat、role/、package/の固定layoutだけを受け付ける。
同じbasenameが複数layoutにあれば拒否。各packageのsource/hash/cert/DriverVerを別々に保持する。
certはpackage内のhash済み `mt8167-development.cer`。既知のBCBF/2655混在を同じcert扱いしない。

```powershell
# source固定checkout内の新規出力。署名鍵は所有者が明示する有効な開発鍵を使える。
.\tools\ci\build-mt8167-hardware-probe.ps1 -AudioMode EnginePcm `
  -DeferredPackageMap $FixedPackageMap -WorkspaceRoot $TaskRoot `
  -OutputDirectory $NewOutput -Sign -CertificateThumbprint $DevelopmentThumbprint
```

helperは固定5packageの実manifest/hash/cert/INFを読み、hash・version/date・client hash・
fresh backup hashをheaderへ生成してEXEへcompileする。policy JSON自体を実行時入力にはしない。
任意hardware ID/INF/target path/EXE/command/argsを受け付けるbrokerではない。
任意ディレクトリの探索・自動candidate選択・旧1002へのfallbackも行わない。

## private媒体と明示再arm

既存 `build-mt8167-shared-io-update-media.py -h` のEngine入力に
`--deferred-package-map <同じJSON>`を加える。既存 `--package` / `--engine-package` /
`--engine-client-package`は固定旧WLAN/d0eb/audio modeの検証用に引き続き指定する。
mapの2clientはこの固定bundleの実hashと一致が必要。
`--wifi-package` / `--parent-package` / `--brightness-package`とは併用しない。
optional TOPRGU入力は従来どおり独立に指定可能で、runtime延期更新対象には追加しない。

使用済みEngineなら既存の2段階を省略しない:

1. `--rearm-previous-media <旧manifest> --write-engine-rearm-plan <新plan>` で、
   同じfresh backupと修正package hash/新版へ結び付ける。新service manifest/certとmapも渡す。
2. 固定native helperでそのEngine planを適用し、clean candidateを回収。
3. 同じmapと `--engine-rearm-plan <plan> --engine-rearm-plan-sha256 <実hash>`、
   prepared SYSTEM/result/hashを明示してprivate媒体を生成する。

旧Engine marker/log/EXEは従来の `C:\a04backup\engine-rearm-<planhash>` へ保存する。
ディレクトリ全体を保存するため、過去のphase marker/reboot.logも失わない。
旧tone service/key/markerは保持。freshbackup gate、native7値/fingerprint限定監査、
private SYSTEMのcopy-before-DISM、WinPE SYSTEM RegLoad禁止も従来のまま。
private hive/backup/媒体/生ログを公開repoやCI artifactへ含めない。

## 2boot・各1回の動作

最初のbootはserviceを自分で無効化し、既存 `attempted.bin`をCREATE_NEWで作る。
全5targetの全INF/SYS/CAT hash・CAT信頼・INF/SYS CATmembership・一意のpresent node・
現在versionを先に確認する。高い既存版をdowngradeしない。
同版は選択dateと実installed SYS hashまで一致するときだけskipし、同版の別SYSは拒否する。
未install子の選択version欠落はinstall phaseだけ許可し、診断成功とは扱わない。

固定INFだけを列挙し、その唯一のdriver version/dateを照合して延期installを各1回行う。
前後のCM status/Problem、ProblemStatusのCR/type/bytes/実NTSTATUSをdiskへ記録する。
未知NTSTATUSを0に置換しない。全件成功した場合だけpolicy hash内容の
`deferred-pending.bin`をCREATE_NEWし、同じEngine serviceを次boot診断用に1回だけAUTOへ戻す。
部分更新はエラーを記録してservice無効を維持し、受理済み延期installのため再起動だけ行う。

同じ署名EXEの固定 `--reboot-after-stop` waiterがSCM停止/3010を確認し、
`reboot-attempted.bin`をCREATE_NEWして30秒猶予・forceなしの正規planned rebootを1回要求する。
SCM待ちは30秒上限。再起動要求の受理は実際の再起動成功の証明ではない。
要求失敗や上限超過は無効化して止まり、自動retry/rearmをしない。

install boot中はservice下のvolatile `DeferredInstallBoot` keyを残す。
診断bootはそのkeyが消えていることとpendingのpolicy一致を要求し、
`diagnostic-attempted.bin`をCREATE_NEWして自分を無効化する。
5target全ての選択version/dateと実installed SYS hashを照合してから、
固定署名WLAN/d0eb Engine clientだけを引数なしで各1回起動する。
CMエラーは最終失敗として保持しながらclient diskログを回収する。
package不一致ならclientを起動しない。loaded image hashの検証とは主張しない。
将来のfresh backupは既存 `verify-engine-installed.py` で媒体 `expected_installed_sys` と照合する。

install/診断/再起動waiterに `PowerRequestSystemRequired`を保持し、自動sleepを抑える。
ユーザーの明示Sleepやpower policyは変更しない。markerは一切削除せず、phaseは再試行しない。
service.logの既存CMイベントに加え、ID10..14へEVT100=phase、101=hash結果、102=CAT結果、
103=既版一致、104=install flags、105=install error、106=status、107=Problem、
108=NTSTATUS query CR、109=NTSTATUSを出す。reboot.logはEVT10Bへ要求/エラーを記録する。
UART通知は補助だけで、disklog/SCM/再起動後hashを個別に判断する。
