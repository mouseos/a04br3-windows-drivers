# 通常Windowsの一回限りPnPインストール

`build-mt8167-device-install-media.py`は検証済みmaintenance WinPEを元に、搭載Windowsの標準PnP APIを実行するサービスを配置する媒体を生成する。生成と実機動作を区別する。`MT8167DeviceInstall.exe`は`tools/ci/build-mt8167-device-install.ps1`から生成し、対象OSが信頼する証明書で署名する。displayディレクトリはINF/SYS/CATと`mt8167-development.cer`を含める。

引数は`--workspace`、`--base`、`--display`、`--installer`、`--out`、`--tools`、完全な固定`--source-commit`。出力先は新規ディレクトリを指定する。base WIM/ESPは既存ハッシュで固定。manifestに全入力・コマンド・ESPのハッシュを記録する。

準備時はWindows領域を識別してSYSTEM/SOFTWAREとトランザクションログを保存し、信頼ストア、固定System32ディレクトリ、LocalSystem自動サービスを登録する。実測したControlSet001以外は拒否する。WinPEではデバイスを開始しない。hive unloadとvolume releaseの成功を確認してから通常ESPへ戻す。

サービスは通常OSで一回だけ動き、試行前に自身を無効化してattempted.binを永続化する。成功・失敗とも自動再試行・自動再起動をしない。結果はSystem32/MT8167DeviceInstall/install.logとUARTのDOD動作を照合する。DISM/staging成功だけをPnP開始・GPU描画成功とは扱わない。
