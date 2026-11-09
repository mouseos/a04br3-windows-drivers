# run97: fresh96 / single GPU3.14 / SOFTWARE非書込みstage

run95はcompare index5で更新前停止。保存bytesではSOFTWARE.LOG1だけ565248→196608へ縮小し、
残存196608byteは元prefixと完全一致。他25file集合の全hashは同一。stage20.binも元fresh94と一致。
変化させた主体は未特定。EXIT1は旧comparatorではopen/read/byte不一致を区別しないため、
実回収bytesを根拠に差分を限定する。後半trust処理は未実行であり今回の原因と断定しない。

run95にreg load SOFTWARE→同Root/TrustedPublisher再登録→unloadが残っていた。
「newservice7値only」という旧HANDOFF説明は誤りで、原記録保持の訂正を追記した。
run97はこの未使用trust block全体とcleanupのunloadを除去する。SOFTWARE/LOG1/LOG2は書かず、
インストール前の既存WinVerifyTrust/CAT所属検証を成功条件として維持する。trust不在なら失敗停止し、
証明書追加で成功扱いにしない。

比較対象をSYSTEM trio（native置換の正本）、DRIVERS trio（既存driver database）、
全捕捉固定driver SYS、既存EngineのEXE/attempted/service.logへ限定する。
SOFTWARE trioは一切変更しないので、そのlog末尾縮小だけでGPU更新を止める比較から外す。
削除根拠は固定fresh94→96の実bytesであり、SYSTEM log/driver/Engine比較を弱めない。

fresh96 manifestf0fab046と元94manifest4ff327d3の実捕捉files全てを再照合する。
新しいnative loadは行わない。元native candidate6e8b1fa6を、SYSTEM/LOG1/LOG2全byte同一、
新service7値、同Engine ledger保持policy集合を根拠に再利用する。
元native result/観測receiptは変更しない。派生bindingsには元receipt hashと「新load無し」を明示し、
新source manifestはfresh96へ結合。native再利用proofは元94→96、既知SOFTWARE.LOG1 prefix縮小だけに限定。
他の差分やcapture集合変更があれば再利用を拒否する。

profile render-single-gpu314-run97-v1、service MT8167RenderSingleGpu。
render3.13→3.14だけPnP更新、同SYS/FW、新UMD、GPUclient1回、planned reboot最大1。
親/Audio1026、MSDC1.8、USB1005、WiFi1012、DOD1.6、Engine/boot/GPTを保持する。
quiet WinPEとeMMC命名候補は混入しない。実機操作はmainのみ。

専用stageのjournalはPRECHECK→CANDIDATE_WRITE_STARTED→COMMITTED。
比較/既存専用service directory拒否はBASELINE_REJECTED_NO_CHANGE（候補書込み0）、
候補書込み開始後の失敗はCANDIDATE_WRITE_FAILEDと表示する。NO_CHANGEは候補を変更していない意味で、
journal directory作成やfilesystem mount自体の無書込みを主張しない。
旧DISM_STARTED/DISM_FAILED_RESTORE_FROM_HOST_IMAGEと未使用DISM関数は削除する。
このラベル修正でdriver/serviceを再buildしない。builder sourceと署名runtime sourceを別記し、
差分が専用media builder/このREADMEだけで、祖先関係とcompile policy一致を確認した場合だけ既成署名を使う。
