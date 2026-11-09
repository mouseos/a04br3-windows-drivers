# run175で確認した標準APIの範囲

Windows RT 8.1 ARM32 / MT8167 / PowerVR GE8300。実GPU354 SYS SHA256 `221e8c430b9cdff0c6d237ec777f19224cb41d8f05565799995158ce58e7e2dc`、BuildID `GPU354-texture-temp-contract-v1`、client SHA256 `9934ba6bf9741630a544bf096ea3261c717d45873f91b899cc7e3eae62466add` を同起動の結果へ照合した。PID2568、GPR2 slots32～43 checksum一致、全PASS。

有限2×2 textureはPOINTの赤/緑/青/白、LINEARの中央平均/上側平均/範囲外UV clamp、default sampler中央の8ケース。各内部149784＋外部322536＝472320画素、境界7680画素除外、UNORM8各channel±2、不一致0。先行する2色Clear、単色三角形、色補間三角形もPASS。Diagnostic terminal0、client exit0、install/diagnostic volumeFlush0。fresh176 manifest SHA256 `d0e0c86eaf67ee1b2ccc487fa6e84e8a6fc40760c34df9b5a8c8062323d6bb0b` と原41ファイルの回収・native errors[]を照合した。

ローカル正本は `port/gsi/logs/ios-feasibility/windows-rt-combined-run175/standard-D3D-texture-proof.json`。Windowsバイナリ・private hives・実機dumpはこの公開文書へ収録しない。

標準Bluetooth radio列挙はrun171-diag1で成功。MT6630 client2018/source `aa4233fd` とprovider1027/source `73427b48`、root/child Problem0、RADIO_FIRST_PRESENT1/RADIO_ERROR0/RESULT0。ペアリング・RF通信は未確認。

未完了：標準DXGI Presentと通常Windows/DWM画面へのGPU出力、物理画面確認、任意shader/texture、全機能・長時間安定性・電源9F。現DODのCPU presentationや古いPOST直接転送を、今回の標準D3D readbackや通常デスクトップGPU化と同一視しない。次の355は別branchで所有権・実GPU転送・表示退役の契約を結合する作業であり、354/175固定成果物を変更しない。
