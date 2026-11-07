# Incremental 3.17: only KMD context/render/patch contract. Frozen native/UMD.
param([Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$dpRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $dpRoot status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Input pin mismatch: $p"}}
Pin "$BaselinePackageDirectory\signed-manifest.json" 'cc666fb4b525de4f0d0fd51ee708ca4a08af1815ea2e47294a6b5b45d3a4df24'
Pin "$BaselineBuildDirectory\combined-manifest.json" '64e5a57e3e3ce12ad086b3990608fe906ac796e1ce8c7a75333a6df7eb0e83d3'
Pin "$BaselineBuildDirectory\ge8300-pair-kmd.lib" '57998c3e4fe31052e6c3630d4370ba867c6f3014f4ab4044e3813ef6e49acbe9'
Pin "$BaselineBuildDirectory\ge8300-native-owner-core.lib" '3b2f10bb422e9f913115246a34093b79016383d19d8d3dba3f3c30c5936eeec8'
$dpBase=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
foreach($dpE in $dpBase.outputs){Pin (Join-Path $BaselinePackageDirectory $dpE.name) $dpE.sha256}
$dpOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!(($dpOut.StartsWith($dpRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))) -or (Test-Path $dpOut)){throw 'New checkout-local output required'}
New-Item -ItemType Directory "$dpOut\kmd","$dpOut\tmp"|Out-Null
$dpExtract=@'
import pathlib,sys,subprocess,ntpath,hashlib,json
src,out=sys.argv[1:];out=pathlib.Path(out);rows=[];seen=set()
for member in subprocess.check_output(['llvm-ar','t',src],text=True).splitlines():
 name=ntpath.basename(member)
 if name in seen:raise ValueError('Duplicate archive member')
 seen.add(name);data=subprocess.check_output(['llvm-ar','p',src,member]);(out/name).write_bytes(data)
 rows.append({'member':member,'name':name,'sha256':hashlib.sha256(data).hexdigest()})
(out/'cache-members.json').write_text(json.dumps(rows,indent=2)+'\n')
'@
$dpExtract|& python - "$BaselineBuildDirectory\ge8300-pair-kmd.lib" "$dpOut\kmd";if($LASTEXITCODE){throw 'KMD extract failed'}
$dpInc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include";$dpResource=(& clang -print-resource-dir)
$dpFlags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-D__ldrexd=__geo_wdk81_ldrexd_declaration',"-I$dpResource\include","-I$dpInc\km","-I$dpInc\km\crt","-I$dpInc\shared",'-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002','-DGEO_LINKED_NATIVE_CORE=1')
$dpUnits=@('kmd_objects','kmd_clear');$dpOldTemp=$env:TEMP;$dpOldTmp=$env:TMP
try{
 $env:TEMP="$dpOut\tmp";$env:TMP=$env:TEMP
 foreach($dpUnit in $dpUnits){
  if(!(Test-Path "$dpOut\kmd\$dpUnit.obj")){throw 'Expected TU member absent'}
  & clang @dpFlags -c "$dpRoot\drivers\gpu\mt8167\wddm-render\$dpUnit.c" -o "$dpOut\kmd\$dpUnit.obj"
  if($LASTEXITCODE){throw "Compile failed: $dpUnit"}
 }
 $dpMembers=Get-Content "$dpOut\kmd\cache-members.json" -Raw|ConvertFrom-Json
 foreach($dpMember in $dpMembers){if($dpMember.name -notin @('kmd_objects.obj','kmd_clear.obj') -and (Sha "$dpOut\kmd\$($dpMember.name)") -cne $dpMember.sha256){throw 'Unchanged member changed'}}
 $dpObjects=@($dpMembers|ForEach-Object{"$dpOut\kmd\$($_.name)"})
 & llvm-lib /machine:arm "/out:$dpOut\ge8300-pair-kmd.lib" @dpObjects;if($LASTEXITCODE){throw 'KMD archive failed'}
 if(@(& llvm-ar t "$dpOut\ge8300-pair-kmd.lib").Count -ne $dpMembers.Count){throw 'Archive count changed'}
 Copy-Item "$BaselineBuildDirectory\ge8300-native-owner-core.lib" $dpOut
 Pin "$dpOut\ge8300-native-owner-core.lib" '3b2f10bb422e9f913115246a34093b79016383d19d8d3dba3f3c30c5936eeec8'
 & "$dpRoot\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$dpOut\ge8300-pair-kmd.lib" -NativeLibrary "$dpOut\ge8300-native-owner-core.lib" -OutputFile "$dpOut\ge8300-render.sys" -MapFile "$dpOut\ge8300-render.map" -MetadataFile "$dpOut\linker.json"
 if($LASTEXITCODE){throw 'Microsoft LINK failed'}
 $dpUnsigned=Sha "$dpOut\ge8300-render.sys";$dpThumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$dpTool="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
 & $dpTool sign /fd SHA256 /sha1 $dpThumb "$dpOut\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS sign failed'}
 & $dpTool verify /pa "$dpOut\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS verify failed'}
 [ordered]@{schema='dma-pool317-kmd-incremental-v1';kmd_source_commit=(& git -C $dpRoot rev-parse HEAD);native_source_commit='fb2173597de9acc12af8f06fd771037e02093360';baseline_package_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");kmd_units=$dpUnits;cached_member_count=$dpMembers.Count;unchanged_members_verified=$true;native_rebuilt=$false;umd_rebuilt=$false;dma_buffer_size_changed=$false;allocation_list_capacity=3;patch_location_capacity=2;private_pointer_input_preserved=$true;signature_status='Valid';signer_thumbprint=$dpThumb;unsigned_sys_sha256=$dpUnsigned;hardware_verified=$false;outputs=@('ge8300-render.sys','ge8300-pair-kmd.lib','ge8300-native-owner-core.lib','linker.json')|ForEach-Object{@{name=$_;sha256=(Sha "$dpOut\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$dpOut\combined-manifest.json"
 'DMA_POOL317_KMD_SIGNED'
}finally{$env:TEMP=$dpOldTemp;$env:TMP=$dpOldTmp}
