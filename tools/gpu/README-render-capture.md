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

`run-render-capture.py --dump-control-stream` はMesa自身のcontrol streamデコーダを有効にし、非公開の `probe.log` とmanifestのハッシュを生成する。**probe.logにはホストアドレスや生BOが含まれ得るため公開しない。** `summarize-render-csb.py CAPTURE_DIR` は内容を転載せずブロックとフィールドの件数を集計し、PDSアドレスの先頭が、いつか記録されたupload範囲内かを照合する。Mesaのdumpは最初のcontrol stream BOのみを解析し、照合はジョブ時点の生存や全到達範囲の検証ではない。

`windows_render_memory.c` は既定OFFの試験用部品。DMA common bufferの取得・ゼロ初期化、所有領域の範囲と重複検査、BRN65273領域と指定VAのページテーブル生成、全域のCPU→GPU同期とMMU無効化待ちを行う。fragment PDSの部分生成のみを接続し、完成fixtureとキックには接続しない。失敗時もメモリを保持し、解放にはprocessor停止だけでなく、呼出し側が別途確立したGPU DMA停止の確認を要する。実行中のfree、WDDM、電源遷移、実機上の整合性は未検証。

## コマンドバッファ内PDS直接構築の追跡

`build-mesa-pds-writes.py` は固定Mesaの `pvr_arch_cmd_buffer.c` の一時翻訳単位に計測を追加する。Mesa checkoutは変更しない。

```sh
python3 tools/gpu/build-mesa-pds-writes.py --source MESA_SOURCE --build MESA_BUILD --sipf1 SIPF1_DIAGNOSTIC_DIR --uploads UPLOAD_DIAGNOSTIC_DIR --output NEW_PDS_DIAGNOSTIC_DIR
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --diagnostic NEW_PDS_DIAGNOSTIC_DIR --output NEW_PRIVATE_CAPTURE_DIR --dump-control-stream
python3 tools/gpu/check-render-trace.py NEW_PRIVATE_CAPTURE_DIR/events.jsonl
python3 tools/gpu/summarize-render-csb.py NEW_PRIVATE_CAPTURE_DIR
```

`pvr_setup_vertex_buffers` と `pvr_setup_descriptor_mappings` は `pvr_arch_cmd_buffer_alloc_mem` でPDS suballocationを取得し、そのmapped先へconstant-mapに従って直接書く。このため `pvr_gpu_upload_pds` の計測だけでは記録されない。診断では構築先GPUアドレス・範囲・entry種別とオフセット、spill/scratchの情報BOと実バッファのGPU範囲、vertex DOUTUのshader範囲を記録する。host pointerやBOのバイト列はイベントに含めない。control stream dump自体は機密を含み得るので非公開とする。

固定試験の２組で、VDMの `vs_pds_data_base_addr` はvertex属性の直接構築、PDS stateの `pds_data_addr` はdescriptorの直接構築に対応した。観測したvertex entryはliteral32とDOUTU、descriptor entryはliteral32とspecial bufferで、後者にはspillおよびscratchが含まれる。今回のdescriptor各組についてspill領域8192バイト、scratch領域49152バイトの記録があり、それぞれの情報BOとバッファの範囲は記録時点で同じVMのマップ内だった。これは一時診断の結果で、shader命令、全参照の寿命、CPUによる他の書込み、fragment側の完全性を証明しない。固定fixtureは引き続きGPU投入不可。

## PDS定数の再構成

`reconstruct-pds-data.py` は固定試験のvertex属性とdescriptorの２種類のconstant mapについて、型付きentryとGPUアドレスだけからバイト列を組み立てる。生BOは読み込まない。対象以外のentryやサイズ、範囲外のアドレス、重複、記録時点でマップされない依存先、元の構築範囲のFNV値との不一致を拒否する。FNVは衝突耐性のある安全性証明ではなく、literalの由来や全GPU参照の完全性も証明しない。記録と生成物には機密値が含まれる可能性があるため非公開で扱う。

```sh
python3 tools/gpu/reconstruct-pds-data.py PRIVATE_CAPTURE --index 0 --output NEW_PRIVATE_DIR --experimental-incomplete
```

`--index 0` と `--index 1` は観測された２組を選ぶ。独立した２回のshimキャプチャで各組を再構成し、構築時のFNV値との一致を確認した。`render_pds_data.c` は対応する配置を、呼出し元が所有するGPU範囲とアクセス権を検査した上で組み立てるホスト検証済み部品で、GPUドライバの実行経路には接続していない。shader命令・fragment・VDM/PPPの全間接参照、固定payload、DMA/kickとの統合、実機検証は未完了であり、出力はGPU投入禁止。

## fragmentとPPPの対応

再構築した診断ライブラリによる新しい非公開キャプチャでは、`check-render-trace.py` がfragmentのPDSデータ・コードとUSC shaderの確保範囲が同じVMに存在すること、書込みが追跡された範囲にentryが入ること、重なる後続uploadがあれば旧uploadを無効にすることを検査する。`check-render-fragment.py PRIVATE_CAPTURE` は固定probeの２件のPPP `shaderbase` とfragment PDSデータ先頭を順に照合し、コード先頭がデータ確保範囲の直後であることを確認する。Mesaの `pvr_arch_cmd_buffer.c` と `csbgen/rogue/ppp.xml` ではshaderbaseはコードではなくデータ先頭を指す。

観測した２件はそれぞれPDSデータ書込み32バイト、コード書込み4バイト、コード確保16バイト、USC書込み360バイト、USC確保448バイトだった。確保領域の残りをMesaが書き込んだと解釈してはいけない。記録時点の対応と固定probe固有の順序による照合であり、ジョブ投入時の全寿命、全CPU書込み、VDM/PPPの残る参照、命令の安全な生成は証明しない。非公開ログにはホスト情報が混入し得る。両検査器はGPU実行を許可しない。

さらに `pvr_arch_pipeline.c` の `pvr_pds_fragment_program_create` はstaging bufferを `vk_alloc2` で取得し、`pvr_pds.c` の `pvr_pds_kick_usc` は32バイトのデータ領域のうちDOUTUの８バイトだけを明示的に書く。`setup_pds_fragment_program` は後でその８バイトを更新するが、uploadはデータ領域32バイトをコピーする。従って残り24バイトの由来は未確認であり、ゼロ初期化を保証しない元のstaging bytesやそのままのBOをfixtureに使用しない。GPUに必要なDOUTUを型付き情報から生成し、残りをゼロから作る必要がある。

`reconstruct-fragment-pds.py PRIVATE_CAPTURE --index 0 --output NEW_PRIVATE_DIR --experimental-incomplete` は、検証した単一sampleのGPU shader entryとDOUTU制御値、Mesaの単一DOUTU命令から48バイトを生成する。データ内24バイトとコード確保内12バイトの未使用部はゼロとする。DOUTU先頭８バイトと命令４バイトについて、独立した２回のキャプチャでMesa生成結果のFNV値と一致した。`a04_render_make_fragment_pds` は同じ構成を所有済み実行可能GPU範囲を検査して生成するホスト側部品で、GPU投入経路には接続していない。FNVは非暗号学的照合であり、shader自体の内容や残る間接参照は検証しない。

## USC compilerの由来と書込み検査

Mesaの `pvr_arch_pipeline.c` は固定probeのSPIR-VからNIRとPCO IRを経て `pco_encode_ir` を呼び、vertexとfragmentの `pco_shader_binary_data` を `pvr_gpu_upload_usc` に渡す。`build-mesa-usc-diagnostic.py` は一時的なMesa診断ライブラリだけを作り、PCOの各符号化先を指定パターンで埋めてから生成し、compiler出力のstage・サイズ・FNVを記録する。upload直前にも同じ範囲のハッシュとGPUアドレスを記録する。BOや命令バイト、ホストポインタは記録しない。

```sh
python3 tools/gpu/build-mesa-upload-diagnostic.py --source MESA_SOURCE --build MESA_BUILD --sipf1 SIPF1_DIR --output NEW_UPLOAD_DIR
python3 tools/gpu/build-mesa-pds-writes.py --source MESA_SOURCE --build MESA_BUILD --sipf1 SIPF1_DIR --uploads NEW_UPLOAD_DIR --output NEW_PDS_DIR
python3 tools/gpu/build-mesa-usc-diagnostic.py --source MESA_SOURCE --build MESA_BUILD --sipf1 SIPF1_DIR --uploads NEW_UPLOAD_DIR --pds NEW_PDS_DIR --output NEW_USC_DIR
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --diagnostic NEW_USC_DIR --pco-fill 00 --output PRIVATE_ZERO --dump-control-stream
python3 tools/gpu/run-render-capture.py --mesa-source MESA_SOURCE --mesa-build MESA_BUILD --shim SHIM_DIR --probe-source CLEAN_UNIXNU_SOURCE --diagnostic NEW_USC_DIR --pco-fill ff --output PRIVATE_ONES --dump-control-stream
python3 tools/gpu/check-render-usc.py PRIVATE_ZERO PRIVATE_ONES
```

２回の独立したshim実行では、２組のvertex 312バイトとfragment 360バイトについて、PCO生成結果とPDSから参照されたuploadのハッシュが一致し、ゼロと全ビット１の事前初期化でも一致した。これは特定probeでの上書き状況を調べる診断で、衝突耐性のある一致証明でも、shaderの即値が安全である証明でもない。upload後の未計測CPU書込み、shader実行時の間接参照、VDM/PPPの全参照、ジョブ完了までの寿命は未確定。shader fixtureの公開やGPU投入、DMA/kickへの接続は行わない。

## VDMの取得範囲とDMAへの部分配置

`check-render-vdm-pds.py PRIVATE_CAPTURE` は固定probeの２描画について、VDMの参照先と直接構築されたPDS定数の位置、生成時に取得範囲全体が一つのVM mappingに収まること、PDSコードのupload開始位置を照合する。観測上、vertex定数は12バイト生成・16バイト取得、descriptor定数は24バイト生成・32バイト取得であり、コードuploadはそれぞれ12バイトと８バイトだった。取得末尾の４バイトと８バイトは生BOから取らず、`reconstruct-pds-data.py` の `*-fetch.bin` ではゼロを追加する。コードuploadの長さはGPU命令実行範囲の証明ではない。

固定probeのGLSLでは、vertexは `gl_VertexIndex` で３点 `(-0.75,-0.75)`、`(0.75,-0.75)`、`(-0.75,0.75)` を選び、z=0.5、w=1.0を出力する。fragmentは `(1,0,1,1)` を出力し、probeは64×64背景 `(0x22,0x44,0x66,0xff)` と比較する。頂点バッファやアプリケーションdescriptorは使用しないが、Mesa生成のspill/scratch等のPDS参照は別に必要。固定したprobe checkoutの `shaders/gen_triangle.py --check` が、GLSLから生成したSPIR-Vと同梱headerの一致を検証した。これはPCO出力の全即値や実行時参照を証明しない。

実験フラグ下の `a04_windows_render_stage_fragment_pds` は、取得済みDMAメモリ内の読み取り専用PDS/USC mappingを確認し、fragment PDSを公開前に生成する。USCは固定probeで観測した先頭entryと360バイトがmappingに収まることだけを確認し、命令の内容は確認しない。公開前の変更だけを許可し、既存の全域同期とMMU無効化へ渡す。ホストmockとARM向けの型チェックで検証した段階で、実際のWDKビルド、実機のDMA、shader投入、fence・cleanup・kickとの結合は未実施。GPU投入の許可にはならない。

`a04_windows_render_memory_bind_vm` は新規DMAページ表のルートを、そのfirmwareのheapに確保したメモリcontextに結び付ける。`a04_windows_render_prepare_context` は同じVMをgeometry／fragment contextに設定し、Mesaの最大深度１に対応する８バイトのVDM callstackが所有済み書込み可能範囲にあることを検査する。`a04_windows_render_publish_context` はcontextとリング、状態、fenceを同期した後、ページ表とVMを同期し、MMU無効化を待つ。VMルート、所有範囲やcontext間の参照不一致を拒否し、同期・無効化の失敗時は資源を保持する。呼出し側はfirmware arenaとcontextへのアクセスを直列化する必要がある。

この接続はkickを発行しない。Mesaのcontext switch用静的レジスタはVDM状態・geometry状態とstore/resume用のPDS/USCを参照し、その完全な生成・所有検証は未実装である。HWRT、freelist、固定三角形コマンドとshader、完了fence、firmware cleanupもこの接続から実行できない。ホストmockとARMターゲットのスタブによるコンパイルのみで、WDK・実機では検証していない。

## context switchの依存範囲とPM構築

`build-mesa-context-diagnostic.py --source MESA_SOURCE --build MESA_BUILD --uploads UPLOAD_DIR --pds PDS_DIR --usc USC_DIR --output NEW_CONTEXT_DIR` は、固定Mesaの一時翻訳単位にcontext switchの範囲記録だけを追加する。通常の `run-render-capture.py` に `--diagnostic NEW_CONTEXT_DIR --pco-fill 00 --dump-control-stream` を指定し、`check-render-context.py PRIVATE_CAPTURE` で検査する。BO内容やCPUアドレスはこの追加イベントには記録しない。

今回のshimでは、VDM／geometry状態２領域と２組のpersistent／shared状態・store／resume PDSデータ・コード・USCの計26領域が、render context生成時と９件のrender job記録時に同一VMへマップされていた。PDS／USCの記録されたuploadも各開始位置に存在した。これは確保範囲と書込み範囲の対応であり、命令に埋め込まれた参照、未使用padding、GPUによる後続アクセス、実機上の寿命の証明ではない。

実験用の `a04_windows_render_prepare_freelists` は、所有DMAの未マップ領域から、重複しない連続PMページを２つの非成長freelistへ割り当てる。GE8300の64バイトSLCラインに合わせ表を64バイト境界、ページ数を16単位に制限し、GPU側の表マップにはSLCバイパスを要求する。`a04_windows_render_prepare_target` は64×64・単一layerのTPC、vheap、MLIST、region、freelist表、callstackの全範囲と重複を検査し、firmwareのHWRTを構築する。ホスト試験にはlocal 64ページ、global 1024ページを使うが、このサイズで描画できる保証はない。

Mesaのfreelist BOはSLCバイパスとPM/FW保護を要求する。Windowsページ表ではLinux `ROGUE_MMUCTRL_PT_DATA_PM_META_PROTECT_EN` と同じbit 62を指定でき、freelist表には両フラグを必須とする。callstackや通常のHWRT領域にはPM/FW保護を付けず、公開前に記録された全mappingについて実PTEのDMAアドレスと属性を検査する。CPU側の書込み保護は提供しない。ホスト試験でbit 62の設定・欠落検出を確認したが、実GPUでの保護動作は未検証。context switchプログラムの全参照、HWRTとfreelistの実機整合、fenceとcleanupも未完了であり、GPU投入は禁止のままである。

公開前のページ表検査は、指定外のPC・PD・PTエントリとBRN65273のゼロページの改変も拒否する。４組のPD/PTを使う現在の実装では、同じ組は同じ2 MiB領域内に限定し、同一1 GiB領域に別の組を割り当てない。これにより共有leafを別領域へリンクした予期しないaliasを作らない。同期・MMU無効化のホストmockは通過したが、GPUのページ権限動作、WDKビルド、実機検証は行っていない。

context switchのpersistent PDSは `pvr_pds_generate_stream_out_terminate_program`／`pvr_pds_generate_stream_out_init_program` が生成し、persistent stateのGPUアドレスをLD/ST定数へ埋め込む。shared-register用PDSは `pvr_pds_generate_shared_storing_program` が生成し、shared状態の２アドレスとstore/load USCへのDOUTUを含む。対応USCは `pco_usclib_common` の事前コンパイル済み `CS_STORE_SR_1024_COMMON`／`CS_LOAD_SR_256_COMMON` からuploadされる。２回のshimキャプチャで26領域の種類・サイズが一致したが、USC命令の固定化、残りの参照閉包、kickとcleanupは未完成。

`render_context_pds.c` はGE8300用persistent state 128バイトのstore／resume定数（各32バイト）と、shared state 16432バイト・USC entryを参照する定数（64バイト）を、所有GPU範囲を検査した上で生成する。persistent store／resumeとshared用のPDS命令もMesaのエンコード式から生成し、未使用部分はゼロとする。２回の独立したshimキャプチャで、定数８範囲と命令８範囲それぞれのMesa生成時FNV値と一致した。`check-context-pt-data.py PRIVATE_CAPTURE` で照合できる。FNVは衝突耐性がなく、USC命令、他の間接参照やGPU実行を検証しない。

`a04_windows_render_stage_context_pt` はpersistent定数と命令を、公開前の所有DMAの読み取り専用PDSマップへ配置する。stateの書込み権限、４出力範囲の重複、実PTEを確認し、成功時だけ書き込む。shared用PDSとUSC、固定fixture、kick、fence・cleanupには接続していない。Mesa `usclib/common.cl` のshared store/loadは、PDSから受け取ったstate先頭に保存されたサイズを読み書きし、同領域の48バイト後から可変長のshared registerを転送する。GPUによるサイズ値と全転送範囲の正当性、対応するUSC命令の固定化は未検証。

`generate-context-usc.py --source MESA_SOURCE --build MESA_BUILD --capture-zero PRIVATE_ZERO --capture-ones PRIVATE_ONES --output NEW_PRIVATE_DIR --experimental-incomplete` は、固定checkoutのMesa PCOコンパイラを同一SPIR-Vから２回起動し、生成C中のstore／load USC配列だけを抽出する。既存ビルドと２回のshimで観測した対応uploadのFNV値にも照合し、生成物とコンパイラ・SPIR-VのSHA-256を非公開manifestに記録する。生成された候補はstore 544バイト、load 488バイト。BOはコピーしないが、コンパイラ出力内の即値の全由来、GPUが生成するsharedサイズと間接アクセスは未証明なので、出力は非公開・GPU投入禁止である。これは安全な固定fixtureや実行経路ではない。

`analyze-context-shared-bounds.py` は固定Mesa `common.cl` のループを32ビット符号なし演算で評価する。storeは `SH_ALLOC_SIZE` を16 DWORD単位として取得し、loadは保存済み値を読み、残量からそれぞれ毎回1024／256 DWORDを減算してゼロで終了する。Mesaの `pco_trans_nir.c` は前者を特殊レジスタ `PCO_SR_SH_ALLOC_SIZE` から読み取る。生成時の最適化後NIRでも、ゼロ比較、固定量の減算、アドレスの4096／1024バイト加算が残ることを確認した。

ソース上、例えばサイズ１（16 DWORD）は両ループで固定量減算後に符号なしで周回し、ゼロへ到達しない。storeはサイズが64の倍数、loadは16の倍数なら有限回で終了し、サイズ256までなら確保された16384バイトのstate本体内に収まる計算になる。MesaのLLS定義は本体16384バイトと先頭予約48バイトを割り当てるが、GE8300の特殊レジスタがこの粒度・上限を必ず満たすこと、保存済み値が改変されないことは未証明。したがって少なくとも非整列値・範囲外値の扱いを解決するまでUSC候補を実行可能にしない。モデルはソースレベルの演算検査であり、GPU実行を模擬しない。

`build-context-shared-bounded.py --source MESA_SOURCE --build MESA_BUILD --output NEW_PRIVATE_DIR` は、独立した一時 `common.cl` に限りサイズ256超を０にし、残量を飽和減算する診断版を生成する。Mesa checkoutは変更せず、MesaのOpenCLコンパイラとPCOを使ってSPIR-VとUSC候補を２回生成・一致検査し、ソースと生成物のハッシュをmanifestへ記録する。ソースレベルモデルでは入力０〜256について両ループは有限回で終了し、範囲外は０として扱う。最適化後NIRにも上限比較と飽和減算が残ることを確認した。ただし範囲外を０にするとcontext復元は不完全であり、実行結果やGPU上のメモリ安全を保証しない。未検証の診断版で、Windowsドライバへ組み込まず、実機投入は禁止する。

`check-context-usc-ir.py --source MESA_SOURCE --build MESA_BUILD` は固定Mesaコンパイラを非公開の一時ディレクトリで再実行し、通常ビルドと対象USC配列の一致を確認した後、最適化後NIRの定数とメモリ操作を検査する。store／loadのアドレスはPDSが設定するvertex inputから、storeのサイズはISA上の特殊レジスタ番号103 `sh_alloc_size`、loadのサイズはGPUがstate先頭に書いた値から来る。NIR上の定数はregister index、ループ幅、アドレス増分、減算値に限定される。ただしNIRと機械語の完全な意味対応、機械語の全即値、実機での特殊レジスタの値域と粒度を証明しない。非公開の生IRは保存・出力しない。実機操作は行っていない。

`a04_render_check_saved_shared_size` は、完了fenceとCPU同期が呼出し側で既に確立した後に、shared state先頭４バイトをlittle-endianで読み、元のMesa store／load両ループが割当内で終了する条件（０、64、128、192、256）を検査するオフライン部品である。成功時にDWORD数とバイト数を返し、失敗時は出力を変更しない。これは保存値の事後観測にすぎず、その前に発生したGPUアクセスを防がず、特殊レジスタの値域やfence・キャッシュ同期を証明しない。fixtureやkickの許可条件には接続していない。Mesa由来のfirmware文字列にはTA contextのshared alloc sizeを記録する形式があるが、今回参照した既存資料から有効な実機値は得られていない。

`a04_windows_render_stage_context_sr` は、公開前の所有DMAにshared store／load用PDS定数と命令を配置する。16432バイトの書込み可能state、store 544バイト・load 488バイトの読み取り専用USC範囲、４出力範囲の所有・重複・実PTEを検査し、出力先がすべてゼロの場合だけ書込む。USCの内容・由来と他の後続配置との重複は呼出し側の責任であり、USCコピー、contextレジスタ、fixture、kickには接続していない。これは部分配置のホスト検証で、GPU実行や `sh_alloc_size` の実測を証明しない。

`a04_windows_render_collect_pair` は、別途投入済みの非partial pairに対し、geometry／fragment両fence値と両リングのread offsetが期待値になるまでCPU同期しながら待ち、payloadをCPU同期してからgeometry context、fragment context、HWRT、両freelistのfirmware cleanupを順に要求する実験部品。timeout、同期失敗、cleanup BUSY／失敗時はpoison状態で保持し、後続のcleanupを行わない。成功もMMU退役やGPU DMA停止の証明ではなく、allocationの解放には別途停止とDMA quiescenceが必要。入力pairの安全性を検証せず、kickも発行しない。ホストmockでのみ検証している。

`a04_windows_render_prepare_pair` は公開前に非partialのCCCBとkickを構築し、`a04_windows_render_publish_context` によるDMA同期・MMU無効化後、`a04_windows_render_submit_pair` がkickを１回だけ送る。送信が失敗した場合は到達の有無が不明なためpoisonし、再投入・解放を禁止する。`collect_pair` は送信成功前には開始できず、完了とcleanupの後も独立したGPU/DMA停止を要求する。これらは信頼された内部呼出し向けで、geometry／fragmentのアドレスやUSCの由来・動的範囲を検証せず、ユーザー向け入口も持たない。`A04_GE8300_RENDER_EXPERIMENT=1` でこれらをWDK providerのビルドに含められるが、固定fixtureも呼出し元も未完成で、このフラグを設定するだけでは描画しない。実機投入可能な構成ではない。

`summarize-shared-size-log.py PRIVATE_FIRMWARE_LOG` は、既に収集された `TA context shared alloc size store 0x..., load 0x...` 形式のfirmware traceだけを集計する。実機へ接続せず、元ログや他の内容を出力しない。ログの出自、記録フィールドと特殊レジスタ103の同一性、未観測値の範囲は証明できない。実機操作と採取は主担当が担当する。

`A04_GE8300_SHARED_TRACE_DIAGNOSTIC=1` は、明示的な `A04_GE8300_BOOT_TEST=1` と組み合わせる追加診断で、既定OFF。既存のfirmware trace２本をCPU同期してCSW 15のID候補を走査し、UARTへ候補数、store/loadの値0・64・128・192・256のビット集合、それ以外の値の個数、異なる組の数、走査中にtrace pointerが変化したかを一行で出す。生のtrace、タイムスタンプ、範囲外の生値は出力しない。通常のboot testではqueue test後のshutdown前、copy serviceでは状態照会時に出力する。保存したUARTログは `python3 tools/gpu/summarize-shared-size-log.py --uart PRIVATE_UART_LOG` で構造化できる。診断実行とログ取得は主担当が行う。

この走査はIDと引数位置だけに基づく候補検索で、レコード境界を証明せず誤検出があり得る。同期中にもfirmwareは書き込め、pointer不変でも整合したスナップショットとは限らない。既存queue testはrender context switchを発生させるものではなく、候補０件でも正常・安全・異常のいずれも判定できない。上記の値が特殊レジスタ103と同じか、対象firmwareに当該ログが実装されているかも未証明であり、fixtureやkickの有効化条件には使えない。

描画用 `a04_windows_render_collect_pair` では、呼出し側が `trace_capture_enabled` を明示した場合に限り、両fenceの完了または待機タイムアウト後、cleanup前に同じ候補集計を `trace_summary` に記録する。`trace_capture_status` と `trace_capture_valid` を確認する。同期失敗や不正なtrace範囲は描画の完了判定を変更しない。タイムアウト時は従来どおりpoisonして資源を保持する。これはkickの発行、外部への診断表示、context switchの発生を実装するものではない。

fragment PDS、persistent PDS、shared PDSの各配置は、成功した出力範囲をDMA allocationの寿命中に予約する。別の配置がpaddingを含む予約範囲と重なれば、書込み前に拒否する。未追跡のCPU書込みや別の方法による配置は呼出し側で防ぐ必要があり、この検査はfixture全体の参照閉包やGPU投入を保証しない。

`a04_windows_render_stage_vertex_pds` は、Mesa由来の型付き値からvertex・descriptor PDS定数とspill／scratch情報を生成し、所有済みの読取り専用DMA範囲へ公開前にまとめて配置する。vertexとdescriptorの取得範囲はそれぞれ16・32バイトとして予約し、未使用部分はゼロに保つ。情報領域、312バイトのvertex USC、spill 8192バイト、scratch 49152バイトの所有と権限、出力間や依存先との重複、配置済み範囲、実PTEを検査し、検査失敗時は書き込まない。USCの内容・即値、PDSコード、VDM／PPP、他の参照は検証しない。呼出し元のない既定OFFの部品であり、描画fixtureや実行を許可するものではない。

固定probeで観測されたgeometry１件・partial fragment１件・最終fragment１件の形に対応するため、`a04_windows_render_prepare_partial_pair` は公開前に２本のkickと完全なCCCBを作る。公開後はcombined kick、最終fragment kickの順に送る。どちらかの送信失敗時はpoisonし、再送や解放を許さない。完了待ちはgeometry fence値１と最終fragment fence値２、および両read offsetを要求し、partialの値１だけではcleanupを始めない。この部品もコマンドやfixtureの安全性を証明せず、実行入口には接続されていない。

`check-render-submit-dependencies.py PRIVATE_CAPTURE` は、固定probeの２描画について、VDMから参照したvertex／descriptor PDSと対応コード、vertex USC、spill／scratch、PPPに対応するfragment PDSとUSCが、記録されたgeometry jobの時点でも同じVMに残り、観測済みuploadがunmapや重なる後続uploadで無効化されていないことを検査する。固定probeの記録順と２描画に限定し、アドレスや生データは出力しない。両PDS再構成ツールもこの検査に通ったcaptureだけを受け付ける。これはgeometry投入時の記録された参照のみであり、未計測のCPU書込み、GPU完了までの寿命、VDM／PPPの全参照、命令実行範囲やUSCの動的アクセスを証明しない。出力は引き続き実行禁止。

固定probeのvertex／descriptor PDS命令は、Mesa `pvr_pipeline_pds.c` と `pvr_rogue_pds_encode.h` の式から生成する。vertexは頂点IDのDOUTW、DOUTU、HALTの12バイト、descriptorはspill／scratch用DOUTD２命令の８バイトで、両16バイト配置の余りはゼロ。`a04_windows_render_stage_vertex_pds_code` は公開前の所有済み読取り専用DMAへ配置し、既存の予約範囲との重複とゼロ初期化を検査する。

新しいupload診断はPDSコードについてもGPUアドレス・サイズ・FNVだけを非公開captureに記録する。`check-vertex-pds-code.py PRIVATE_CAPTURE` はVDMの２参照とこれらの生成命令を照合し、アドレスや命令バイトを出力しない。独立した２回のshim captureの各２描画で両命令列のFNVが一致した。FNVは衝突耐性を持たず、これはUSCやVDM／PPP、実機の動作、全メモリ参照の証明ではない。GPU実行は引き続き禁止。

固定三角形shaderのコンパイラ出力候補は、Mesa診断ライブラリを `build-mesa-usc-diagnostic.py` に `--private-compiler-candidates` を付けて構築し、通常のshim captureで取得できる。これはPCOコンパイラの出力バッファのみを非公開の新規ディレクトリへ書く経路で、生BOをコピーしない。`run-render-capture.py` の `--pco-fill 00`／`ff` の２captureを `check-triangle-usc-candidates.py ZERO ONES` で比較し、各shaderのSHA-256、PCOイベントのFNV、２初期化条件と２描画のバイト一致を検査する。候補はホスト由来即値の混入を完全には排除できないため、非公開で保持しGPUには投入しない。

同診断の `--pco-ir` はNIRを非公開probe.logに記録する。`audit-triangle-nir.py IR_CAPTURE ONES_CAPTURE` は固定probeのsource commit・SHA-256とshader候補の比較に加え、２組の最適化後NIRのintrinsicと定数、scratch格納位置、vertex IDの８倍を使う８バイト読込み、３頂点の非index描画を検査する。固定probeの `vkCmdDraw(...,3,1,0,0)` とVDM挙動を前提とすると読込みは24バイト以内。NIRと実機械語の完全な意味対応、実機のVDM挙動、context switchや他のGPU参照は未証明。`generate-triangle-usc.py --ir IR_CAPTURE --ones ONES_CAPTURE --output NEW_PRIVATE_DIR --experimental-incomplete` は上記検査後のPCO出力をvertex 384バイト・fragment 448バイトへゼロpaddingして非公開出力するが、manifestはGPU実行禁止のままにする。

NIR上の24バイト検査は、物理scratch allocation全体の境界を単独で証明しない。Mesa `pco_trans_nir.c` の `trans_scratch` はshared registerからscratch基底とblock sizeを読み、`local_addr_inst_num` 特殊レジスタをblock sizeに掛けた後にNIR offsetを加える。固定captureのblock size 24・allocation 49152はMesa `rogue_get_total_instance_count` の2048インスタンス分だが、GE8300で特殊レジスタが実際に常に0〜2047となることは未実測。したがって候補を実行可能とせず、値域の根拠を持たない小さいallocationへも接続しない。

`scratchless` 固定probeは `tools/gpu/shaders/triangle-scratchless.vert` を使い、頂点IDの条件式で同じ３頂点を生成する。`run-render-capture.py` に `--scratchless-vertex --pco-ir --pco-fill 00` または `ff` とprivate compiler diagnosticを指定すると、固定probeのsourceとshader headerを新規非公開ディレクトリへ複製し、glslangValidator・spirv-valで新しいvertex SPIR-Vを生成してからshim実行する。元のprobe checkoutは変更しない。manifestにはshader source・SPIR-V・両ツールのハッシュを記録する。

独立した２captureでは、各２描画の最適化後NIRは元と同じ頂点座標・深度を示し、vertex shaderのscratch intrinsicはなかった。grouped PCOも限定された頂点演算と出力命令だけで、vertexのload／storeは観測されなかった。vertex USCは176バイト（256バイト確保）、descriptorはspill情報１件の12バイト（16バイト取得）であり、scratch情報・scratch bufferを参照しない。vertex PDSコード12バイトとdescriptor PDSコード４バイトは、２captureでMesa uploadのFNVと一致した。`a04_windows_render_stage_scratchless_pds` と `_code` はこの別レイアウトを所有DMAへゼロpaddingして配置するが、shaderそのものの信頼性や他の参照を判定せず、既定OFFのまま実行入口には接続していない。元のscratch使用版とデータ・命令を混用しない。

この変更でvertex NIR上のscratch参照は除けるが、spill allocation、context switchのshared state、fragmentやVDM／PPPの全アクセス、USC機械語との意味対応、実機動作は未証明。captureはGPUを実行せず、非公開生成USCは引き続きGPU投入禁止。

scratchless版のPDS定数は、２回のcapture・各２描画でvertex literal `0x80000000`、DOUTU制御 `0x800000000`、descriptor literal `0x90000003` が一致した。`a04_render_make_scratchless_pds_data` と再構成ツールはこの固定値とspill-only構成を要求し、別のshader／layout用の値を拒否する。

scratchless三角形のVDMは、Mesa `vdm.xml` に従うPDS state・PPP state・VDM state・３頂点index list・PPP terminate・stream terminateの計60バイトとして `a04_render_make_fixed_vdm` で生成する。PDS・PPPの６参照範囲の所有、読み取り専用属性、アドレス幅、境界と重複を検査し、`a04_windows_render_stage_fixed_vdm` が未公開DMAへ配置する。独立した２回のcaptureで各２描画の生成時VDM FNVと一致し、PPP 100／12バイトがgeometry job時に対象VMへマップされていた。

PPPは固定probeの100バイト状態と12バイトterminateを `a04_render_make_fixed_ppp` で構成する。fragment PDS、fragment uniform code/dataは型付きGPUアドレスから埋め込み、その他はMesaのPPPパケット定義と固定probe条件から生成する。`a04_windows_render_stage_fixed_ppp` は所有DMAへ両状態を配置する。診断用Mesaは参照されたPPP範囲のGPUアドレス・長さ・FNVだけを非公開記録し、`check-fixed-ppp.py` で２回のcaptureについて一致を確認した。生BOをfixtureとしてコピーしない。

PPPのfragment uniformはspill情報と４バイトFS_METAを読み、固定captureのFS_META値は `0x01fffffe`、scissorは `(xmin,ymin)=(0,0)`・`(xmax,ymax)=(64,64)` の８バイト。Mesaの生成箇所から出力範囲とFNV／型付き値を診断記録し、２回のcaptureでpartial・最終fragment双方のscissor参照が対象VMに存在することを検査した。`a04_windows_render_stage_fragment_uniform` と `_fixed_scissor` はこれらをゼロ初期化DMAへ構成する。FNVは衝突耐性を持たず、これらの一致はジョブ完了までの寿命、render target・背景プログラム・shader全体・context switchの安全性や実機描画を証明しない。実行入口は未接続でGPU実行は禁止。

`check-fragment-background.py PRIVATE_CAPTURE` はscratchless固定probeのgeometry後の４fragmentについて、通常／partial両方の背景shader PDSデータ・コード、texture PDSデータ・コード、およびpixel event PDSデータが、記録されたfragment job時に同一VMに残ることを検査する。背景ごとの32バイトPDSデータ＋４バイトコード、８バイトtextureコード、12バイトtextureデータと16バイト取得範囲を照合する。pixel eventはMesaの生成点でデータ長・DOUTU制御値・USC範囲・FNVを非公開の構造化情報として記録し、16バイト取得範囲、128バイトUSC確保範囲と40バイトuploadの存続を検査する。２回の独立captureで各４fragmentの上記条件が一致した。`a04_windows_render_stage_pixel_event` は型付きUSC GPUアドレスと固定DOUTU制御から16バイトを所有DMAに生成し、後半８バイトをゼロにする。

これは背景textureの内部descriptor、EOT USC中のPBE状態、partial renderのSPM scratch、render target書込み範囲、全GPU実行中の所有・同期を検証しない。既存のfragment commandの値をそのまま実機投入することはできず、上記部品も実行入口に接続されていない。

pixel eventのEOT USCは `pvr_usc_eot` がPBE stateの２ワードを即値として埋め込むため、shaderだけを固定して別の出力先へ流用できない。診断はPBE生成時に画像のGPU基底・確保サイズ、出力先、寸法、stride、layout、２state wordを非公開記録し、pixel eventの同２wordとの対応を検査する。`check-fragment-background.py` はstate wordから出力先を復元し、64×64、stride 64、16 KiBの画像確保範囲がfragment job時に同一VMへ存在することを確認する。独立した２captureで各４fragmentに対応した。ただしlayoutの実際の書込み範囲、USC即値の全由来と命令の意味、partial時のSPM scratch、GPU終了までのマップ寿命は証明していない。記録されたstate wordやアドレスを生のfixtureとして公開・投入しない。

`build-mesa-spm-diagnostic.py` は既存のVDM・PBE・PCO診断と同じ固定Mesaソースとbuildを照合し、別ディレクトリの `pvr_arch_spm.c` に計測を加える。SPM loadのscratch、定数、PDSデータのGPU範囲と構成値だけを非公開イベントに記録し、BO内容やCPUポインタは記録しない。`run-render-capture.py` でこの診断を指定し、`check-spm-load.py PRIVATE_CAPTURE` で範囲が構築時に同一VMへ一意にマップされていることを検査する。成功時も終了コード1でGPU実行を禁止する。

scratchless固定probeの独立した00／ffの２captureでは、それぞれ３件のSPM load構築について64×64、１sample、出力レジスタ１、tile bufferなし、scratch使用・確保とも16384バイト、定数32バイト、PDSデータ16バイトだった。Mesaの `pvr_arch_cmd_buffer.c` は `barrier_load` のときにSPM背景状態をjobへ設定する。通常の背景状態と同一視できず、この検査は描画jobとの対応、GPU完了までの寿命、texture descriptorとUSCの実際の到達範囲、DMA同期を証明しない。生アドレスを含むcaptureは非公開のまま保持する。

`build-mesa-eot-diagnostic.py` はSPM・VDM・PBE診断の一致を確認してから、独立したMesaライブラリに背景load-opとEOTの計測を追加する。PCOへの00／ff充填を変えた２captureの固定probeでは、背景６構築すべてがclear１ワード、画像texture０件、tile buffer０件で、clear値はRGBA `22 44 66 ff` だった。fragment jobの通常・partial各背景について、PDSがこの４バイトを同一VMから１ワード読み出すこと、背景USCとPDSが各job投入時にマップされることを検査した。生成点の記録から16バイトの背景USC、32バイトPDSデータ、４バイトPDS shader code、８バイトPDS DMA codeを照合した。FNVは暗号学的な一致証明ではない。

EOTについてはcaptured PBE stateを含む基準入力の各64ビットを個別に反転したprivateなPCO出力と、２組の固定人工入力を検査した。40バイト命令列中のstate wordはbyte offset 16と4に配置され、残りが一致した。ただし一般の入力ではPCOが異なる長さの命令列を選ぶ。`check-eot-synthetic.py` は２captureのmanifest、候補SHA-256、PBEとupload、背景依存、個別ビットの結果を照合する。生のアドレスを含む候補・イベントは非公開とし、スクリプトの終了コード1は引き続きGPU実行禁止を意味する。

`a04_windows_render_stage_fixed_background`、`a04_windows_render_stage_background_shader`、`a04_windows_render_stage_fixed_eot` は、上の固定条件の背景定数・PDS・USC、および指定されたGPU出力先から生成したEOTを、公開前の所有済みDMAへ配置する。各出力範囲の予約・読取り専用PTE・ゼロ初期状態を検査し、未使用の予約領域はゼロにする。EOTは書込み可能な16 KiB出力先を予約する。これらは実行入口に接続されておらず、PBEの実際の書込み範囲、context switchの動的なshared state範囲、実行中の全参照と寿命、GPU cache同期を証明しない。背景がtextureを使わないという結果も、この固定clear probeに限定される。

`build-context-shared-bounded.py --compiler-diagnostic USC_DIAG` は、固定Mesaからshared state診断版を生成する。保存・復元の両方でサイズが256ブロックを超えた場合はループを実行せず、保存時は `0xffffffff` をstate先頭へ書く。残数の減算は０で打ち切る。元のMesaと異なる診断用shaderであり、範囲外のstateを復元できるようにするものではない。通常PCOで２回、さらにPCO出力領域を00／ffで事前充填して再生成し、対象２shaderの一致を検査する。`check-context-bounded-ir.py` は最適化後NIRの上限判定、ループ、markerと該当バイナリの一致を検査する。これはNIRから機械語への完全な意味対応や実機の特殊レジスタ値を証明しない。

`generate-context-bounded-fixture.py` は上記検査と固定SHA-256・PCO metadataを通したUSCだけから `render_context_bounded_usc.h` を再生成する。`a04_windows_render_stage_bounded_context_usc` は２shaderをゼロpaddingして所有済みDMAへ配置し、`a04_windows_render_stage_bounded_context_sr` は予約済みshaderの全バイト・paddingを再確認して、必要なtemp数からPDSのDOUTUを構成する。所有範囲、予約、実PTE、ゼロ初期化を確認するが、static context registerと固定描画への接続やGPU実行の承認は行わない。

persistent／shared context stateとHWRT、freelist表、callstackの書込み範囲は、各DMA allocationが独立したGPU/DMA停止後に解放されるまで予約する。互いの重複や既存配置との重複を拒否し、公開時にHWRTの７範囲が予約と一致することを再確認する。bounded stateは両fenceとCPU同期後に読み、無効値でもfirmware cleanupを試みたうえで `STATUS_DATA_ERROR` を返す。０はcontext switch発生の証拠ではない。firmware cleanup失敗・timeout時は従来どおりpoisonして保持する。これらはhost mock検証であり、実機上の同期や全間接参照の寿命を保証しない。

`build-mesa-context-static-diagnostic.py` は独立Mesaライブラリでrender contextの静的レジスタ値を型付きで非公開記録する。`check-context-static.py` は２slotそれぞれのstate base、store／resumeのPDS state、persistent PDSのdataとcodeの連続配置を生成点のmetadataと照合する。00／ffの２captureで一致した。MesaのLLS定義ではVDM stateが92バイト、TA stateが484バイトである。

`a04_render_make_context_static` は上記の固定レイアウトを所有範囲のGPUアドレスから生成する。`a04_windows_render_make_bounded_static` は先に配置された２組のbounded shared USC、shared PDS、persistent PDSと一致する場合に限り、ゼロ初期化されたSLC bypassのVDM／TA stateを予約してstatic registerを生成する。context生成にはその値をそのまま渡し、公開時にも固定値・shader・PDS・stateの予約と初期状態を再確認する。各部品はホストmockで検証した。機械語の意味、特殊レジスタの実機値、GPU cache動作、描画job全体やcleanup後のDMA停止を証明するものではなく、描画fixture・利用者向け実行入口は未完成である。

`generate-triangle-fixture.py` はscratchなし固定頂点shaderとfragment shaderについて、独立した00／ff capture、SPIR-V／NIRの固定probe検査、PCO出力SHA-256を照合する。fragmentのgrouped PCOは27命令群、固定したopcode集合、`sh0` と `sr53` のみを受け入れる。比較対象はMesa compiler出力であり、生BOは読まない。生成した `render_triangle_usc.h` は実験用コードで、GPU上の命令意味や特殊レジスタ値の証明ではない。

`a04_windows_render_stage_triangle_usc` と専用PDS／uniform配置関数は、生成shaderの所有DMAへの配置、検査済みshaderへのPDS接続、vertex／fragmentの各spill領域の別々の所有・ゼロ初期化・寿命予約を行う。公開時には固定shader、PDS、spill情報、FS metadataとspill領域を再確認し、不完全または改変された場合は拒否する。この検査もVDM／PPP、背景、PBE、partial renderの全依存やrender kickの安全性を保証せず、実機描画の入口には接続されていない。

`check-fixed-commands.py` はscratchless固定probeの最初の64×64描画について、geometryとpartial／最終fragmentのレジスタ列を型付きGPUアドレスと固定値から再構成し、２つのMesa captureで一致を検査する。border tableはMesaの128 format × 非圧縮／圧縮 × 16バイト × 64 entryで262144バイト。固定shaderと背景load-opにはtexture参照がないため、この診断では確保済み読取り専用tableをゼロにする。検査では投入時のmappingも確認するが、実GPUのアクセス範囲・機械語・cache動作は未証明。

`A04_GE8300_RENDER_DIAGNOSTIC=1` は `A04_GE8300_RENDER_EXPERIMENT=1`、`A04_GE8300_BOOT_TEST=1`、`A04_GE8300_ALLOW_MISMATCH=1` を必要とする、D0 entryで一度だけ動く既定OFFの診断。copy serviceや他のclear/copy/linear試験とは併用しない。固定DMA配置、firmware固定heapのVDM syncとpixel eventプログラム、bounded context、freelist／HWRT、背景・EOT・triangle・VDM／PPPを生成し、公開前に型付き参照と生成データを照合してからcombined kickと最終fragment kickを送る。geometry／fragment両fenceとring read offsetを待ち、CPU同期、bounded state検査、firmware cleanupへ進む。失敗やtimeoutでGPU到達の有無が不明な場合はDMAを保持し、再送しない。firmware power-off応答、BIF／SLC idle・outstanding確認とresetの成功後にのみDMAを解放する。

UARTの `A04GPU RENDER STATUS=... STAGE=... BG=... TRI=... OTHER=... BOUNDED=... B0_VALID=... B0=... B1_VALID=... B1=...` は集計値で、アドレスや生BOは出力しない。`STAGE=32` はfenceとcleanupを通った後の読戻しまで到達したことを表す。成功statusには背景・前景の両色、前景1128〜1176画素、その他０画素を要求するが、これはtwiddled bufferの色ヒストグラムであり三角形の位置や向きの検証ではない。`BOUNDED` 値０や保存サイズ０はcontext switch発生の証拠ではない。実機結果は未取得で、ビルド・host mock成功を実機成功として扱わない。
