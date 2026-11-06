# Recoveryで元MediaTek EGL/GLESのnative packetを捕捉する準備

実行は主担当がRecovery利用可能と明示してから。この準備では実機・ADB・runner・cameraを操作していない。通常Android起動、SurfaceFlinger起動、独自TA/3D packet送信は行わない。

## 実行可否

ARM64 Bionic上で同版vendor UM/KM/FWを使い、PowerVR hardware EGL ES2 pbufferが作れる場合は実行可能。現在のWinPEからは不可。保存済みソースは条件を満たす構成を持つが、現在のRecovery imageの実際のnode、mount、library、KM buildとの適合は未観測。

- device/benesse/a04br3/BoardConfig.mk: TARGET_ARCH=arm64、firmware_class.path=/vendor/firmware。
- kernel a04br3_defconfig: CONFIG_MTK_GPU_SUPPORT=y、MTK_GPU_VERSION=rgx clark 1.11ED、CONFIG_DRM=y、CONFIG_ION=y。
- vendor DDK config: BVNC22.40.54.30、native fence sync/ION。DRM ioctl registrationはPVR_SRVKM_CMDにDRM_RENDER_ALLOWを指定。
- ueventd.rc: /dev/dri/card0、/dev/pvrsrvkm、/dev/pvr_syncを記載。ただしファイルの記載はlive nodeの存在・対応driverの証明ではない。
- 元UM PVRSRVBridgeCall 0x11a7c0はioctl(fd,0xc0206440,&32-byte DRM package)。証拠ev_101d07674dbf0ec61bec9d853635560769085bb77cebfb8b42cf7da980e209b1。/dev/pvrsrvkmの存在だけでは不十分で、PowerVR DRMのcard/render nodeが必要。

## 必要なuserland

Bionic ARM64 linker64/libc/libdlと元vendor lib64/egl/libEGL_mtk.so、libGLESv2_mtk.so、lib64/libsrv_um.so、libusc.so、libIMGegl.so、同版FWが必要。Android EGL loader経由なら/system/lib64/libEGL.soとlibGLESv2.so、そのloaderが参照するvendor namespace、gralloc.mt8167.so等も解決すること。glibc executableにAndroid .soを直接dlopenする構成ではない。

AOSP recovery/minuiは通常EGLを必要としないため、Recoveryにこれらが既に入っているとは仮定しない。Loader.cppはro.hardware.eglやvendor/eglを参照してSP-HAL libraryをロードする。既存sourceのinit.recovery.mt8167.rcにはGPU専用mount/loader準備はない。主担当のread-only preflight結果で不足を特定し、必要ならhost側で対応ramdiskを準備する。今はmount・chmod・setprop・追加boot・書込みをしない。

## 準備したコード

- recovery-native-preflight.sh --inspect: nodeとfileのmetadataのみ。GPU fdを開かない。
- recovery-native-clear.c: --runとEGL/GLES絶対pathを必須とする。800x600、RGBA8、MSAA/Z/SなしをeglGetConfigAttribで厳密確認し、ES2 pbufferを作り、元glClear→glFinish→glReadPixelsを1回行う。capture環境変数がある場合はobserverの初期化完了をdriver load前に必須確認する。全480000画素のRGBA={17,34,51,255}一致を検査。PowerVR以外のrendererは拒否。30秒alarm。デフォルト起動はdriverをloadせずusage終了。
- recovery-native-bridge-capture.c: ARM64 LD_PRELOAD ioctl observer。A04_M111_CAPTUREの絶対pathをO_EXCLで新規作成する。元ioctlをそのままsyscallへ渡す。group0x82/function12のみ、input0x160・TA0x88・PR0または0x150・3D0x150を送信前にdeep-copyし、ioctl後のoutput12 bytesと結果を保存。packetを生成・書換え・再送しない。最大32 kick、形式相違や書込み失敗はbrokenとして記録する。CPU pointerは捕捉時点でcaller自身が所有するものに限る。Android SP-HAL namespaceがdefaultのLD_PRELOADを参照しない可能性があるので、CAPTURE_SUMMARY kicks>0を実測するまでinterpose成功と扱わない。必要なら許可後にvendor direct library pathを使うstandalone構成を選び、その依存解決を確認する。
- read-recovery-native-capture.py: 独自TLVの整合、pre/post pair、サイズを検査してJSONをstdoutへ出す。GPU成功とは判定せず、常に別途harnessのphase/readbackを要求する。

`build-recovery-native-probe.py --ndk <完全NDK> --output <root内成果物dir>`でprobe/observerだけをローカルビルドできる。標準NDKのaarch64-linux-android26-clangでprobeをPIEとしてlibdlへlinkし、observerを-fPIC -sharedでlibcへlinkする。observerのpthreadはBionic libcに含まれる。公式repository2-3.xmlのstable channelからNDK r30（30.0.16248370）を作業root内へ取得し、公式SHA-1一致とarchive sizeを検証済み。NDK API26の正規sysrootと-Wl,-z,defsでprobe PIE／observer shared libraryを最終linkした。DT_NEEDEDはlibc.so/libdl.soのみ、probe interpreterは/system/bin/linker64。未解決symbolを無視するlinkや偽Bionic libraryは使っていない。実行先にはAPI26以上の互換Bionicが必要で、実機起動・EGL依存解決・捕捉成功はまだ未検証。成果物はport/xnu/out/windows-rt-m111-recovery-linked-20261107/に固定ソースとhash manifestを含めて置く。

## 後日のREA runtime記録

`prepare-recovery-native-capture.py --serial <主担当確認serial> --bundle <固定成果物dir> --device-directory /tmp/<新規device側dir> --log-directory <root内ios-feasibility>`はstaging commandとREA tool引数JSONをstdoutへ生成するだけで、ADBもGPUも実行しない。serialは主担当が確認した実値を必ず指定し、実行許可前のtemplateにあるPRIMARY_APPROVED_SERIAL_REQUIREDを実値と誤認しない。

現在のREA observe_native_callsはMach-O/LLDB用で、Linux ARM64への関数attachには使えない。Recovery許可後はREA capture_process_scenarioで明示したADB/harness commandを起動してphase/終了状態をEvidenceにし、捕捉fileも対応付けて回収する。これは今実行する指示ではない。hostへの回収先は必ず指定TAB-A04-BR3 root内のport/gsi/logs/ios-feasibility/とする。

順序はread-only preflight→依存library/UM-KM version確認→明示許可されたpbuffer probe。harnessをA04_M111_CAPTURE=<device capture path>、LD_PRELOAD=<observer path>付きで起動し、元EGL/GLES libraryの絶対pathを渡す。capture fileは新規名を使い既存記録を上書きしない。driver binaryのhash、Recovery image/kernel/config、firmware hash、harness/observer hash、library選択pathをmanifestへ残す。

成功条件はPowerVR renderer、EGL/GL errorなし、glFinish到達、readback全画素一致、捕捉pre/postの対応、ioctl成功かつbridge eError=0、broken=0が同じrunで揃うこと。glReadPixels側のTQをclear TA/3Dと取り違えず、PHASE clear-begin/clear-finished/readback-beginとseqで分離する。ioctl受付やpacket取得だけで描画成功にしない。

packetのGPU VA/FW addressをWindowsへそのまま再送しない。captureは残るoffset・flag・code/resource参照を確定する一次資料として使い、Windows ownerに再配置するCPU producerへ反映する。code/resourceのmapping、lifetime、初期内容も別途照合する。

## 同版sourceからのcold zero条件

config_kernel_user_mt8167.hとeng版はPVR_LINUX_PHYSMEM_ZERO_ALL_PAGESを定義。physmem_osmem_linux.c:3710はfresh PMRのbZeroをTRUE、poisonをFALSEへ上書きし、gfp生成で__GFP_ZEROへつなぐ。ただしUMの再利用suballocation全体が毎回zeroになる保証ではない。Windowsのfresh/cold arena zeroはこのsource構成と一致する初期policyであり、FW consumerの読出し・書込み順まで証明したとは扱わない。

## run24 Recoveryの観測と/tmp stagingの条件

主担当のread-onlyログではaarch64/Linux4.14.141+、SDK29/VNDK29、ro.hardware.egl=mtk。card0/renderD128/pvr_sync/ionあり、pvrsrvkmなし。linker64は存在するがEGL/GLES/vendor UM/FWは欠落。DRM nodeの存在はUM/KM互換やGPU実行成功を証明しない。

libIMGeglのDT_NEEDEDはlibsrv_um/libm/libsync/libged/libgralloc_extra/libhardware/libz/libgpud/libc。ES2 driverは`/system/vendor/lib64/egl/libGLESv2_mtk.so`固定pathでdlopenする（REA dependency bundle参照）。LD_LIBRARY_PATHを/tmpに向けるだけでは固定pathを解決できない。元binaryを書換えず、主担当が別途承認するmount namespace/bindまたはRecovery imageへの正規配置が必要。firmwareもkernelの実際の検索pathを満たす必要がある。

指定libsrv_um hashとstock backup内の別版を取り違えない。同梱libgedはHIDL GPU 1.0、libgpudはgraphics common1.1/1.2とmapper2.0/2.1/3.0に依存。これらの.soが存在してもサービスが必要かどうかは別途runtime観測が必要。NDK sysrootのstubをruntime代用として配置しない。platform library候補は既存Android10 system imageから別directoryに抽出し、互換性未検証のままvendor正本と混ぜない。

REA providerの一時DB/PTY capture作業領域もroot内へ限定するため、既定/tmpで起動済みのserverは使用せず、root内TMPDIRを指定したREA CLIまたはREA MCP processを使う。capture-process CLIはscenario JSON pathを受け取る。生成planのrea_host_environmentとrea_cli_capture_NOT_EXECUTEDは起動条件の例で、実行はしない。root外の既存設定ファイルを変更する必要はない。
