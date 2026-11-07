# 標準clientの固定無人選択契約

Raman媒体serviceは署名済みclientを次の固定引数で一度だけ起動する。

```text
ge8300-d3d-clear.exe --hardware=PowerVR
```

オプション無しはinventoryのみ。既存 `--luid=HHHHHHHH:LLLLLLLL` は維持。
自動選択は既定OFF。serviceによるログ解析／二度目起動は不要。

選択は標準APIだけで行う：

1. SetupDiGetClassDevsW/EnumDeviceInterfacesでpresentなGUID_DISPLAY_DEVICE_ARRIVALを列挙。
2. 同interfaceのSP_DEVINFO_DATAから実HardwareID multiSZ、instance IDを取得・記録。
3. HardwareIDの完全一致 `ACPI\MOU0006`、実Serviceの完全一致 `GE8300Render` を要求。
4. そのdevice pathをD3DKMTOpenAdapterFromDeviceNameで開き、実AdapterLuidを取得。
   KMTQAITYPE_ADAPTERTYPEのRenderSupported=1/SoftwareDevice=0を要求し、handleを閉じる。
5. PnP候補が唯一である場合だけ、そのLUIDと一致するDXGI adapterを選ぶ。
   IDXGIAdapter1/GetDesc1のSoftware flag=0と、base descとのLUID/VendorId/DeviceId一致を確認。
6. DXGI一致候補も唯一の場合だけ標準D3D11CreateDeviceへ渡す。

PnP取得失敗を含む不確定、0件／複数件は拒否。first-adapter、表示名substring、WARP/REF fallback無し。
今回のKMDはPCI VendorId/DeviceIdを定義していない。推測IDをpinせず、上記の実PnP→LUID対応を使う。
Vendor/DeviceとSoftware flagは全DXGI候補について観測値を記録する。
`PowerVR`は明示opt-in tokenであり表示名検索ではない。

必須disklog `C:\MT8167D3DClear\clear-*.log` schema4に、PnP全interface/実ID/service/
KMT status・type・LUID、DXGI全候補Vendor/Device/LUID/Flags、PNP_MATCH_COUNT、SELECTED_COUNT、
SELECTED_LUID_HIGH/LOW、SELECTION_REASONを記録。
理由1は明示LUID、理由2は実PnP→KMT→DXGI非Software一致。
API_READBACK_SUCCESSと実pixel検査の成功条件は変更していない。
輸送としてUARTを要求しない。任意payload/brokerなし。

0f8f68d2の署名済SYS/DLL/INF/CAT/FWは保持し再compile/再署名しない。
この変更の成果物は別client EXEとbuild/signed manifestのみ。媒体はその固定hashを適用する。
DOD1.6 CPUおよび親1014の取り込みはmain/Raman所有。

一次仕様：
- [GUID_DISPLAY_DEVICE_ARRIVAL](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/guid-display-device-arrival)：Vista以降。render-onlyを含み、device nameをKMTへ渡す公式経路。
- [D3DKMT_OPENADAPTERFROMDEVICENAME](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_openadapterfromdevicename)：device nameと実AdapterLuidのmapping。
- [SetupDiGetDeviceInterfaceDetailW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinterfacedetailw)：同interfaceのSP_DEVINFO_DATAを返す。
- 実WDK8.1 km/d3dkmthk.hのD3DKMT_OPENADAPTERFROMDEVICENAME／KMTQAITYPE_ADAPTERTYPEでcompile確認する。

## Raman serviceとのtimeout固定

外部待機上限は **120000ms、retry0**。Ramanが同値を採用した。
client内部のblocking標準Mapに独自のtimeoutは設けていない。
期限到達はserviceの失敗結果として保存する。プロセス終了・強制終了をGPU停止／IRQ drainの
証明にせず、成功や自動retryへ変換しない。今回の自動client sourceは
`6303fe54dc745950ea79658c35677c820adeddac`、署名済EXE SHA256は
`fd6b20243bd991ba1c91c9d5c452ac3a29ddf1cc0cd23e630f928f2d288889d4`。
実WDK compile/link・BCBF署名Valid・回収hash照合済み。実機未確認。
