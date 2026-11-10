# MediaTek MT8167 USB / inbox PowerShell起動barrier修正

USB trial199はSTART0→RUN_RESULT1790→SCM_STOP0→FLUSH0→TERMINAL1790。
PowerShell/PnPの実行成功やUSB給電改善はない。旧CのFILE-choice WinVerifyTrustは
失敗時のLONGをすべてERROR_TRUST_FAILURE(1790)へ潰していたため、trial199の実HRESULTは未採取。
本修正が、その過去HRESULTを推測で補うことはない。

## 実byteの根拠

- actual200: `Windows/System32/WindowsPowerShell/v1.0/powershell.exe`
- SHA256: `8266b956658681fe18f3851384ca8bb7f91a57a93dc29b91fd4519a6eea72052`
- 458240 bytes、ARMNT/PE32、PE security directory certificate_bytes=0。
- actual capture: fresh200 manifest `fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9`、Recovery boot `64657ae7-390c-431b-ae4d-ae5de209793b`。
- 既取得Microsoft回復媒体 `official/install.wim` SHA256 `b949656175c4e6c2d2c0c0b0915982b15c35387aaea6c3c267e7781e89b1216a` のindex1・同memberからhost抽出した原本と完全byte一致。

source pinは`tools/usb/powershell-system-image-pin.json`。
Microsoft binaryやprivate captureそのものは公開repoへ含めない。
CAT固有の名前・証明書は未解決であり、埋込署名0を「不正なOS binary」と判定しない。
追加CAT転送・圧縮component manifest展開は不要。

## sourceの変更

固定pathのinbox PSに対するFILE-choice WinVerifyTrustを起動可否のgateから除外する。
代わりに上記SHAをCへ固定し、既存hold()でファイル全byteをSHA256照合する。
holdはFILE_SHARE_READだけで開き、reparse/directoryを拒否し、CreateProcessから
子process終了までhandleを保持する。任意のexe、別path、変造PSへのfallbackはない。

起動は従来どおり通常のCreateProcessW。CI無効化、signature policy変更、手動mapping、
DLL injection、任意loaderや無署名driverの追加は行わない。OS自身のimage load CIを保つ。
CreateProcess失敗のWin32 codeをそのまま返す。失敗APIでGetLastErrorが0でも
ERROR_GEN_FAILUREとし、成功へ変換しない。

同署名EXEのobserver自身のFILE trust、署名済PnP helperの検証、candidate/rollback CAT検証、
既存停止証明・VBUS/OC保護は維持する。driver/helper/GPU/SDK source変更・再buildはない。
新SCM sourceだけの1 TU変更であり、実build/signはmainが新baseline入力確定後に行う。

## 証拠のevent

prefix/50-byte frameと既存event1..7の停止・flush・terminal gateは維持。
追加eventは診断のみで、単独ではRecovery回収や成功の条件にならない。

| Event | code |
|---|---|
| 8 OBSERVER_FILE_TRUST_HRESULT | observer自身のWinVerifyTrustの実32bit LONG。0以外も変換せず記録。起動結果への1790変換とは別に保持 |
| 9 PS_IMAGE_PIN | 0=exact image hash照合・hold成功、23=照合/hold失敗。失敗時はCreateProcessを呼ばない |
| 10 PS_CREATEPROCESS | 0=API成功、それ以外=OS APIの実Win32 error。以降のPowerShell exit codeは従来event2 |

event8は旧PS gateの過去HRESULTではない。PSに対するFILE-choice判定はruntimeでは呼ばない。
既controller f345は未知のeventも同run/PIDへ保存し、既存1..7のgateのみを使うため整合する。

必要ならmainのoffline Windows VMで、診断だけを実行できる。
`inspect-inbox-powershell-trust.ps1`はUTF-8 BOM付き。actual/公式抽出PSのSHAとPEを検証し、
同じFILE-choiceの実HRESULTを採取するが、ARM PSを起動しない。hostのHRESULTを
trial199の実HRESULTへ読み替えず、失敗結果も失敗としてJSONに保持する。

```powershell
& "$src\tools\ci\inspect-inbox-powershell-trust.ps1" -ActualPowerShell $CapturedPs -OfficialPowerShell $OfficialExtractedPs -ReportPath $NewReport
```

## 未確定・次の基点

GPU368 trialが先行するため、次USBの実機baselineは実fresh202を待つ。
旧fresh198 profileのretainedGPU367、旧native SYSTEM、旧ready7ecを新媒体へ流用しない。
このbranchの旧fresh198 build/mediaコマンドは今回の新candidate用ではない。
新しいnative service198 Start4→再arm条件とGPU368保持は、実fresh202を得てからmainが固定する。
本修正では新native、profile、WIM、ESPをfinalizeしていない。

host stub検証は実run()の6経路（exact pin、改変PS拒否、CreateProcess1790保存、
子exit失敗保持、script/profile拒否、CreateProcess失敗+GetLastError0拒否）と、
既terminal7/job8経路を確認。実Windows CreateProcess/PowerShell/PnP/給電は未検証。
