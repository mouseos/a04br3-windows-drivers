# GE8300の同じFW上でのClear・Copy

run147の遅延ログは初回Clear→GPU Copy→Mapの480000画素一致、Unmap成功、RemovedReason=0を記録し、2回目ClearのRenderCbに戻りがない。旧341のClearは毎回stop→retire→cold buildへ入る。Unmapを原因とする再試験は行わない。

342はCopy用のcold配置時に、RTへ書くTQ fillのEOT PDSをslot0へ追加する。Copy EOT64、fragment192、secondary256/272、shared512と重複しない。native22 clear_imageの52word payloadとPDSを使い、実resident RTのPTEに対応する出力先、既wide producerのpitch・tile・clipへ結び付ける。色はCCB task[2]に渡し、稼働中のUSC/PDS/PTEやRT画素をCPUで書き換えない。

初回の従来3D Clearは保持する。完了したCopyに続くClearは同じTQ contextを使い、既UFO/SLC/GP、MDLのcache所有権返却、新IRQ、scheduler tokenの経路を通る。WDDMのjob kindはClearのまま、内部job_is_copyは共通TQ経路を選ぶ。途中失敗はowner FAILEDで保持し、成功を捏造してcold経路へ逃がさない。

Copy/Clear/Copy/Clearの実factory.prepareとtransfer・CCB producerをASan/UBSan下で実行した。4 transaction完走、Clearでstop/retire/rebuild/live-code-writeを呼ばないことを確認した。既64Copy連続回帰も維持。GPU/IRQ/cacheはmockであり実機動作成功ではない。

83f2652eの同一immutable Draw再利用も含む。ただし最初のDrawや異なるshaderへの遷移は既cold経路が必要で、342が全reset原因を解消するとは未確認。通常desktop Present、任意shader/texture、長期安定、電源遷移は未完了。

増分ビルドはtools/ci/build-ge8300-tq-clear342.pyを使う。署名済み341のmanifest・全package・KMD13/native67 objectを照合し、native7 TUとidentity miniport1 TUだけcompileする。UMD/PCO/FW/clientと無変更objectはhash固定。SDK再setupは不要。ビルド成功と実機成功は別に記録する。
