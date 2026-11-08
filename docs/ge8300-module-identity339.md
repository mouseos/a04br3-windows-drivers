# GPU module identity339（根本修正ではない）

338を基点にminiport.c1TUだけへDriverEntry PASSIVE時のlocal registry recordを追加。DriverObject実DriverStart/DriverSize、固定source base3aaaef10、build ID GPU338-module-identity-v1、load時FILETIMEとKeQueryInterruptTimeを144B REG_BINARYへ保存する。Parameters/NativeCheckpoint/ModuleIdentityV1。レジストリ保存失敗は初期化戻り値を変えない。IRQ/DPC/Render/copy/power/CPU数/timeout/cache同期は変更しない。

同source baseは変更の親commitを示し、自己参照hashではない。正確なcandidate source/署名SYS SHAはpackage manifestで結合する。FILETIME−uptimeはboot時刻の推定でありboot GUIDではない。同起動collector/実image hashと結合するまでPC関数照合を許可しない。次bootのASLRbaseを旧143lastpcへ当てることは禁止。

readerは`tools/ci/read-ge8300-module-identity.py --binary ... --out <newJSON>`。native readerはSYSTEMcloneのServices/GE8300Render/Parameters/NativeCheckpoint/ModuleIdentityV1をBinary raw144Bで出すだけでよい。起動/initだけで記録でき、GPUclientを実行する必要はない。専用probe bootは要求せず、mainの次通常audio/thermal等機能batchに任意結合する。

実関数のregistry/DriverObject mock9case＋decoder4case、ASan/UBSan。実ARM1TUcompile/link/sign/INF3.39/CATを既存cacheで行う。RT8.1 import集合を338とexact比較し、不一致なら候補を止める。UMD/PCO/native67/FW/client保持。moduleidentity証拠追加でありresetrootfix未証明。
