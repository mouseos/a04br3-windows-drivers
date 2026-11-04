@echo off
title MT8167 GE8300 firmware test
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
set GE8300_PACKAGE=X:\a04br3\ge8300
rem ESPをWindowsから参照する前に、既存WinPEのMSDCドライバを開始する。
if not exist X:\a04br3\a04-msdc-probe.inf goto load_driver
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto load_driver
rem 試験用ESPにパッケージがある場合は小さなドライバ更新だけで再試験する。
mountvol V: /S
if not errorlevel 1 goto check_external
rem WinPEのRAM起動では /S がESPを特定できない場合がある。
rem ボリュームGUIDの実ファイルを確認してからドライブ文字を割り当てる。
for /f "tokens=*" %%G in ('mountvol') do if exist "%%Ga04-ge8300\mt8167-ge8300.inf" mountvol V: "%%G"
:check_external
if not exist V:\a04-ge8300\mt8167-ge8300.inf goto load_driver
if not exist V:\a04-ge8300\mt8167-ge8300.sys goto packagefail
if not exist V:\a04-ge8300\mt8167-ge8300.cat goto packagefail
if not exist V:\a04-ge8300\ge8300-mips.fw goto packagefail
if not exist V:\a04-ge8300\ge8300-status.exe goto packagefail
if not exist V:\a04-ge8300\ge8300-pnp-status.exe goto packagefail
copy /Y V:\a04-ge8300\ge8300-mips.fw X:\Windows\System32\drivers\ge8300-mips.fw >nul
if errorlevel 1 goto packagefail
set GE8300_PACKAGE=V:\a04-ge8300
:load_driver
echo GE8300_PACKAGE=%GE8300_PACKAGE%
drvload %GE8300_PACKAGE%\mt8167-ge8300.inf
echo GE8300_DRVLOAD_EXIT=%errorlevel%
%GE8300_PACKAGE%\ge8300-pnp-status.exe
echo GE8300_PNP_EXIT=%errorlevel%
%GE8300_PACKAGE%\ge8300-status.exe
echo GE8300_STATUS_EXIT=%errorlevel%
echo GE8300_TEST_FINISHED
goto end
:consolefail
echo GE8300_CONSOLE_FAILED
goto end
:packagefail
echo GE8300_PACKAGE_FAILED
:end
