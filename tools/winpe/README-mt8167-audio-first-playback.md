# WinPEからの初回KS再生とUART観測

08faae5の共有parent・Audio SYSのWDK 8.1 compile/link成功を基点とする。
この追加は、音声サービスがないWinPEから実際にKS pinを開き、既存の固定PCM再生を
一度だけ開始するclientと、読み取り専用snapshot、UART転送を提供する。
可聴成功は常に未確認。camera・マイク・USB音声による確認を必要条件にせず、
DMA進行・codec状態・停止をそれぞれ観測する。

## 主担当向けのビルドと組込み

同じcommitで親・Audio・clientを一括ビルドする。

```powershell
./tools/ci/build-mt8167-first-playback.ps1
```

今回、共有音声interfaceはv11へ更新した。08faae5のv10 parentに新Audio SYSだけを
組み合わせない。署名、媒体、ACPI対応、競合する旧所有者の停止は主担当が行う。
こちらのスクリプトはrunnerの開始・終了、実機操作、drvloadを行わない。

追加成果物は `build/mt8167-first-playback/client/` の以下のファイル。

- `mt8167-audio-first-playback.exe`
- `run-mt8167-audio-first-playback.cmd`
- `client-manifest.json`

親子SYS・INFとEXE/CMDのSHA-256は一括ビルド側のmanifestにも残す。
clientだけのcompile確認には `tools/ci/build-mt8167-audio-ks-client.ps1` を使える。
既存runnerでの今回の変更のcompile/linkは主担当から結果を受け取る。

実行は、対応する署名済みparent・Audio・TOPRGUのPnP開始後にCMDを一度呼び出す。
通常のAudioSrv、MMDevice、WASAPI、Speaker endpoint、topology列挙を経由しない。
SetupAPIでKSCATEGORY_AUDIOを列挙し、instance IDが `MT8167\AFE\` で始まる
対象一つだけを開く。他の音声デバイスは開かない。

EXEの通常importsはkernel32.dll、setupapi.dll、cfgmgr32.dll。
`ksuser.dll` の `KsCreatePin` は動的に解決する。対象WinPEにない場合は、
`ksuser-error` をUART・ログへ記録し、RUNせず終了する。主担当が対象と同じARM32
Windows媒体の正規コンポーネントを組み込む。存在しないDLLやサービスを補うために
別アーキテクチャのファイルを使用しない。kernelのKS/PortCls・WaveRT対応も必要。

## clientから実再生まで

1. EXEと同じ場所に `audio-first-playback-<runid>.log` を新規作成する。
2. TOPRGUの既存 `\\.\A04BR3ScreenUart` を開く。UART経路が利用できない場合はRUNしない。
3. 対象filterとPnP状態を取得し、新snapshot ABIのsize/version、未開始状態、
   parent lease・電源参照を確認する。
4. pin 0のinterface/mediumをKS propertyで問い合わせ、広告された標準interfaceを選ぶ。
   LOOPED_STREAMINGがあれば優先する。48 kHz・stereo・S16_LEでKsCreatePinする。
5. ACQUIRE→KSPROPERTY_RTAUDIO_BUFFER（8192 bytes）→PAUSE→RUNを一度要求する。
   利用者側bufferはdriverのDMA用固定PCMとは別。clientはどちらも書き換えない。
6. RUN後に8回、実snapshotを取得する。短い待機とUART出力を挟むため、間隔は一定ではない。
   記録したGetTickCountとCURを使う。CUR差から総経過frame数を推測しない。
7. PAUSE→STOPと停止後snapshotを回収する。pin/filterを閉じてもdrainや資源解放を意味しない。
   kernelは既存設計どおり、露出したDMA・clock・codec・mapping・電源参照を保持する。

RUNの再送・自動再起動・復旧・追加書込みはない。途中失敗もerror、failure_stage、
有効なsnapshotを記録する。filterがない場合はCMのdevnode status/problemを回収する。
この場合、開始失敗したAudio内部snapshotを取得できたとは扱わない。

## snapshotとUART

Audio filterの `A04_AUDIO_READ_STATUS` は固定長METHOD_BUFFEREDの読み取り専用IOCTL。
通常のKS IOCTLは従来どおりPortClsへ渡す。PnP revokeはsnapshotのrundownを待って
owner参照を破棄する。callbackや任意レジスタのアドレスをuserlandへ渡さない。

AFE/codec/PMICの観測は共有parentロック内で行う。valid bitが立っていない組の値は
不明であり、ゼロや成功として扱わない。複数レジスタの原子的な瞬間snapshotではない。
IRQ通知数はソフトウェアの累積通知数であり、ハードウェアの経過period数ではない。

UART行の形式は次のとおり。全値は32-bit hexadecimal、各行96 bytes以下。

```text
A04AUDKS r=12345678 sample-cur a=00100040 b=00000040 c=00000002
```

`r` はclient開始時のGetTickCount。同じIDが後続bootで再利用される可能性があるので、
回収側はbeginごとに別実行として扱う。snapshotのphaseは0=RUN前、1=RUN中初回、
2=停止直前、3=停止後。主要レコードのa/b/cは以下。

| tag | a | b | c |
| --- | --- | --- | --- |
| pnp / pnp-no-filter | CONFIGRET | devnode status | problem code |
| run / acquire / pause-before / pause-after / stop | Win32 error（0=受付成功） | tick等 | 予約 |
| snapshot | phase | snapshot sequence | tick |
| state | 現在stage | kernel NTSTATUS | 最初のfailure stage |
| transport | transport state | IRQ active | DMA exposed |
| valid | Audio valid mask | codec valid mask | Audio online |
| dma | DMA論理BASE | inclusive END | bytes |
| cur | DL1 CUR | 検証済み位置（valid時のみ） | DMA exposed |
| irq | DPC通知数 | fault通知数 | IRQ_STATUS |
| afe | DAC_CON0 | AUDIO_TOP_CON0 | IRQ active |
| lease | parent online | leased | D0電源参照保持 |
| flags | shared quarantine | digital retained | parent poisoned |
| codec | digital prepared | Voice active | hardware trim取得済み |
| analog0 | APMIXED 0x700 | 0x704 | 0x708 |
| analog1 | APMIXED 0x70c | 0x710 | TOP audio clock status |
| speaker | SPK_CON0 | SPK_CON9 | SPK_CON11 |
| trim | SPK_CON1 raw | trim code | SPK_CON6 raw（bit14=Class D OC） |
| rails | VAUD22 raw | VAUD28 raw | TOP_CKPDN1 raw |
| sample | sample index | tick | Audio valid mask |
| sample-cur | DL1 CUR | 位置bytes | DPC通知数 |
| sample-codec | SPK_CON9 | SPK_CON6 | fault通知数 |
| sample-valid | codec valid mask | poisoned | failure stage |
| observed | 有効CUR標本数 | 隣接位置の変化数 | codec状態確認数 |
| end | client終了code | UART queue失敗あり | 常に0＝可聴未確認 |

Audio valid mask: parent=1、AFE=2、position=4、DMA descriptor=8。
codec valid mask: parent=1、clock=2、analog=4、speaker=8、rails=16、trim/OC/PMIC clock=32、AFE=64。
stage: created=1、digital=2、voice=3、DMA=4、unmute=5、running=6、stop=7、stopped/pinned=8、revoked=9。

終了code 0は、RUN/STOPの受付、範囲内で変化したCUR、codec enable/mute解除・電源状態、
faultなし、停止後muteを観測したことを示す。可聴成功ではない。
code 2は重複client起動、3はUART経路、4はksuser、5はfilter発見、6はsnapshot ABI、7は未開始条件、
8はpin interface/medium、9はpin作成、10はACQUIRE、11はbuffer、12はPAUSE、
13はRUN、14は観測不十分、15は停止要求、16は停止後観測、17はローカルログの問題。

UART IOCTLは非同期送信queueへの受付であり、ホスト受信完了の証拠ではない。
主担当が既存UART収集ログのbegin・各観測・endを確認する。ローカルログと標準出力にも
同じ内容を残す。UART送信が途中で失敗すると残りはログへ保存し、再生の観測を打ち切り
停止へ進む。end自身のqueue失敗はローカルのuart-queue-errorとprocess終了codeに残る。

## 回収済みログの解析

```bash
python3 tools/uart/analyze-mt8167-audio-first-playback.py \
  /指定作業ルート内のUART.log --output /指定作業ルート内のaudio-observation.json
```

解析器は保存済みファイルを読むだけで、シリアル・実機・runnerには接続しない。
runごとのCUR進行、codec状態、停止後mute、資源保持、欠落レコードをJSONへ出し、
`audible_verified` は常にfalseとする。実測ログがまだない段階では動作成功を記録しない。
