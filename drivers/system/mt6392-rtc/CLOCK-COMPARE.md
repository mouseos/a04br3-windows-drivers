# MT6392 / Windows clock comparison

固定client `mt6392-clock-compare.exe`。引数なし。既存の信頼済みLocalSystem diagnostic launcherからprivate固定logへstdoutをリダイレクトして一回実行する。標準`GetSystemTimeAsFileTime`で既存RTC読取りIOCTLを前後から挟み、`GetTickCount64`で所要時間を記録する。既存\\.\MT6392Rtc、read-only IOCTLと36byte ABIをそのまま使い、親/RTC driver/ownerは変更しない。

前後UTC FILETIME、FileTimeToSystemTimeによるUTC年月日、SystemTimeToTzSpecificLocalTime(NULL)によるOS設定local年月日、実RTC年月日を記録する。localはOSのtimezone設定を使用し、実RTCが同じtimezoneであるとは仮定しない。timezone名/ユーザー名/PMIC RAW/校正値を出力しない。RTC成分をFILETIME暦scaleに符号化して前後UTC/localとの差を100ns単位で記録するが、これは暦成分算術であり実RTCのUTC変換でも同期成功判定でもない。未設定timezoneを捏造しない。

成功rc0は読出し・ABI・API変換・前後時刻順・log書込み成立だけ。OS時計同期/UEFI consumer採用/精度/RTC保持電源の成功ではない。rc1は取得/変換/順序の失敗、rc2はlog書込み失敗。同期中のWindows壁時計の後退も記録して収集失敗にする。SetSystemTime/SetLocalTime/firmware write/privilege調整は行わない。任意device/path/API/command引数は受け付けない。

既存runnerのkit81 header/ARM UM closure/kernel32.libと既存署名鍵を使う `tools/ci/build-mt6392-clock-compare.ps1` で1TU compile/link/signする。rootに転送入力の固定source-pin.txtを置き、clean committed sourceをそのhash付きで転送する。driverやSDK全体を再buildしない。RT8.1実DLLのexportsとのimport照合が別途必須。ログはprivate媒体回収対象で公開repoへ入れない。
