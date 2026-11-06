# Same ARM32 SCM/signing recipe as MT8167DeviceInstall b508de8. No target actions.
param(
 [string]$OutputDirectory,
 [string]$WlanProbePath,
 [string]$AudioClientPath,
 [switch]$Sign,
 [string]$CertificateThumbprint='2655FC89A7082D3BAB3F26EC556EE8A3C52F4358',
 [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateLocation='CurrentUser'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt8167-hardware-probe'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
 throw 'OutputDirectory must be inside this checkout'
}
if([bool]$WlanProbePath -ne [bool]$AudioClientPath){throw 'Provide both fixed clients, or neither for a service-only build'}
$inputs=@()
foreach($item in @(@{path=$WlanProbePath;name='mt6630-wlan-probe.exe'},@{path=$AudioClientPath;name='mt8167-audio-first-playback.exe'})){
 if(!$item.path){continue}
 $full=[IO.Path]::GetFullPath($item.path)
 if(!$full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
  throw 'Client input must be inside this checkout'
 }
 if([IO.Path]::GetFileName($full) -ine $item.name){throw 'Only the exact fixed client filenames may be staged'}
 if(!(Test-Path -LiteralPath $full -PathType Leaf)){throw 'Fixed client input is missing'}
 $inputs+=@{path=$full;name=$item.name}
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$priorTmp=$env:TMP;$priorTemp=$env:TEMP
try {
 $env:TMP=Join-Path $out 'tmp';$env:TEMP=$env:TMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
 $source=Join-Path $root 'tools\winpe\mt8167-hardware-probe.c'
 $object=Join-Path $out 'hardware-probe.obj';$exe=Join-Path $out 'MT8167HardwareProbe.exe'
 & clang --target=thumbv7-pc-windows-msvc -O2 -fno-builtin -fno-stack-protector -w -fms-extensions -ffreestanding `
  -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$inc\um" "-I$inc\shared" -c $source -o $object
 if($LASTEXITCODE){throw 'ARM32 hardware service compilation failed'}
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat `
  $object (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'advapi32.lib') "/out:$exe"
 if($LASTEXITCODE){throw 'ARM32 hardware service link failed'}
 foreach($item in $inputs){
  $destination=Join-Path $out $item.name
  if(![String]::Equals($item.path,$destination,[StringComparison]::OrdinalIgnoreCase)){Copy-Item -LiteralPath $item.path -Destination $destination}
 }
 Copy-Item (Join-Path $root 'tools\winpe\README-mt8167-hardware-probe.md') $out
 $files=@('MT8167HardwareProbe.exe')+@($inputs|ForEach-Object{$_.name})
 foreach($name in $files){
  $header=& llvm-readobj --file-headers --coff-imports (Join-Path $out $name)
  if($LASTEXITCODE -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw "Expected ARM32 PE: $name"}
 }
 if($Sign){
  $cert=Get-Item "Cert:\$CertificateLocation\My\$CertificateThumbprint"
  if(!$cert.HasPrivateKey){throw 'Signing certificate has no private key'}
  $signArgs=@('sign','/fd','SHA256','/sha1',$CertificateThumbprint,'/s','My')
  if($CertificateLocation -eq 'LocalMachine'){$signArgs+='/sm'}
  # Match b508de8: no timestamp server or target/runner operations.
  foreach($name in $files){
   & (Join-Path $kit 'bin\x86\signtool.exe') @signArgs (Join-Path $out $name)
   if($LASTEXITCODE){throw "Hardware probe signing failed: $name"}
  }
 }
 $files+='README-mt8167-hardware-probe.md'
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;
  source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);service='MT8167HardwareProbe';
  target='Windows RT 8.1 ARM32 LocalSystem';service_destination='System32\MT8167HardwareProbe.exe';
  client_directory='System32\MT8167HardwareProbe';scm_arguments='';scm_start=2;scm_delayed_auto_start=$false;
  clients_staged=($inputs.Count -eq 2);signing_requested=[bool]$Sign;
  signing_thumbprint=$(if($Sign){$CertificateThumbprint}else{$null});target_signature_trust_verified=$false;
  marker='attempted.bin';startup='standard auto start; disabled before the only attempt';
  timeouts_ms=@(600000,120000);audio_interface=11;client_arguments=@('','');hardware_verified=$false;
  files=@($files|ForEach-Object{@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $out 'hardware-probe-manifest.json')
 Write-Host "One-shot hardware service package: $out. Signing, staging and SCM registration on target remain primary-owner actions."
} finally {$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
