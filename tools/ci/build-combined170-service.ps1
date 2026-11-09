# One SCM TU and (separately) the corrected capture client TU; no driver rebuild.
param([Parameter(Mandatory=$true)][ValidateSet('CaptureClient','Service')][string]$Mode,
 [Parameter(Mandatory=$true)][string]$Workspace,[string]$PackageMap,[Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$KitHeaderRoot,[Parameter(Mandatory=$true)][string]$LibraryRoot,[Parameter(Mandatory=$true)][string]$SigntoolPath)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory);$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
if(@(& git -C $root status --porcelain).Count -or (Test-Path $out) -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Clean checkout/new checkout-local output required'}
$inc=[IO.Path]::GetFullPath($KitHeaderRoot);$lib=[IO.Path]::GetFullPath($LibraryRoot);$sign=[IO.Path]::GetFullPath($SigntoolPath)
$cert=Get-Item "Cert:\CurrentUser\My\$thumb";if(!$cert.HasPrivateKey){throw 'Existing BCBF private identity required'}
if($Mode -ne 'Service' -or !$PackageMap){throw 'Combined service-only final map required'}
& python -B "$root\tools\recovery\prepare-peripheral-followup170.py" --workspace $Workspace --map $PackageMap --out $out --finalize
if($LASTEXITCODE){throw 'Final combined contract rejected'}
$name='MT8167CombinedFollowup170';$source="$root\tools\winpe\mt8167-peripheral-update.c"
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out
 & clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_PERIPHERAL_BATCH=1 -DA04_PERIPHERAL_NEXT_PROFILE=1 "-I$inc\um" "-I$inc\shared" "-I$out" -c $source -o "$out\$name.obj"
 if($LASTEXITCODE){throw 'Actual ARM TU compile failed'}
 $libs=@('kernel32','advapi32','setupapi','cfgmgr32','bcrypt','wintrust','crypt32');$pins=@();$paths=@()
 foreach($n in $libs){$p=Join-Path $lib ($n+'.lib');$paths+=$p;$pins+=@{name=$n;sha256=(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}}
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\$name.obj" @paths "/out:$out\$name.exe"
 if($LASTEXITCODE){throw 'ARM EXE link failed'}
 & $sign sign /fd SHA256 /s My /sha1 $thumb "$out\$name.exe";if($LASTEXITCODE){throw 'BCBF signing failed'}
 $sig=Get-AuthenticodeSignature "$out\$name.exe";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signed EXE identity/trust mismatch'}
 $isCapture=if($Mode -eq 'CaptureClient'){'True'}else{'False'}
 & python -B -c "import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from combined170_policy import assert_paths;assert_paths(Path(sys.argv[2]),sys.argv[3]=='True')" "$root\tools\winpe" "$out\$name.exe" $isCapture
 if($LASTEXITCODE){throw 'Signed EXE UTF16 path mismatch'}
 Export-Certificate -Cert $cert -FilePath "$out\development.cer"|Out-Null
 $toolchain=@{headers=$inc;arm_libraries=$lib;import_libraries=$pins;signtool=$sign;signtool_sha256=(Get-FileHash $sign -Algorithm SHA256).Hash.ToLowerInvariant();ntddi='0x06030000';win32_winnt='0x0603'}
 $m=[ordered]@{schema=$(if($Mode -eq 'CaptureClient'){'capture-next122-log-isolation-v1'}else{'combined170-service-v1'});source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();data_directory='MT8167CombinedFollowup170';profile=$null;service='MT8167CombinedFollowup170';signer_thumbprint=$thumb;signature_status='Valid';exe_sha256=(Get-FileHash "$out\$name.exe" -Algorithm SHA256).Hash.ToLowerInvariant();path_header_sha256=(Get-FileHash "$out\peripheral-service-path-policy.h" -Algorithm SHA256).Hash.ToLowerInvariant();runtime_utf16_path_verified=$true;toolchain=$toolchain;driver_rebuilt=$false;device_verified=$false}
 if($Mode -eq 'Service'){
  $policy=Get-Content "$out\peripheral-policy.json" -Raw|ConvertFrom-Json
  if($policy.service -cne $name -or $policy.data_directory -cne $name -or $policy.profile -cne 'combined-features-fresh170-v1'){throw 'Generated policy identity mismatch'}
  $m.profile=$policy.profile
  foreach($p in $policy.packages.PSObject.Properties.Value){$d=Join-Path $Workspace $p.directory;foreach($n in (@($p.names.inf,$p.names.sys)+@($p.extra_members|Where-Object {$_ -and $_.name}|ForEach-Object {$_.name}))){& $sign verify /pa /c (Join-Path $d $p.names.cat) (Join-Path $d $n);if($LASTEXITCODE){throw 'Candidate CAT membership failed'}};$cs=Get-AuthenticodeSignature (Join-Path $d $p.names.cat);if($cs.Status -ne 'Valid' -or $cs.SignerCertificate.Thumbprint -cne $thumb){throw 'Candidate CAT signer mismatch'}}
  foreach($c in $policy.diagnostic_clients){$sigc=Get-AuthenticodeSignature (Join-Path (Join-Path $Workspace $c.directory) $c.exe);if($sigc.Status -ne 'Valid' -or $sigc.SignerCertificate.Thumbprint -cne $thumb){throw 'Pinned client signature mismatch'};foreach($dep in $c.dependencies){$sigc=Get-AuthenticodeSignature (Join-Path (Join-Path $Workspace $c.directory) $dep.exe);if($sigc.Status -ne 'Valid' -or $sigc.SignerCertificate.Thumbprint -cne $thumb){throw 'Pinned client dependency signature mismatch'}}}
  $bt=$policy.bt_first_install;$bd=Join-Path $Workspace $bt.directory
  foreach($member in @('mt6630-combo-bluetooth.inf','mt6630-combo-bluetooth.sys')){& $sign verify /pa /c (Join-Path $bd 'mt6630-combo-bluetooth.cat') (Join-Path $bd $member);if($LASTEXITCODE){throw 'BT2013 membership failed'}}
  $bs=Get-AuthenticodeSignature (Join-Path $bd 'mt6630-combo-bluetooth.cat');if($bs.Status -ne 'Valid' -or $bs.SignerCertificate.Thumbprint -cne $thumb){throw 'BT2013 signer mismatch'}
  $m.observer_contract=$policy.observer_contract;$m.source_TUs=1;$m.wlan_client_rebuilt=$false;$m.policy=$policy;$m.policy_manifest_sha256=(Get-FileHash "$out\peripheral-policy.json" -Algorithm SHA256).Hash.ToLowerInvariant();$m.preservation_plan_sha256=(Get-FileHash "$out\peripheral-preservation-plan.json" -Algorithm SHA256).Hash.ToLowerInvariant()
 }
 $manifest=if($Mode -eq 'CaptureClient'){'capture-client-manifest.json'}else{'peripheral-service-manifest.json'}
 $m|ConvertTo-Json -Depth 30|Set-Content -Encoding UTF8 "$out\$manifest"
 & (Join-Path (Split-Path (Get-Command clang.exe).Source) 'llvm-readobj.exe') --file-headers --coff-imports "$out\$name.exe"|Set-Content "$out\pe-imports.txt";if($LASTEXITCODE){throw 'PE import inspection failed'}
 Write-Output ($Mode+'_SIGNED_PATH_VERIFIED')
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
