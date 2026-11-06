# Same ARM32 SCM/signing recipe as MT8167DeviceInstall b508de8. No target actions.
param(
 [string]$OutputDirectory,
 [ValidateSet('Tone','EnginePcm')][string]$AudioMode='Tone',
 [string]$WlanProbePath,
 [string]$AudioClientPath,
 [switch]$Sign,
 [string]$CertificateThumbprint='2655FC89A7082D3BAB3F26EC556EE8A3C52F4358',
 [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateLocation='CurrentUser'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$enginePcm=$AudioMode -eq 'EnginePcm'
$engineDefine=if($enginePcm){'1'}else{'0'}
$modeName=if($enginePcm){'engine_pcm_retained'}else{'private_fixed_tone_diagnostic'}
$serviceName=if($enginePcm){'MT8167EngineProbe'}else{'MT8167HardwareProbe'}
$audioName=if($enginePcm){'mt8167-audio-engine-probe.exe'}else{'mt8167-audio-first-playback.exe'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root $(if($enginePcm){'build\mt8167-engine-probe'}else{'build\mt8167-hardware-probe'})}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
 throw 'OutputDirectory must be inside this checkout'
}
if([bool]$WlanProbePath -ne [bool]$AudioClientPath){throw 'Provide both fixed clients, or neither for a service-only build'}
$priorManifest=Join-Path $out 'hardware-probe-manifest.json'
if(Test-Path $priorManifest){
 $previous=Get-Content $priorManifest -Raw|ConvertFrom-Json
 if($previous.audio_mode -and $previous.audio_mode -ne $modeName){throw 'Use separate output directories for fixed service modes'}
 if(!$previous.audio_mode -and $enginePcm){throw 'Legacy output cannot be reused for EnginePcm'}
}
if($enginePcm -and $Sign -and !$PSBoundParameters.ContainsKey('CertificateThumbprint')){
 throw 'EngineProbe signing requires an explicit CertificateThumbprint from your development certificate store'
}
$signingThumbprint=$null
if($Sign){
 $requestedThumbprint=([string]$CertificateThumbprint).Trim().ToUpperInvariant()
 if($requestedThumbprint -notmatch '^[0-9A-F]{40}$'){throw 'CertificateThumbprint must be an explicit 40-digit SHA1 thumbprint'}
 $cert=Get-Item -LiteralPath "Cert:\$CertificateLocation\My\$requestedThumbprint"
 if(!$cert.HasPrivateKey){throw 'Signing certificate has no private key'}
 $nowUtc=[DateTime]::UtcNow
 if($nowUtc -lt $cert.NotBefore.ToUniversalTime() -or $nowUtc -gt $cert.NotAfter.ToUniversalTime()){
  throw 'Signing certificate is outside its validity period'
 }
 $signingThumbprint=$cert.Thumbprint.ToUpperInvariant()
 if($signingThumbprint -ne $requestedThumbprint){throw 'Resolved signing certificate thumbprint differs from the request'}
}
$inputs=@()
foreach($item in @(@{path=$WlanProbePath;name='mt6630-wlan-probe.exe'},@{path=$AudioClientPath;name=$audioName})){
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
 $object=Join-Path $out 'hardware-probe.obj';$exe=Join-Path $out ($serviceName+'.exe')
 & clang --target=thumbv7-pc-windows-msvc -O2 -fno-builtin -fno-stack-protector -w -fms-extensions -ffreestanding `
  "-DA04_HARDWARE_PROBE_ENGINE_PCM=$engineDefine" -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$inc\um" "-I$inc\shared" -c $source -o $object
 if($LASTEXITCODE){throw 'ARM32 hardware service compilation failed'}
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat `
  $object (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'advapi32.lib') "/out:$exe"
 if($LASTEXITCODE){throw 'ARM32 hardware service link failed'}
 foreach($item in $inputs){
  $destination=Join-Path $out $item.name
  if(![String]::Equals($item.path,$destination,[StringComparison]::OrdinalIgnoreCase)){Copy-Item -LiteralPath $item.path -Destination $destination}
 }
 Copy-Item (Join-Path $root 'tools\winpe\README-mt8167-hardware-probe.md') $out
 $files=@($serviceName+'.exe')+@($inputs|ForEach-Object{$_.name})
 foreach($name in $files){
  $header=& llvm-readobj --file-headers --coff-imports (Join-Path $out $name)
  if($LASTEXITCODE -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw "Expected ARM32 PE: $name"}
 }
 if($Sign){
  $signArgs=@('sign','/fd','SHA256','/sha1',$signingThumbprint,'/s','My')
  if($CertificateLocation -eq 'LocalMachine'){$signArgs+='/sm'}
  # Match b508de8: no timestamp server or target/runner operations.
  $signFiles=if($enginePcm){@($serviceName+'.exe')}else{$files}
  foreach($name in $signFiles){
   & (Join-Path $kit 'bin\x86\signtool.exe') @signArgs (Join-Path $out $name)
   if($LASTEXITCODE){throw "Hardware probe signing failed: $name"}
  }
 }
 $files+='README-mt8167-hardware-probe.md'
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;
  source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);service=$serviceName;
  target='Windows RT 8.1 ARM32 LocalSystem';service_destination=('System32\'+$serviceName+'.exe');
  client_directory=$(if($enginePcm){'System32\MT8167EngineProbe\clients'}else{'System32\MT8167HardwareProbe'});
  log_directory=$(if($enginePcm){'System32\MT8167EngineProbe\logs'}else{'System32\MT8167HardwareProbe'});scm_arguments='';scm_start=2;scm_delayed_auto_start=$false;
  clients_staged=($inputs.Count -eq 2);signing_requested=[bool]$Sign;
  signing_thumbprint=$signingThumbprint;target_signature_trust_verified=$false;
  audio_mode=$modeName;engine_pcm_compatible=[bool]$enginePcm;engine_pcm_auto_registration_allowed=[bool]$enginePcm;
  fixed_clients=@('mt6630-wlan-probe.exe',$audioName);client_signatures_preserved=[bool]$enginePcm;
  build_defines=@{A04_HARDWARE_PROBE_ENGINE_PCM=$engineDefine};
  pnp_rebind=$(if($enginePcm){@{api='UpdateDriverForPlugAndPlayDevicesW';flags=5;
   fixed_hardware_ids=@('MT8167\AFE','MT8167\USB20','A04BR3\MT6630SDIO');
   directory='System32\MT8167EngineProbe\drivers';maximum_calls_per_id=1;
   parent_updated=$false;reboot_required_stops_clients=$true;catalog_signature_required=$true;
   status_observation='before/after CM status, problem, query errors, match count; no retries'}}else{$null});
  power_request='PowerRequestSystemRequired; scoped to the single attempt; clear/close on exit';
  explicit_user_sleep_overridden=$false;power_policy_changed=$false;
  marker='attempted.bin';marker_directory=('System32\'+$serviceName);startup='standard auto start; disabled before the only attempt';
  timeouts_ms=@(600000,120000);audio_interface=11;client_arguments=@('','');hardware_verified=$false;
  files=@($files|ForEach-Object{@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $out 'hardware-probe-manifest.json')
 Write-Host "One-shot hardware service package: $out. Signing, staging and SCM registration on target remain primary-owner actions."
} finally {$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
