# run91 ERROR22: RT8.1 WaveRT buffer取得前のACQUIRE

run91保存capture.logはpnp problem0、固定filter1、KsCreatePin0の直後にend22。旧client source2d623172は最初にKSSTATE_ACQUIREを要求し、その後にKSPROPERTY_RTAUDIO_BUFFERを取得していた。失敗を無視したものでもbuffer取得済みでもない。

## 実artifactで確定した契約

既存インストール済みWindows partitionのホストバックアップから読み取ったportcls.sysと、同一公式install.wimのportcls.sysは全byte一致、SHA256 `f9d0f5ce9d6072785a9cf8d242c3f490e4b2af6455ff05fec6a21815bc3e9db6`。実機から新しい読出しは行っていない。

REAの独立stdio session（mainのdxg session不使用）で実RT8.1を解析した。

- RtAudio GUIDは0x1def4、WaveRT property setは0x2223c（8items）。Property1/5は0x1ee40/0x1ee68からThumb pointer0x26479へ結合。
- Connection GUID/property0は0x1ed78からGet/Set pointer0x27b29へ結合。
- State handler0x27b28は、要求STOPまたはpin+0x4c非0の場合だけ状態変更へ進む。それ以外は`0xc0000184 STATUS_INVALID_DEVICE_STATE`を返す。
- Buffer handler0x26478はminiport AllocateAudioBuffer/MDL map成功後にpin+0x4c=1を設定する。
- miniportの未buffer ACQUIREを成功にしても、PortClsの先行gateは通らない。観測ERROR22は最初のACQUIREで拒否される実装と一致する。

Ghidraの通常解析で二つのproperty handlerがprocedureとして未認識だったため、専用解析コピーの**PE header AddressOfEntryPointだけ**を各table由来pointerへ変更してREAへ入力した。元artifactは不変、code/data sectionは全byte保持。コピーは静的解析用だけで実行/配布しない。解析entryの関数名は意味を持つ元symbolではない。

REA Evidence: original table bytes `ev_bba0be306c7900f1a5f62b7d1df38034511815ea57ece1cff4f704a9292aa6ee`、buffer function `ev_1596fe90576d2abf4689892b791e60699ccbc725a75d6b6baff0ccbb3b519dbb`、state function `ev_a719b0eaa9285e094df847f2d4e9600b9efd32e82316d225723f4106a8137028`。生証拠は作業root内logsのwindows-rt-capture-error22-20261108に保存。

Microsoft一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-setstate
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/ksproperty-rtaudio-buffer
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-allocateaudiobuffer

## 修正と適用境界

KsCreatePin後のSTOP状態でbufferを取得してから、各passをACQUIRE→PAUSE→RUN→STOPとする。固定48k/stereo/S16、buffer16KiB、2pass、実CUR/IRQ条件、PCM集計、optional latency未資格判定は維持。state-setに要求state/error/返却byte数、bufferにerror/size/barrierを記録する。実PCMやuser pointerは記録しない。親・Audio・USB・WiFi・GPU・Storage・RTC/DDI/flagsは一切変更しない。

tests/audio_capture/probe_protocol_test.pyは旧/新clientの実run/state本文を抽出し、Windows native fixtureへ組み込む。REAで確定したPortCls buffer gateを独立に適用し、旧22/新2pass通過、buffer拒否後にstate要求しないこと、PAUSE拒否時にRUNしないことを検証する。合成IRQ/PCMはfixtureだけのもので実機結果とは区別する。

build-capture-client-only.ps1はsource2d623172の全driver source不変、署名済みcapture1026 package全filehashを確認し、KS client1TUだけをWDK8.1 ARMでcompile/signする。親/Audio SYS/CAT再buildや版上げは不要。返却signed-client-manifestはclient sourceとdriver sourceを明確に分ける。

次回実行前に、run91が作成した固定MT8167CaptureProbe/capture.logをfresh92の実hashに基づいて非破壊退避する必要がある。CREATE_NEWは保持し、旧logを黙って上書きしない。更新policyは最新fresh92の親/Audio1026/eMMC1.8/render3.13を保持し、client更新だけのためにdriverを再installしない。GPU新COLOR clientの独立pinも別管理する。実機の録音完了・IRQ・電源遷移は署名成功だけで成功扱いしない。
