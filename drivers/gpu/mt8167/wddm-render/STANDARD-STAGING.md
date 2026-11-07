# 標準STAGINGSURFACEのCPU-only allocation

基点は実搭載GPU source f4f1b4f94ade6f59efef042f680c2c3e363442f8。
miniport/query_adapterは9f31ddcfのRT8.1契約修正をそのまま継承する。UMDとMapFlagsを編集しない。

WDK8.1 d3dkmdt.hはD3DKMDT_STANDARDALLOCATION_STAGINGSURFACE=3、
D3DKMDT_STAGINGSURFACEDATA={Width(in),Height(in),Pitch(out)}を定義し、
標準stagingは常にD3DDDIFMT_X8R8G8B8と明記する。
Microsoft LearnのSTAGINGSURFACEDATAも、backbufferから転送した後CPUがlock/readする面と説明する。
これはアプリのD3D11 staging CreateResourceが必ずこのcallbackを使うことの証拠ではない。
今回のrun97 STAGING_HRESULT887a0005/DDI80070057との因果は未確定で、mainの新traceで判定する。

既存geo_createにはGEO_USAGE_STAGINGをCPU-onlyにし、owner.createを呼ばない経路がある。
GetStandardAllocationDriverDataで標準stagingをX8形式のstaging descriptorへ変換し、
validatorでX8をstaging usageに限って認める。X8 render targetは拒否し、format capability広告を変えない。
geo_bindはCPU-onlyのsurface0を拒否し、clear/drawのprepareはRENDER_TARGET以外を拒否する。
既存KMD CreateAllocationはCPU-onlyをread/write segment set0、preferred system memory、CpuVisibleとして扱う。
GPUのX8 clear/draw/copy/Present対応を追加・広告したものではない。既存Present制限も維持する。

寸法は既存1024×1024以下、非zero、pitch=width*4、総byte<=4MiBに限定する。
乗算前に寸法を検査し、validatorもuint64_tでpitch*heightを検査する。
callbackはsize queryとprivate data fillを同じdescriptionで返し、ResourcePrivateDriverDataSizeは0。
WDKのAllocationPrivateDriverDataSizeはoutなので、入力capacityと誤認して変更しない。
shadowのA8R8G8B8経路は保持し、primary/GDI/他formatは成功扱いしない。

実source callbackをhost fixtureへ取り込み、size/fill、pitch、null/限界/overflow、canary、旧shadow、
actual geo_create/destroy/prepare/bindでCPU-onlyかつGPU owner call0を確認する。
実WDK8.1 compileは別途実headerで行い、fixtureだけをABIの証拠にはしない。

一次資料:
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dkmdt/ns-d3dkmdt-_d3dkmdt_stagingsurfacedata
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_getstandardallocationdriverdata
- WDK8.1 Include/shared/d3dkmdt.h STAGINGSURFACEDATA、Include/shared/d3dkmddi.h GETSTANDARDALLOCATIONDRIVERDATA

MapFlagsは本変更と独立であり、CPU_ACCESS直接通過の既存契約を変更しない。
実機はmain所有。ビルド成功と標準stagingの実機利用成功を区別する。
