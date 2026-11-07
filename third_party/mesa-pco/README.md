# Mesa PCO/NIR source closure

DriverのARM32 compilerが実際に消費するsource closureだけを保存する。
Mesa source、target中立の生成C/header、既存Windows ARM portを含む。prebuilt object/library、Microsoft header/libraryは含めない。
source-plan.jsonに上流commit、file hash、port overlay、相対compile recipeを記録する。
巨大なnir_opt_algebraic.cも生成済みのC sourceであり、過去のbinary build cacheではない。
個別sourceのcopyright/SPDXとMESA-LICENSE.rstを保持する。upstream-local.patchは取得sourceの既存差分。
Windows/CRT/WDKは別途正規toolchainを使用する。旧machineのabsolute path/reference manifestはbuild入力にしない。
