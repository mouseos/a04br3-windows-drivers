# Keypad initial installation: selected information versus persisted properties

This branch is independent of the frozen run127 source/media. It does not enable
keypad installation in the fresh126 continuation profile. FIRST_INSTALL_MASK stays
zero there; no new hardware experiment or functional success is claimed.

Run125 recorded node7 install error13 before EV_PNP_API_BEGIN, and native fresh126
shows MT6392\Buttons still unselected, ConfigFlags 0x60, no Service/ClassGUID/Driver.
The old trace does not distinguish individual SetupAPI calls. The exact failed API
remains unknown; the persisted ClassGUID check is a supported defect hypothesis.
Do not report it as proven root cause or as successful physical keypad repair.

The install routine now validates the explicit first-install baseline before
building a compatible list, selects exactly one signed/pinned compatible driver
through DIF_SELECTBESTCOMPATDRV, checks its exact version/date/identity and the
SP_DEVINFO_DATA.ClassGuid set by SetupAPI, then uses the standard DIF dispatcher
for coinstallers/interfaces/install. It never creates or edits class registry keys.
The installed-node checks after a planned restart still require exact persisted
class/service/driver selection and actual SYS bytes.

Microsoft's SetupDiSelectBestCompatDrv reference documents class selection; the
actual RT SetupDiSetSelectedDriverW updates SP_DEVINFO_DATA.ClassGuid and internal
information-set state. SetupDiGetDeviceRegistryPropertyW delegates to DevObj for
persisted device properties. A missing preinstall property cannot be used as the
selected-driver class proof. The ARM SetupAPI evidence and raw run125 logs are
stored outside source under the project root, with hashes and inference limits.

PB0x106 now records exact failure/success step before PB0x105's unchanged Win32
result, one pair per attempted target:

| Step | Boundary |
|---|---|
| 1 | explicit unbound first-install baseline |
| 2 | initial GetDeviceInstallParams |
| 3 | initial SetDeviceInstallParams |
| 4 | BuildDriverInfoList |
| 5 | EnumDriverInfo index0 |
| 6 | reject any second candidate / require NO_MORE_ITEMS |
| 7 | pinned version/date |
| 8 | DIF_SELECTBESTCOMPATDRV |
| 9 | GetSelectedDriver |
| 10 | selected driver identity |
| 11 | selected information-set ClassGuid |
| 12 | DIF_REGISTER_COINSTALLERS |
| 13 | DIF_INSTALLINTERFACES |
| 14 | preinstall GetDeviceInstallParams |
| 15 | preinstall SetDeviceInstallParams |
| 16 | existing pre-DIF log boundary |
| 17 | DIF_INSTALLDEVICE |
| 18 | postinstall GetDeviceInstallParams |
| 19 | require DI_NEEDREBOOT |

DI_DONOTCALLCONFIGMG/DI_NEEDREBOOT and planned restart ownership remain unchanged.
No live parent restart or manual input is requested. First-install eligibility is
still an explicitly pinned profile bit; unknown configuration never opts in.

Run `python3 -B tests/peripheral_first_install_test.py --scratch <root-local-dir>`.
It compiles and executes the production function with Win32 boundary stubs and
ASAN/UBSAN. It validates the nonpersisted initial selection, rejects unknown class,
version and duplicate candidates, and injects each API failure. No fake registry
class implementation is supplied, so reintroducing that dependency fails compile.
This is host contract verification, not Windows SetupAPI or hardware execution.

Sources:
https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiselectbestcompatdrv
https://learn.microsoft.com/en-us/windows-hardware/drivers/install/dif-selectbestcompatdrv
https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceregistrypropertyw
