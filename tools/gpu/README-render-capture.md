# Mesa描画キャプチャの検査

`python3 tools/gpu/audit-render-capture.py /tmp/a04-wrt-render-detail --submit 0`

従来のshim形式を読み、BOのバイト列、ハッシュ、GPUアドレスを出力せず、マップの長さ・40ビット範囲・重複・不整合とgeometryのVDM開始位置を検査する。終了値1は監査が実行されたがfixture出力不可、2は入力不正。開始位置がマップ内でも参照全範囲や間接参照が収まるとは限らない。TPU等のレジスタ値は直接GPU仮想アドレスと解釈しない。これは安全なGPU送信や内容の秘匿性を証明しない。

対象のcapture用shimは、サイズ・samples・layers・region・mergeだけをHWRTとして記録する。HWRTのTPCアドレス・サイズ・ストライド、VHEAP・RTC、２組のPM MLIST・macrotile・region、２つのfreelist、ジョブとHWRT/コンテキストの対応が欠ける。全マップを無差別に保存し、参照範囲もBO内容の生成元も記録しない。書込み失敗も確実に報告しないため、ファイル長の検査は必要条件にすぎない。`contexts.txt`とBOの生データは公開しない。

今後のfixture生成には、独立した入力を用意し、少なくとも次の根拠が必要。

1. Mesa・shim・テストプログラムのソースコミット、差分、ビルド設定、実行バイナリのハッシュを一連の実行に結び付ける。キャプチャ対象が合成データのみであることも検証する。
2. コンテキスト・HWRT・freelist・submitの識別子とライフタイムを結び付け、全HWRT入力と２つのRT設定を欠落なく記録する。
3. geometry、fragment、VDM、PDS、USC、PBE等の実際のエンコードを解析し、読み書き・実行範囲と間接参照を再帰的に特定する。GPUアドレスのエンコードと範囲を検証し、重複・別名・境界またぎ・オーバーフローを扱う。
4. 最初にゼロ化した公開可能な合成バッファを構築し、意味と生成元を確認できた命令・定数・参照だけを書く。未分類のバイトをBOからコピーしない。GPU書込み領域も適正な大きさと初期状態を確保する。
5. GPU VAと全領域のアクセス権・配置・寿命・同期、HWRT、freelist、ファームウェアとの対応を検証する。固定値は実際に使用されるGPU仕様と独立に照合し、既存のstream decoderの成功だけを安全性の根拠にしない。
6. 同一入力からバイト単位で再生成し、範囲外参照、未知命令、欠落、汚染や重複の失敗試験を行う。公開物にホストポインタや未検証生BOがないことを確認する。

現状、これらを満たすfixture生成器と実機実行経路は未実装。この監査器は意図的にfixtureを書き出さない。


## 構造化された再キャプチャ

`build-render-capture.py` はMesaの固定コミット `ef11349e8e5d620a2227fac0eed17e7ed1f86428` とクリーンなcheckoutを要求する。新しいMeson build directoryを用意し、`-Dplatforms= -Dgallium-drivers= -Dvulkan-drivers=imagination -Dopengl=false -Degl=disabled -Dglx=disabled -Dtools=drm-shim -Dbuild-tests=false --buildtype=release` で構成する。LLVM、Clang、libdrm等の依存とMesonは別途必要。環境ごとの依存バージョン差によるバイナリ一致は保証しない。Mesaのcheckoutは変更せず、一時ディレクトリのshimソースにフックを組み込む。

```sh
python3 tools/gpu/build-render-capture.py --source MESA_SOURCE --build MESA_BUILD --output NEW_SHIM_DIR
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim NEW_SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --output NEW_PRIVATE_CAPTURE_DIR
python3 tools/gpu/check-render-trace.py NEW_PRIVATE_CAPTURE_DIR/events.jsonl
```

試験ソースはクリーンなUniXNU-a04br3の `5980bb779d837b9a00e03b799dc8119bee2cae4b` に固定する。shimはBVNC `22.67.54.30` を明示し、キャッシュを無効化する。このBVNCはキャプチャ用に選択したモデルであり、実機のCPU/GPU識別を示さない。出力ディレクトリは新規作成し、トレースは0600で生成する。run manifestにはソース・バイナリ・イベントのハッシュ、試験の終了値を残す。no-op shimでは実際にGPUが動かず、画素検査は失敗する。

再キャプチャはHWRT全入力、freelist、VMの作成・マップ・解除・破棄、コンテキストとジョブの関連、BO作成・CPUアクセス要求・GEM closeをJSONLに記録する。各新規BOはshimによりゼロ初期化してから利用者へ返し、再利用されたshimメモリの残留を防ぐ。CPU側がその後に書いたバイトの由来は追跡しておらず、コマンドストリームは非公開のJSONLに記録する。BOの生データは保存しない。GEM close後もGPU側マップが生存する場合を区別し、GPUが処理したとは扱わない。DRM fd closeやインポートBO、並列スレッドの厳密なイベント順序は未追跡であり、現段階の判定は出力不可とする。

`check-render-trace.py` はsubmit時のfreelist全範囲とTPC範囲、その他のHWRTアドレスの開始位置を検査する。後者はサイズ不明のため全範囲の検証ではない。ジョブの間接参照や命令は未検査。トレースが完全に記録された保証には、captureしたアプリケーションとshimの終了状態、オブジェクト寿命、GPU側書込みの扱いをさらに厳密に実装する必要がある。


## 既定OFFの固定geometryテンプレート

以下の生成物は**オフライン解析専用**で、実GPUに送信できるfixtureではない。明示フラグがない場合は出力を拒否する。

```sh
python3 tools/gpu/generate-render-fixture.py NEW_PRIVATE_CAPTURE_DIR --submit 0 --output NEW_PRIVATE_FIXTURE_DIR --experimental-incomplete
```

固定64×64、単一sample・layer、対象モデル `22.67.54.30` のgeometryについて、記録されたMesaのレジスタ整数だけを、値と形式を限定して取り出す。GPUアドレスとエンコード済みTPUフィールドはゼロにする。`zero-initialized-mappings.bin` は新たに全バイトをゼロから作り、BOの内容をコピーしない。`fixture-plan.json` にGPU仮想アドレス、対象マップ、ファイルオフセット、個別の必要範囲を記す。同期・DMAページ取得・実行権限設定・ジョブ投入には接続していない。

`pvr_arch_job_render.c`、`pvr_free_list.c`、`rogue_hw_utils.h` と `rogue_hw_defs.h`、`csbgen/rogue/cr.xml` の固定Mesaコミットの割当式を使用する。今回の値はTPC使用・割当とも128バイト、VHEAP 1536バイト、MLIST各4096バイト、region各128バイト、local/global freelist表192/262144バイト。RTCとmacrotile配列は非使用。２組のRT間strideも記録と照合する。freelist表以外のPM物理ページは別途必要で、ここには含まない。

**診断パッチの制約**: Mesa原版の `pvr_arch_rt_mtile_info_init` はSIPF有効時に版数2をassertする一方、同じ固定ソースのGE8300設定は版数1。修正前のassert無効ビルドから得たTPC 32バイト・region 64バイトの値は不整合で、生成器はそのキャプチャを拒否する。下記診断パッチでSIPF1を１マクロタイルにした場合、64×64ではTPCとregionが各128バイトとなった。これは機種側Linux由来のローカル修正と整合するが、実機での正しさは未検証。

未知のVDM参照、PDS/USCコード、fragment側参照、BOへのCPU書込みの由来、PMページとGPU書込み、キャッシュ整合性は未解決。開始位置確認を全参照範囲の証明に使わない。manifestのハッシュは入力ファイルの変更検知であり、信頼できるcaptureの暗号学的な署名ではない。traceまたはmanifestを第三者へ公開する場合は、GPUアドレスやその他の情報が含まれる点も考慮する。


## SIPF1診断とWindows側の条件

`build-mesa-sipf1-diagnostic.py` は固定Mesaソースと対応するビルドを検査し、別ディレクトリに一時ソース、差分、オブジェクト、ライブラリ、manifestを生成する。Mesa checkoutと既存ライブラリは変更しない。対象のrender翻訳単位のみ `-UNDEBUG` でassertを有効にする。`--original-assert` は無修正の対照、フラグなしはSIPF1分岐を追加した診断版。

```sh
python3 tools/gpu/build-mesa-sipf1-diagnostic.py --source MESA_SOURCE --build MESA_BUILD --output NEW_DIAGNOSTIC_DIR
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim NEW_SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --diagnostic NEW_DIAGNOSTIC_DIR --output NEW_CAPTURE_DIR
python3 tools/gpu/generate-render-fixture.py NEW_CAPTURE_DIR --output NEW_FIXTURE_DIR --experimental-incomplete
```

GE8300のfeature表はSIPF版1・16×16 tile。機種側 `UniXNU-a04br3` のLinux由来 `pvr_hwrt.c` はコミット `36cee00` のローカル変更で、版1のmacrotileを画面全体とする。診断版Mesaは同じ条件とし、版2の既存分岐はそのまま残す。これは上流Linuxそのままの仕様と主張するものではなく、GPU実機や仕様書による独立検証もない。

Mesaの単一layer割当ではTPCのlayer strideは0、RTCは不要、SIPFのmacrotile配列は不要。Windowsの `render_hwrt.c` は単一sample・単一layer・SIPF1専用なので、その条件でstride 0とRTC・macrotileのアドレス0を受け入れる。指定された非ゼロの任意GPUアドレスには従来の範囲・アラインメント検査を行い、TPC・VHEAP・MLIST・regionとfirmwareアドレスは引き続き必須。メモリ範囲、同期、ハードウェアでの安全性は呼出し側の別途検証が必要。

旧生成器が出力した不整合captureに基づくテンプレートやマップは使わない。生成器はSIPF1パッチと対象翻訳単位のassert有効化が記録された診断captureだけを受け入れる。manifestは自己申告の整合性記録であって、暗号学的な信頼証明ではない。

## PDS/USCアップロード範囲の診断

固定SIPF1診断ビルドを入力として、別ディレクトリに次の診断ライブラリを生成できる。

```sh
python3 tools/gpu/build-mesa-upload-diagnostic.py --source MESA_SOURCE --build MESA_BUILD --sipf1 SIPF1_DIAGNOSTIC_DIR --output NEW_UPLOAD_DIAGNOSTIC_DIR
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --diagnostic NEW_UPLOAD_DIAGNOSTIC_DIR --output NEW_PRIVATE_CAPTURE_DIR
python3 tools/gpu/check-render-uploads.py NEW_PRIVATE_CAPTURE_DIR
```

固定Mesaソースの `pvr_gpu_upload_usc` と `pvr_gpu_upload_pds` を診断用の一時翻訳単位で計測し、コード・データのGPU仮想アドレスと実際のコピー長を記録する。BO内容・CPUポインタ・機械語は記録しない。全BOや全PDS/USC生成経路を捕捉するものではなく、読み込み元の安全性、relocationや間接参照、正しいジョブとの対応を示すものでもない。範囲検査はcapture全期間内に該当マップが存在したかの確認で、ジョブ実行時点の有効性を保証しない。capture時にshimはGPU処理を実行しない。生成した情報のままGPUへ送ることは禁止する。

`render_execution.c` は、所有範囲のページ整列・VA/DMA範囲・重複を検査し、同期、MMU反映、送信、両フェンス、CPU同期、firmware cleanup、MMU解除の順をコールバックで表すホスト検査用の基礎部品。Windows側に接続されておらず、ハードウェアに送信しない。失敗がMMU反映以降に起きた場合、状態をpoisonedにし、独立したリセット・停止確認なしでDMAを解放できると判断しない。フェンス通知単体でcleanupが完了したとみなさない。

実GPU用のfixture・Windows側のDMA確保と同期、完了・cleanup処理は未完成。特に固定テストに必要なVDM/PPP、PDS/USCの全到達範囲と生成元、fragmentの埋め込みアドレス、freelist用PMページ、エラー時の停止と回収を確定する必要がある。

アップロード診断は、PDS/USCの範囲をVMイベントと同じ非公開JSONLにも記録する。`check-render-trace.py` は記録時点のマップに範囲が収まるか、複数VMに曖昧に一致するかを数える。`render-upload-candidates.py EVENTS_JSONL` は各geometry/fragmentジョブ時点で残るアップロードを候補として数える。上書きが観測された候補は除外するが、計測対象外のCPU書込みや間接参照は追跡しないため、候補は到達証明ではない。並列イベントの厳密な順序も未保証。

Windowsの `A04_GE8300_RENDER_EXPERIMENT=1` は、診断用の結合geometry/fragment kickとfirmware cleanupを許可するコンパイル時スイッチで、既定は0。cleanupはreturn slotの応答のみを使い、execution counterの変化だけでは成功にしない。BUSY、応答なし、poll failureでは資源を保持する。ホストmockのみで検証しており、実GPUの結果ではない。VAの完全な所有確認、DMA取得・同期、固定fixture、cleanup後の解放をつなぐ実行パスは未完成。
