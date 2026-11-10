# BGRA/RGBA UI Texture2D: 寸法・row pitch・formatを実GPU descriptorへ接続

基点359 f4c80be6。Carson360 KMD start/resource/FW、Hilbert clock185、USB、EDKは変更しない。
既driverは2x2限定ではなく128x128/64KiB/SRV-BGRAのみ。2x2は旧client入力。
不足はtyped CreateResource/SRV→backend private allocation→geo_desc_valid→native Drawの
BGRA固定注入/descriptor dimension/stride validationが一貫して拡張されていないこと。

実装: single-mip/layer/sampleの通常Texture2D SRVで、合法なNPOT/長方形寸法と独立uploadpitchを受理。
Rogue descriptorのwidth/height14bit、stride15bitを超えず、既正規MDLの1allocation4MiB以内。
CreateResource内部pitchは64B行へ整列、Mapは実pitchを返し、InitialData SysMemPitchは独立に検証・行upload。
CPUuploadは標準resource初期化でありGPUdraw成功とは扱わない。row末paddingを画素に混ぜない。
BGRAはD3DDDIFMT21→DXGI87→swizzle2/1/0/3、RGBAはD3DDDIFMT32→DXGI28→0/1/2/3。
allocation.metadataにformatを保持してnativeで選択。CPU channel swapなし。RGBAのRT/primaryは追加しない。

根拠: Imagination MIT Mesaのrogue/texstate.xml IMAGE_WORD0(width34..47,height48..61)と
STRIDE_IMAGE_WORD1(stride0..14,texaddr16..53,mipmaps_present15)およびpvr_arch_tex_state.cの
width/height/stride minus-one、format-dependent swizzles/alpha_msb。strideが15bitなので
131072Bを超えるpitchはmipmaps_presentを汚すため明示拒否。
実Linux MT8167 m1.8ED4333936 core22.40.54.30とsysconfig.c UMA CPU/devphysical変換、
既WDDM MDL/PFN/lease/PT検査を保持。Linux m1.8をWindows m1.11と同一binaryと偽らない。

限界: 寸法encoding上限16384と、実allocation4MiB/合計16MiB(unified359)の予算は別。
全D3D法定最大サイズを同時residentに出来るという主張ではない。容量上限やKMD resourcesは変更しない。
mip/array/MSAA/sRGB/comparison/gather/loadの実装追加なし。現sample PSは有限でCB0.xy座標、
varying UVや任意DWM shaderを未実装のまま成功にしない。通常Present接続/POST問題はCarson/main担当。
本差分だけで通常UI/DWM完成、実GPU pixel成功、POST rootfixと呼ばない。

hostではproduction upload/geo_desc_valid/native descriptorをNPOT/portrait/長細/境界・両formatで実行、
padding保持とstridemipbit非汚染を検証。既16384extent descriptor/filter回帰を保持。
次GPU batchはmain最新freshとCarson360以後を前方結合し、実SRV allocation+SAMPLE+読戻しの
source/hash/format/pitch/expected pixelを結合して評価。359単独を新POST修正版へ巻戻す媒体にしない。
