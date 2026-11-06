# Windowsの画面取得・UART転送試作

`A04_TOPRGU_UART_SCREEN=1` と `A04_TOPRGU_CPU1_HANDOFF=1` を明示した４ CPU試験だけで
有効。既定はOFF。`A04_TOPRGU_THERMAL_STOP=1` と併用した場合、熱管理の
デバイス名を使用し、UART用のDOSリンクを同じデバイスへ付ける。両要求を同一の
並列キューで受け、熱管理のpingがUART要求の後ろに直列化されないようにする。
併用はビルド対象だが、実機での給餌間隔やD0遷移は未検証。
UARTはTOPRGUのみが操作し、SYSTEM／管理者限定IOCTLで最大96バイトを１件受ける。
送信は高分解能WDFタイマーの１回につき準備済みの１バイトのみで、ハードウェアを
待たない。watchdogと同じatomic gateを使うため、送信中に別の給餌処理は走らず、
競合するとその回を飛ばす。送信中以外はタイマー停止。D0退出時は同期停止する。
フレーム取得はユーザーモードのGDIで行い、物理FB領域はマップしない。

`uart-screen.exe` は現在のGDIデスクトップを二分の一幅・高さ、白黒１bitで送信。
`-f` は元解像度。各パケットを２回送り、各片と全体のCRCを付ける。既存UARTログの
保存は維持し、送信完了後 `python3 tools/uart/decode-windows-screen.py --log LOG
--output OUTPUT.pbm` で完全なものだけ復元する。ASCII UART出力は他ドライバの
ログと混ざり得るため、欠落やCRC不一致を拒否する。両複製が壊れた場合は失敗。
時間は画面サイズと実効UART・タイマー速度に依存し、取得ツールは15分で打切る。
既存UART監視を再設定しない。GDI画像と物理ディスプレイの一致は保証しない。

ホスト: `python3 tests/uart_screen_decode_test.py`、WDK: 画面取得プログラムと
TOPRGUをそれぞれ `tools/ci/build-uart-screen.ps1`、`build-mt8167-toprgu.ps1` で
ビルド。署名・WinPE組込み・実機試験は別途必要。画面データには表示内容が含まれる。
同時UART書込み、画面内容の鮮度、送信中のwatchdog期限、sleep／復帰は未実証。
高分解能タイマーはWindows 8.1/KMDF 1.13で使用可能で、相対負の期限を使う。
https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/high-resolution-timers
