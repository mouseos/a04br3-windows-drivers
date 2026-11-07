# fresh92の一回限り観測

`codec-render-observe-fresh92-v1` は07454fabの固定Engineを再利用する独立profile。
`A04_ENGINE_OBSERVE_ONLY=1` とpolicyの `DEFERRED_OBSERVE_ONLY=1` は相互一致必須。
PnP install本体、driver store copy、planned reboot本体・引数経路をcompileから除外する。
PnP install count=0、planned reboot count=0。既定の延期更新profileは維持する。

fresh92 manifest/full SHA256とnative seven-node receiptを明示し、全backup実体を照合する。
親/Audio1026、MSDC1.8、render3.13を現在選択として固定。source2d623172/e2db926f/b27cd0b3、RTC borrow flags、SYS/FW/UMD、USB1005/WiFi1012/DOD1.6を保持する。
packageのINF/CAT/memberも従来どおり検証するが、証拠用private package以外へdriverをコピーしない。

serviceは自己disable、CREATE_NEW markerと診断leaf、全保持照合、署名済みcapture61106acfとloader dcb21bc8の2clientを各1回実行、stdout/stderr・終了値・CM snapshot・flushの結果を保存して停止する。
旧run91 capture.logはfresh92の固定SHA256照合後に新Engine logsへMoveFile（置換不可）して保存する。
追加のMSDC速度読み取りは行わず、性能改善も録音成功も仮定しない。

CR_NO_SUCH_VALUE(37)はCM status成功、DN_STARTED、problem0、DN_HAS_PROBLEM無しのときだけ任意ProblemStatus不在として受理する。snapshotのproperty_cr=37、nt_valid=0、未取得ntstatus=FFFFFFFFは保持。型不正・別CR・非zero NTSTATUS・未開始・問題ありを成功に変換しない。

native fingerprintの追加許可はschema2/count4/clients2に限定したRetained0..2HardwareId（固定文字列・REG_SZ）と実測既存MSDC6 DWORDだけ。leaf64/allowlisted policy/兄弟値fingerprintは維持する。
旧service再armは固定run91媒体＋fresh92＋同driver packages＋新2clientに限定し、汎用同版retryを許可しない。

実機所有はmain。媒体生成・署名成功と実機での録音/D3D成功を区別する。
