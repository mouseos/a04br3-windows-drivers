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
