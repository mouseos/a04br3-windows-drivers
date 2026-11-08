# Fixed Render-only service. No driver compilation or deployment.
param([switch]$EnableRenderProfile,
 [Parameter(Mandatory=$true)][string]$Workspace,
 [Parameter(Mandatory=$true)][string]$PackageMap,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [switch]$Sign,[string]$CertificateThumbprint,
 [string]$KitHeaderRoot,[string]$LibraryRoot,[string]$SigntoolPath)
$ErrorActionPreference='Stop'
if(!$EnableRenderProfile){throw 'Explicit Render profile opt-in required'}
if($Sign -and $CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$'){throw 'Explicit signing certificate thumbprint required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean immutable source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside source checkout'}
& python -B "$root\tools\winpe\render_single_gpu_policy.py" --enable-render-profile --workspace $Workspace --package-map $PackageMap --output $out
if($LASTEXITCODE){throw 'Fixed package policy rejected'}
New-Item -ItemType Directory "$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
 $inc=if($KitHeaderRoot){[IO.Path]::GetFullPath($KitHeaderRoot)}else{"$kit\Include"}
 $lib=if($LibraryRoot){[IO.Path]::GetFullPath($LibraryRoot)}else{"$kit\Lib\winv6.3\um\arm"}
 $signTool=if($SigntoolPath){[IO.Path]::GetFullPath($SigntoolPath)}else{"$kit\bin\x86\signtool.exe"}
 $libraryNames=@('kernel32','advapi32','setupapi','bcrypt','wintrust','cfgmgr32')
 $libraryFiles=@();$libraryPins=@()
 foreach($name in $libraryNames){$file=Join-Path $lib ($name+'.lib');if(!(Test-Path $file)){throw 'Required ARM import library absent'};$libraryFiles+=$file;$libraryPins+=@{name=$name;path=$file;sha256=(Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()}}
 if(!(Test-Path "$inc\um\windows.h") -or !(Test-Path $signTool)){throw 'Cached headers/signing tool absent'}
 $toolchain=@{header_root=$inc;arm_library_root=$lib;signtool_path=$signTool;signtool_sha256=(Get-FileHash $signTool -Algorithm SHA256).Hash.ToLowerInvariant();import_libraries=$libraryPins;target='thumbv7-pc-windows-msvc';win32_winnt='0x0603';ntddi='0x06030000'}
 $source="$root\tools\winpe\mt8167-render-probe.c"
 $policy=Get-Content -Raw "$out\render-policy-manifest.json"|ConvertFrom-Json
 $ph=[IO.File]::ReadAllText("$out\render-service-path-policy.h")
 if($policy.data_directory -cnotmatch '^MT8167RenderSingleGpuRun[1-9][0-9]*$' -or $policy.engine_preservation.single_gpu_rearm.data_directory -cne $policy.data_directory -or !$ph.Contains('#define A04_RENDER_DATA_DIRECTORY L"'+$policy.data_directory+'"') -or [IO.File]::ReadAllText($source) -cmatch 'L"MT8167RenderSingleGpuRun[0-9]+"'){throw 'Generated/runtime/native data-directory disagreement'}
 & clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_RENDER_PROFILE=1 -DA04_RENDER_SINGLE_GPU=1 -DA04_RENDER_BIND_UPDATE=1 -DA04_HARDWARE_PROBE_ENGINE_PCM=1 -DA04_ENGINE_DEFERRED_UPDATE=1 "-I$inc\um" "-I$inc\shared" "-I$out" -c $source -o "$out\service.obj"
 if($LASTEXITCODE){throw 'Render-only service ARM compilation failed'}
 $policy=Get-Content -Raw "$out\render-policy-manifest.json"|ConvertFrom-Json
 foreach($role in @($policy.packages.PSObject.Properties.Name)){
  $dir=Join-Path $Workspace $policy.packages.$role.directory
  foreach($name in @('ge8300-render.inf','ge8300-render.sys','ge8300-mips.fw','ge8300-umd.dll')){
   & $signTool verify /pa /c "$dir\ge8300-render.cat" "$dir\$name"
   if($LASTEXITCODE){throw 'Fixed 3.23 CAT membership failed'}
  }
 }
 $compareSource=Join-Path (Join-Path $Workspace $policy.comparator.directory) 'compare-files.exe'
 if((Get-FileHash $compareSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $policy.comparator.exe_sha256){throw 'Existing comparator input hash mismatch'}
 $compareSig=Get-AuthenticodeSignature $compareSource
 if($compareSig.Status -ne 'Valid' -or $compareSig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Existing comparator signature mismatch'}
 Copy-Item $compareSource "$out\compare-files.exe"
 $exe="$out\MT8167RenderSingleGpu.exe"
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\service.obj" @libraryFiles "/out:$exe"
 if($LASTEXITCODE){throw 'Render-only service ARM link failed'}
 & python -B -c "import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from render_single_gpu_policy import assert_service_paths,jsonfile;assert_service_paths(Path(sys.argv[2]),jsonfile(Path(sys.argv[3])))" "$root\tools\winpe" $exe "$out\render-policy-manifest.json"
 if($LASTEXITCODE){throw 'Compiled SCM UTF16 path/profile verification failed'}
 if($Sign){
  $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
  if(!$cert.HasPrivateKey -or (Get-Date) -lt $cert.NotBefore -or (Get-Date) -gt $cert.NotAfter){throw 'Valid private signing certificate required'}
  foreach($image in @($exe)){
   & $signTool sign /fd SHA256 /sha1 $CertificateThumbprint /s My $image
   if($LASTEXITCODE){throw 'Render service/comparator signing failed'}
   $check=Get-AuthenticodeSignature $image
   if($check.Status -ne 'Valid' -or $check.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Service/comparator signature rejected'}
  }
  $sig=Get-AuthenticodeSignature $exe
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Final service signature rejected'}
  Export-Certificate -Cert $cert -FilePath "$out\mt8167-development.cer"|Out-Null
 }
 & python -B -c "import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from render_single_gpu_policy import assert_service_paths,jsonfile;assert_service_paths(Path(sys.argv[2]),jsonfile(Path(sys.argv[3])))" "$root\tools\winpe" $exe "$out\render-policy-manifest.json"
 if($LASTEXITCODE){throw 'Final signed SCM UTF16 path/profile verification failed'}
 & llvm-readobj --file-headers --coff-imports $exe | Set-Content "$out\pe-imports.txt"
 if($LASTEXITCODE){throw 'Service PE inspection failed'}
 $policy=Get-Content -Raw "$out\render-policy-manifest.json"|ConvertFrom-Json
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;toolchain=$toolchain;data_directory=$policy.data_directory;path_header_sha256=(Get-FileHash "$out\render-service-path-policy.h" -Algorithm SHA256).Hash.ToLowerInvariant();runtime_utf16_path_verified=$true;profile=$policy.profile;service='MT8167RenderSingleGpu';signed=[bool]$Sign;signing_thumbprint=$(if($Sign){$sig.SignerCertificate.Thumbprint}else{$null});signature_status=$(if($Sign){[string]$sig.Status}else{'Unsigned'});exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();policy_manifest_sha256=(Get-FileHash "$out\render-policy-manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant();policy=$policy;comparator=@{exe='compare-files.exe';exe_sha256=(Get-FileHash "$out\compare-files.exe" -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=(Get-FileHash "$root\tools\winpe\compare-files.c" -Algorithm SHA256).Hash.ToLowerInvariant();signature_status=$(if($Sign){[string](Get-AuthenticodeSignature "$out\compare-files.exe").Status}else{'Unsigned'});signer_thumbprint=$(if($Sign){(Get-AuthenticodeSignature "$out\compare-files.exe").SignerCertificate.Thumbprint}else{$null});comparison='full bytes through exact EOF; no C runtime or external tool'};normal_install_allowed=$true;device_verified=$false}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\render-service-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
