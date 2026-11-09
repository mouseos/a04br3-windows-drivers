# GE8300の選択ドライバ更新

オフラインDISMでpackageを追加しても、既存デバイスの選択INF・版が更新されるとは限らない。実機では共有親SYSが1014のbytesに置換された一方、PnPの選択版は1010のまま、表示は1.5、GPUは旧2.5のままだった。ファイルと選択情報を別々に検査する。

既定OFFの`render-bound-update-v1`は固定3デバイスと標準DXGI clientだけを扱う。新しい`MT8167RenderStage`サービスを使い、以前のRenderProbe記録を残す。WinPEは比較済みpackageとnativeで準備したSYSTEMを配置する。通常OSの初回起動で、全packageの署名・CAT membership・実測した旧選択版/日付/class/service/SYSを確認してから、固定INFだけをSetupAPIへ渡す。

`DI_DONOTCALLCONFIGMG | DI_NEEDREBOOT`を保持し、稼働中の親やGPUを再起動しない。互換driver listと`DIF_SELECTBESTCOMPATDRV`でGPUのSystem→Display変更を処理し、`DIF_INSTALLDEVICE`で選択情報・ファイル・サービスを更新する。一回の計画再起動後にだけ、選択版/class/serviceとSYS/UMD/FWを再検査して標準DXGIのclear/flat/COLOR readbackを実行する。未対応操作を成功扱いせず、WARP/first-adapterへのfallbackはしない。

policy入力はroot内の固定backup/hash、nativeで読んだ3デバイスのbinding receipt、署名package/client/comparator、対応boot manifest。端末固有の校正値を埋め込まない。GPUの全FL9_1機能・一般アプリ対応が完成したというprofileではない。

再現入口は`render_bound_policy.py`、`build-ge8300-render-stage.ps1`、`prepare-offline-system-service.ps1 -ServiceName MT8167RenderStage`、`build-private-render-bound-media.py`。Windows 8.1 ARM WDK、正規OS媒体、別途供給する署名済packageを使う。生ログ・SYSTEM・媒体はsource repoへ入れない。

一次資料:
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiselectbestcompatdrv
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiinstalldevice
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w
