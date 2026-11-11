# IA FLOAT属性のcanonical float4展開

slot0/per-vertex、連続した最大2 input registerという既存profileで、
`R32_FLOAT`、`R32G32_FLOAT`、`R32G32B32_FLOAT`、`R32G32B32A32_FLOAT`
を受け付ける。format照会ではこの4形式にvertex-buffer supportだけを返す。
既存BGRA/RGBA能力、native ABI、packet/private float4 packing、MP flagsは変更しない。

Microsoft [D3D11.3 Functional Specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm)
19.1.3.3は欠け成分についてRGB=0、A=1（FLOATでは1.0f）を規定する。
8.3.1/8.5.1のIAアドレス式はbuffer offset + stride * vertex index + element offsetであり、
strideは要素の実幅とは独立する。stride 0では同じ属性を繰り返し供給する。
4.4.6のalignment条件とstrideの最小幅を混同しない。
既存のstride上限65536、要素末尾65536、vertex index上限65534は維持する。
IA仕様全般の32bit wrapping/OOB zero returnを新規実装するものではなく、
既存有限profileのoverflow/OOB拒否を維持する。

Linux/Mesa側の一次実装との照合対象は公開snapshotの
`third_party/mesa-pco/generated/src/util/format/u_format_table.c` にある
`util_format_r32_float_unpack_rgba_float`、`util_format_r32g32_float_unpack_rgba_float`、
`util_format_r32g32b32_float_unpack_rgba_float`。
いずれも実成分だけを読み、残りRGB=0/A=1へ展開する。
Mesa snapshot/source plan自体は変更しない。

layout内部にregister別実幅4/8/12/16を保持する。bind時にはstride下限を設けない。
draw/indexed共通で64bitアドレスを計算し、既存の32bit overflow拒否後、
既存`geo_typed_read_resource`へ実幅を渡す。同関数の差分形式のresource範囲検査と
map/unmap同期・失敗伝播をそのまま使用する。
ゼロと1.0fの整数bit列を初期値とし、実成分をmemcpyする。NaN/Inf/負のゼロの
bitpatternを校正・有限化しない。下流の既存有限値検査は変更しない。

host fixtureは既存`standard_staging.py`/shared-surface fixtureと同じsource抽出方式で、
実layout/binding/draw/indexed/resource reader/format照会を実行する。
OS handle/mapサービスと最終GPU submitをmockするため、実WDK ABI、描画成功、
run232故障原因の解決は証明しない。

```sh
python3 drivers/gpu/mt8167/wddm-render/tests/ia_float_inputs.py --output <未作成output>
```

変更runtime TUはUMD `typed_draw.c`と`typed_entry.c`の2つ。
native67/KMD/PCO source・native map・compile flagsは不変。
ただし現`ge8300_native_compile_cache.py`はGPU全451入力をbyte照合し、
その中に両UMD sourceも含むため、今回の変更は既存cache admissionで拒否される。
「native依存無変更」を理由にguard成功とはしない。guardは変更しない。
署名、Windows compile/link、実機への投入はこのsource作業には含まない。


## optional UMD-only relink（通常cold entryとは別経路）

`tools/ci/relink-ge8300-ia-float-umd.py` は既定でread-only admissionだけを行う。`--execute-link` を明示した時だけ、28objectのうち旧ace26個と既ARMcompile成功323aの2個でarchive/DLLを作る。新compile、native67/KMD/PCO/client build、署名は行わず、67のbroad451 cache guardも呼ばない。既存通常cold入口とguardは変更しない。private artifact不要の通常cold buildの代替とはしない。このoptional入口では実保持artifactを明示入力する。

```powershell
python tools/ci/relink-ge8300-ia-float-umd.py `
  --baseline-stage <umd-verifiedディレクトリ> `
  --new-compile <回収compile-outputディレクトリ> `
  --new-compile-manifest-sha256 492758853be46ce70563d6daad8fa24dbf46acce5c5184fd3a772c053de84704 `
  --compiler-directory <既保持compiler-adaptersディレクトリ> `
  --output build/ia-float-umd-only
```

新旧実sourceを含むfull Git cloneで実行する（原ace recipe blobも参照）。旧UMD manifest `d9a8e631...`、旧broad451 witness `d12012a2...` はこの候補の実SHAへ固定。451の差がUMD2Cだけであること、旧28objectの実hashとarchive20memberの実bytes、26継承TUのsource、新2objectのsource/flags/clang/-MD/deps/logs、PCO605sourceとsource plan、30cbの原link/pair manifest、順序付き10libsとpco_pair.objを照合する。broad witnessのrecorded sourceはed360、UMDのcompiled sourceはaceであり、別fieldへ保持する。新2objectのcompiled source323aと現relink recipe HEADも分離する。

実行許可・source独立レビュー後だけ `--execute-link --llvm-lib <既存llvm-lib.exe> --lld-link <既存lld-link.exe> --readobj <既存llvm-readobj.exe>` を追加する。公開tool hashesを要求し、元recipeのarchive入力順20件、DLL直入力entry/typed_entry/archive/pco_pair、10libs順を維持する。新archiveの実member名順と各body hashを検査し、DLLのARMNT/imports/exportsを比較する。未使用の旧client objectも26保持に含むがclientを再linkしない。

`relink-receipt.json` は旧26compiled ace、新2compiled323a、PCO compiled30cb、relink recipe HEADを個別記録し、原manifestsをprovenanceへbyte保存する。入力を変更せず、失敗時は完了object/archive/DLL/logを保持し自動再試行しない。

この処理は厳密な旧環境全証明ではない。旧UMDの実効全argvや環境header全snapshotは保存不足であり、旧source recipeのflags・現source/実object/hash・新2TUの実依存に基づく限定候補である。`strict_cache_qualified=false`、`historical_environment_header_snapshot_complete=false` を維持し、元PCO receipt/current_source等を現sourceへ改名しない。source実装時はread-only admission/parseのみ、実archive/link/署名/実機は未実行。
