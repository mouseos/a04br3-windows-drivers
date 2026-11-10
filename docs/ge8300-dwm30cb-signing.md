# source30cbのDWM専用署名候補

対象driver sourceは`30cb20f79ec3c5f71c056d5783fe6743e92bf2ce`。署名recipeのcommitとcompile元は分離して記録する。GPU369 timing consumerは含まれず、既GPU369 profileへ適用できる候補ではない。実機・DWM成功は未確認。

`tools/ci/sign-ge8300-dwm30cb.py`は実七成果物のsource-build-manifest、native/KMD/UMD/link各manifestの実hash、固定clean compile treeを検証する。GPU368署名原本の15file/SHAをbaselineとして、実source30cbからのrender/transport SYS・UMD DLL・新import libraryを置換する。既FW・client・証明書byteを保持。3PEのBCBF署名で非署名payload不変を検証し、INFを実ホスト日付/0.0.3.70へ変更してARM8.1 CATを作成・署名する。5署名と6catalog membershipを実検証する。既CurrentUser秘密鍵の読取りだけを使用し、鍵・証明書のimportやmachine設定、PnP/実機操作は行わない。

入口：`python -B tools/ci/sign-ge8300-dwm30cb.py --enable-candidate --baseline-package <実GPU368 package368> --linked-directory <実source30cb build/dwm-source> --build-manifest-sha256 <実source-build-manifest SHA> --output <この署名checkout内の新build出力> --sign-tool <既signtool> --sign-tool-sha256 <実SHA> --inf2cat <既Inf2Cat> --inf2cat-sha256 <実SHA> --powershell-sha256 <実PowerShell SHA>`。

署名処理はcompile/linkを行わない。失敗時もreceipt/log/ZIPを保持する。先行するstrict-parent＋adapter差分buildの成功を推定せず、実manifest到着後にだけ実行する。
