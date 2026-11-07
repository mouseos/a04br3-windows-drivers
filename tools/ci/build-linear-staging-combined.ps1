# Incremental KMD/native-only build. No UMD, SDK rebuild or device operations.
param([Parameter(Mandatory=$true)][string]$NativeSource,[Parameter(Mandatory=$true)][string]$NativeSourceCommit,[Parameter(Mandatory=$true)][string]$BaselineKernelLibrary,[Parameter(Mandatory=$true)][string]$BaselineNativeLibrary,[Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$Sign)
$ErrorActionPreference='Stop'
$laRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$laNative=[IO.Path]::GetFullPath($NativeSource)
foreach($laSource in @($laRoot,$laNative)){if(@(& git -C $laSource status --porcelain).Count){throw 'Clean fixed source required'}}
if((& git -C $laNative rev-parse HEAD) -cne $NativeSourceCommit){throw 'Native source commit mismatch'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Sha $BaselineKernelLibrary) -cne '4d3c74382291b01e72ffdbe47f8f781a07a8b9db33eaca54158735e758ff54b9' -or (Sha $BaselineNativeLibrary) -cne '17ed51bfa8b24d75595bb80c2b7b534542de79429283d81b4028ed3d06b78fc9'){throw 'Exact b271 KMD / 4260 native cache required'}
$laOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$laOut.StartsWith($laRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $laOut)){throw 'New checkout-local output required'}
New-Item -ItemType Directory "$laOut\tmp","$laOut\kmd","$laOut\native"|Out-Null
# Native private struct additions are appended. Existing offsets remain stable;
# sizeof users cold/backing/target and all new-field users are rebuilt below.
$laPy=@'
import pathlib,subprocess,sys,json,hashlib,ntpath
lib,out=sys.argv[1:];out=pathlib.Path(out);seen=set();rows=[]
for member in subprocess.check_output(['llvm-ar','t',lib],text=True).splitlines():
 name=ntpath.basename(member)
 if name in seen:raise ValueError('Ambiguous cache archive member')
 seen.add(name);data=subprocess.check_output(['llvm-ar','p',lib,member]);(out/name).write_bytes(data)
 rows.append({'member':member,'name':name,'sha256':hashlib.sha256(data).hexdigest()})
(out/'cache-members.json').write_text(json.dumps(rows,indent=2)+'\n')
'@
$laPy|& python - $BaselineKernelLibrary "$laOut\kmd";if($LASTEXITCODE){throw 'KMD extraction failed'}
$laPy|& python - $BaselineNativeLibrary "$laOut\native";if($LASTEXITCODE){throw 'Native extraction failed'}
$laInc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include";$laResource=(& clang -print-resource-dir)
$laCommon=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-D__ldrexd=__geo_wdk81_ldrexd_declaration',"-I$laResource\include","-I$laInc\km","-I$laInc\km\crt","-I$laInc\shared")
$laOldTemp=$env:TEMP;$laOldTmp=$env:TMP
try{
 $env:TEMP="$laOut\tmp";$env:TMP=$env:TEMP
 $laKmdUnits=@('clear_engine','kmd_clear')
 foreach($laUnit in $laKmdUnits){
  & clang @laCommon -DD3D_UMD_INTERFACE_VERSION=0x4002 -DDXGKDDI_INTERFACE_VERSION=0x4002 -DGEO_LINKED_NATIVE_CORE=1 -c "$laRoot\drivers\gpu\mt8167\wddm-render\$laUnit.c" -o "$laOut\kmd\$laUnit.obj"
  if($LASTEXITCODE){throw "KMD compile failed: $laUnit"}
 }
 $laGpu="$laNative\drivers\gpu\mt8167"
 $laNativeUnits=@('vendor_m1_11\native_owner_factory','vendor_m1_11\native_owner_factory_runtime','vendor_m1_11\native_owner_frontend_resolver','vendor_m1_11\windows_native_cold','vendor_m1_11\windows_native_target','vendor_m1_11\windows_native_backing','wddm-render\clear_protocol')
 # Resolve actual include dependencies under the exact native build defines.
 $laDeclared=Get-Content "$laNative\tools\ci\build-ge8300-native-owner-core.ps1" -Raw
 $laAllNativeUnits=@();foreach($laArray in [regex]::Matches($laDeclared,'(?m)^\s*\$units(?:\+)?=@\(([^)]*)\)')){foreach($laQuoted in [regex]::Matches($laArray.Groups[1].Value,"'([^']+)'")){$laAllNativeUnits+=$laQuoted.Groups[1].Value}}
 if(!$laAllNativeUnits.Count){throw 'Native canonical TU list not found'}
 $laDepRows=@();$laDirect=$laNativeUnits;$laNativeUnits=@()
 $laNativeFlags=@('-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1','-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1','-DA04_GE8300_NATIVE_OWNER_DRAW=1',"-I$laGpu","-I$laGpu\fwif","-I$laGpu\compat","-I$laGpu\vendor_m1_11")
 foreach($laUnit in $laAllNativeUnits){
  $laDeps=(& clang @laCommon @laNativeFlags -M -MT closure "$laGpu\$laUnit.c" 2>$null)|Out-String
  if($LASTEXITCODE){throw "Native dependency scan failed: $laUnit"}
  $laAffected=$laUnit -in $laDirect -or $laDeps -match '(native_owner_factory_private|windows_native_cold_private|windows_native_cold|native_owner_frontend_resolver|clear_protocol)[.]h'
  $laDepRows+=@{unit=$laUnit;affected=[bool]$laAffected;dependencies=$laDeps}
  if($laAffected){$laNativeUnits+=$laUnit}
 }
 $laDepRows|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$laOut\native-dependency-closure.json"
 foreach($laUnit in $laNativeUnits){
  $laName=$laUnit.Replace('\','-')+'.obj'
  if(!(Test-Path "$laOut\native\$laName")){throw "Expected native member absent: $laName"}
  & clang @laCommon -DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1 -DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1 -DA04_GE8300_NATIVE_OWNER_DRAW=1 "-I$laGpu" "-I$laGpu\fwif" "-I$laGpu\compat" "-I$laGpu\vendor_m1_11" -c "$laGpu\$laUnit.c" -o "$laOut\native\$laName"
  if($LASTEXITCODE){throw "Native compile failed: $laUnit"}
 }
 foreach($laKind in @('kmd','native')){
  $laMembers=Get-Content "$laOut\$laKind\cache-members.json" -Raw|ConvertFrom-Json
  $laChanged=if($laKind -eq 'kmd'){@($laKmdUnits|ForEach-Object{$_+'.obj'})}else{@($laNativeUnits|ForEach-Object{$_.Replace('\','-')+'.obj'})}
  foreach($laMember in $laMembers){if($laMember.name -notin $laChanged -and (Sha "$laOut\$laKind\$($laMember.name)") -cne $laMember.sha256){throw 'Unchanged cache object byte changed'}}
  $laObjects=@($laMembers|ForEach-Object{"$laOut\$laKind\$($_.name)"})
  $laLib=if($laKind -eq 'kmd'){'ge8300-pair-kmd.lib'}else{'ge8300-native-owner-core.lib'}
  & llvm-lib /machine:arm "/out:$laOut\$laLib" @laObjects;if($LASTEXITCODE){throw 'Archive assembly failed'}
  if(@(& llvm-ar t "$laOut\$laLib").Count -ne $laMembers.Count){throw 'Archive member count changed'}
 }
 & "$laRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$laOut\ge8300-pair-kmd.lib" -NativeLibrary "$laOut\ge8300-native-owner-core.lib" -OutputFile "$laOut\ge8300-render.sys" -MapFile "$laOut\ge8300-render.map" -MetadataFile "$laOut\linker.json"
 if($LASTEXITCODE){throw 'KMD/native Microsoft LINK failed'}
 $laUnsigned=Sha "$laOut\ge8300-render.sys";$laStatus='Unsigned';$laThumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 if($Sign){$laTool="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe";& $laTool sign /fd SHA256 /sha1 $laThumb "$laOut\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS sign failed'};& $laTool verify /pa "$laOut\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS verify failed'};$laStatus='Valid'}
 [ordered]@{kmd_source_commit=(& git -C $laRoot rev-parse HEAD);native_source_commit=$NativeSourceCommit;kmd_baseline='b27164a1738fde39bae51eb32ebcde314216f210';native_baseline='4260e420272198237aa4c5650a24404a12d36494';kmd_cache_sha256=(Sha $BaselineKernelLibrary);native_cache_sha256=(Sha $BaselineNativeLibrary);kmd_units=$laKmdUnits;native_units=$laNativeUnits;dependency_closure_sha256=(Sha "$laOut\native-dependency-closure.json");unsigned_sys_sha256=$laUnsigned;signature_status=$laStatus;signer_thumbprint=$laThumb;umd_rebuilt=$false;abi_version_changed=$false;native_private_struct_append_only=$true;aperture_bytes=4194304;aperture_commit_limit=4194304;hardware_verified=$false;outputs=@('ge8300-render.sys','ge8300-pair-kmd.lib','ge8300-native-owner-core.lib','linker.json')|ForEach-Object{@{name=$_;sha256=(Sha "$laOut\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$laOut\combined-manifest.json"
 'LINEAR_STAGING_KMD_NATIVE_BUILT'
}finally{$env:TEMP=$laOldTemp;$env:TMP=$laOldTmp}
