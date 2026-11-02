@echo off
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto failed
set "BCD=X:\a04-boot.bcd"
set "ID={946e3993-8942-4aec-b737-674d3fe1588e}"
bcdedit /createstore %BCD%
if errorlevel 1 goto failed
bcdedit /store %BCD% /create {bootmgr} /d "A04BR3 boot manager"
if errorlevel 1 goto failed
bcdedit /store %BCD% /set {bootmgr} device boot
if errorlevel 1 goto failed
bcdedit /store %BCD% /create %ID% /d "Windows RT experiment" /application osloader
if errorlevel 1 goto failed
bcdedit /store %BCD% /set %ID% device partition=C:
if errorlevel 1 goto failed
bcdedit /store %BCD% /set %ID% osdevice partition=C:
if errorlevel 1 goto failed
bcdedit /store %BCD% /set %ID% path \Windows\system32\winload.efi
if errorlevel 1 goto failed
bcdedit /store %BCD% /set %ID% systemroot \Windows
if errorlevel 1 goto failed
bcdedit /store %BCD% /set %ID% testsigning on
if errorlevel 1 goto failed
bcdedit /store %BCD% /default %ID%
if errorlevel 1 goto failed
bcdedit /store %BCD% /displayorder %ID%
if errorlevel 1 goto failed
bcdedit /store %BCD% /timeout 5
if errorlevel 1 goto failed
bcdedit /store %BCD% /enum all /v
X:\a04br3\guarded-bcd-export.exe
echo BCD export exit code: %errorlevel%
goto done
:failed
echo BCD creation failed; scratch may need recovery.
:done
echo BCD diagnostic finished.
