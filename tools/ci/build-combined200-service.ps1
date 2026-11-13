# One SCM TU and (separately) the corrected capture client TU; no driver rebuild.
param([switch]$SignedRenderUpdate,[switch]$SignedPeripherals,[switch]$CollectionReboot,[int]$CaptureNumber=200,[switch]$Gpu369Timing,[switch]$Gpu371Output,[switch]$Gpu375Combined,[switch]$Gpu377Output,[switch]$Gpu378Keypad,[switch]$Gpu378Diagnostic,[switch]$Followup231,[Alias('Gpu374Client','Gpu375Client')][switch]$Gpu373Client,[Parameter(Mandatory=$true)][ValidateSet('CaptureClient','Service')][string]$Mode,
 [Parameter(Mandatory=$true)][string]$Workspace,[string]$PackageMap,[Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$KitHeaderRoot,[Parameter(Mandatory=$true)][string]$LibraryRoot,[Parameter(Mandatory=$true)][string]$SigntoolPath)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if($Gpu378Diagnostic){if($Gpu378Keypad){throw 'Select diagnostic or update, not both'};$Gpu378Keypad=$true}
$gpu378Generator=if($Gpu378Diagnostic){'prepare-gpu378-diagnostic229-inputs.py'}else{'prepare-gpu378-keypad13-inputs.py'}
if(@($Gpu369Timing,$Gpu371Output,$Gpu373Client,$Gpu375Combined,$Gpu377Output,$Gpu378Keypad,$Followup231,$SignedPeripherals,$SignedRenderUpdate|Where-Object {$_}).Count -gt 1){throw 'Select one GPU variant'}
if($SignedRenderUpdate){if($CaptureNumber -lt 235 -or $CaptureNumber -gt 999 -or ($CollectionReboot -and $CaptureNumber -lt 237)){throw 'Signed render requires actual235..999; unattended collection needs actual237+'}}elseif($SignedPeripherals){if($CaptureNumber -lt 204 -or $CaptureNumber -gt 999 -or $CollectionReboot){throw 'Signed peripherals requires actual204..999 and terminal hold'}}elseif($Followup231){if($CaptureNumber -ne 231){throw 'Followup231 requires actual231'}}elseif($Gpu378Keypad){if($CaptureNumber -ne $(if($Gpu378Diagnostic){229}else{227})){throw 'GPU378 requires actual fresh227'}}elseif($Gpu377Output){if($CaptureNumber -ne 225){throw 'GPU377 requires actual fresh225'}}elseif($Gpu375Combined){if($CaptureNumber -ne 221){throw 'Combined375 requires actual221'}}elseif($Gpu373Client){if($CaptureNumber -notin @(215,218,221)){throw 'Known client capture215/218 required'}}elseif($Gpu371Output){if($CaptureNumber -ne 212){throw 'GPU371 requires actual212'}}elseif($Gpu369Timing){if($CaptureNumber -lt 204 -or $CaptureNumber -gt 999){throw 'Actual timing capture204..999 required'}}elseif($CaptureNumber -notin @(200,202)){throw 'Legacy200/202 only without Gpu369Timing'}
$clientVersion=if($CaptureNumber -eq 221){375}elseif($CaptureNumber -eq 218){374}else{373}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory);$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
if(@(& git -C $root status --porcelain).Count -or (Test-Path $out) -or !$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Clean checkout/new checkout-local output required'}
$inc=[IO.Path]::GetFullPath($KitHeaderRoot);$lib=[IO.Path]::GetFullPath($LibraryRoot);$sign=[IO.Path]::GetFullPath($SigntoolPath)
$cert=Get-Item "Cert:\CurrentUser\My\$thumb";if(!$cert.HasPrivateKey){throw 'Existing BCBF private identity required'}
if($Mode -ne 'Service' -or !$PackageMap){throw 'Combined service-only final map required'}
if($SignedRenderUpdate){
 & python -B "$root\tools\recovery\render-signed-capture.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($SignedPeripherals){
 & python -B "$root\tools\recovery\peripheral-signed-updates.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Followup231){
 & python -B "$root\tools\recovery\prepare-followup231-inputs.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Gpu378Keypad){
 & python -B "$root\tools\recovery\$gpu378Generator" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Gpu377Output){
 & python -B "$root\tools\recovery\prepare-gpu377-fresh225-inputs.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Gpu375Combined){
 & python -B "$root\tools\recovery\prepare-gpu375-combined-inputs.py" --workspace $Workspace --inputs $PackageMap --out $out --combined-update --finalize
}elseif($Gpu373Client){
 & python -B "$root\tools\recovery\prepare-gpu$clientVersion-client-inputs.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Gpu371Output){
 & python -B "$root\tools\recovery\prepare-gpu371-output-inputs.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}elseif($Gpu369Timing){
 & python -B "$root\tools\recovery\prepare-gpu369-timing-inputs.py" --workspace $Workspace --inputs $PackageMap --out $out --finalize
}else{
 & python -B "$root\tools\recovery\prepare-peripheral-followup$CaptureNumber.py" --workspace $Workspace --map $PackageMap --out $out --finalize
}
if($LASTEXITCODE){throw 'Final combined contract rejected'}
$name="MT8167CombinedFollowup$CaptureNumber";$source="$root\tools\winpe\mt8167-peripheral-update.c"
$policy=Get-Content "$out\peripheral-policy.json" -Raw|ConvertFrom-Json
$expectedProfile=if($SignedRenderUpdate){"combined-features-fresh$CaptureNumber-signed-render-v1"}elseif($SignedPeripherals){"combined-features-fresh$CaptureNumber-signed-peripherals-v1"}elseif($Followup231){"combined-features-fresh231-reviewed-followup-v1"}elseif($Gpu378Diagnostic){"combined-features-fresh229-gpu378-keypad13-diagnostic-only-v1"}elseif($Gpu378Keypad){"combined-features-fresh227-gpu378-keypad13-client375-v1"}elseif($Gpu377Output){"combined-features-fresh225-gpu377-output-client375-v1"}elseif($Gpu375Combined){"combined-features-fresh221-gpu376-battery18-wifi1031-client375-v1"}elseif($Gpu373Client){"combined-features-fresh$CaptureNumber-gpu$clientVersion-clients-v1"}elseif($Gpu371Output){"combined-features-fresh212-gpu371-output-v1"}elseif($Gpu369Timing){"combined-features-fresh$CaptureNumber-gpu369-timing-v1"}else{"combined-features-fresh$CaptureNumber-gpu368-v1"}
if($CollectionReboot -and !$SignedRenderUpdate){$expectedProfile+='-collection-reboot-v1'}
if($policy.service -cne $name -or $policy.data_directory -cne $name -or $policy.profile -cne $expectedProfile){throw 'Generated policy identity mismatch'}
$collectionArgs=@();if($CollectionReboot){$collectionArgs=@('--explicit')}
$collectionJson=& python -B "$root\tools\recovery\collection_reboot.py" --policy "$out\peripheral-policy.json" --repo $root @collectionArgs
if($LASTEXITCODE){throw 'Explicit collection profile/recipe mismatch before compile'}
$collectionClosure=$collectionJson|ConvertFrom-Json
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP=$out;$env:TEMP=$out
 $developmentFlags=@();$displayHeader=$null
 $filterRootHeader="$root\tools\winpe\filter-root-transition.h"
 $filterRootHeaderHash=(Get-FileHash $filterRootHeader -Algorithm SHA256).Hash.ToLowerInvariant()
 if($Gpu377Output -or $Gpu378Keypad -or $Followup231 -or $SignedRenderUpdate){
  $developmentFlags=@('-DA04_DEVELOPMENT_DISPLAY_ON=1')
  $displayHeader="$root\tools\winpe\display-idle-policy.h"
  if(!(Test-Path $displayHeader)){throw 'Actual power helper header required'}
  $displayHeaderHash=(Get-FileHash $displayHeader -Algorithm SHA256).Hash.ToLowerInvariant()
  if($Gpu378Keypad -or $Followup231 -or $SignedRenderUpdate){$keypadHeader="$root\tools\winpe\keypad13-checkpoint-policy.h";$keypadHeaderHash=(Get-FileHash $keypadHeader -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 if($SignedPeripherals){$keypadHeader="$root\tools\winpe\keypad13-checkpoint-policy.h";$keypadHeaderHash=(Get-FileHash $keypadHeader -Algorithm SHA256).Hash.ToLowerInvariant()}
 if($Followup231 -or $SignedRenderUpdate){$vidpnHeader="$root\tools\winpe\vidpn-callback-trace-policy.h";$vidpnHeaderHash=(Get-FileHash $vidpnHeader -Algorithm SHA256).Hash.ToLowerInvariant()}
 & clang @developmentFlags --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DA04_PERIPHERAL_BATCH=1 -DA04_PERIPHERAL_NEXT_PROFILE=1 "-I$inc\um" "-I$inc\shared" "-I$out" -c $source -o "$out\$name.obj"
 if($LASTEXITCODE){throw 'Actual ARM TU compile failed'}
 if($collectionClosure){foreach($r in $collectionClosure.headers){if((Get-FileHash (Join-Path $root $r.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $r.sha256){throw 'Collection source changed during compile'}}}
 $libs=@('kernel32','advapi32','setupapi','cfgmgr32','bcrypt','wintrust','crypt32');$pins=@();$paths=@()
 foreach($n in $libs){$p=Join-Path $lib ($n+'.lib');$paths+=$p;$pins+=@{name=$n;sha256=(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}}
 & lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat "$out\$name.obj" @paths "/out:$out\$name.exe"
 if($LASTEXITCODE){throw 'ARM EXE link failed'}
 & $sign sign /fd SHA256 /s My /sha1 $thumb "$out\$name.exe";if($LASTEXITCODE){throw 'BCBF signing failed'}
 $sig=Get-AuthenticodeSignature "$out\$name.exe";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signed EXE identity/trust mismatch'}
 $isCapture=if($Mode -eq 'CaptureClient'){'True'}else{'False'}
 if($SignedRenderUpdate){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('render_capture',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\render-signed-capture.py" "$out\$name.exe"
 }elseif($SignedPeripherals){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('peripherals',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\peripheral-signed-updates.py" "$out\$name.exe"
 }elseif($Followup231){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('followup231',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-followup231-inputs.py" "$out\$name.exe"
 }elseif($Gpu378Keypad){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('output378',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\$gpu378Generator" "$out\$name.exe"
 }elseif($Gpu377Output){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('output377',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-gpu377-fresh225-inputs.py" "$out\$name.exe"
 }elseif($Gpu375Combined){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('combined375',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-gpu375-combined-inputs.py" "$out\$name.exe"
 }elseif($Gpu373Client){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('client373',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-gpu$clientVersion-client-inputs.py" "$out\$name.exe"
 }elseif($Gpu371Output){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('output371',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.configure(m.MANIFEST);m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-gpu371-output-inputs.py" "$out\$name.exe"
 }elseif($Gpu369Timing){
  & python -B -c "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('timing',Path(sys.argv[1]));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.configure_capture(int(sys.argv[3]));m.assert_paths(Path(sys.argv[2]))" "$root\tools\recovery\prepare-gpu369-timing-inputs.py" "$out\$name.exe" $CaptureNumber
 }else{
 & python -B -c "import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from combined$($CaptureNumber)_policy import assert_paths;assert_paths(Path(sys.argv[2]),sys.argv[3]=='True')" "$root\tools\winpe" "$out\$name.exe" $isCapture
 }
 if($LASTEXITCODE){throw 'Signed EXE UTF16 path mismatch'}
 Export-Certificate -Cert $cert -FilePath "$out\development.cer"|Out-Null
 $toolchain=@{headers=$inc;arm_libraries=$lib;import_libraries=$pins;signtool=$sign;signtool_sha256=(Get-FileHash $sign -Algorithm SHA256).Hash.ToLowerInvariant();ntddi='0x06030000';win32_winnt='0x0603'}
 $m=[ordered]@{schema=$(if($Mode -eq 'CaptureClient'){'capture-next122-log-isolation-v1'}else{"combined$CaptureNumber-unified356-service-v1"});source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();data_directory=$name;profile=$null;service=$name;signer_thumbprint=$thumb;signature_status='Valid';exe_sha256=(Get-FileHash "$out\$name.exe" -Algorithm SHA256).Hash.ToLowerInvariant();path_header_sha256=(Get-FileHash "$out\peripheral-service-path-policy.h" -Algorithm SHA256).Hash.ToLowerInvariant();runtime_utf16_path_verified=$true;toolchain=$toolchain;driver_rebuilt=$false;device_verified=$false}
 if((Get-FileHash $filterRootHeader -Algorithm SHA256).Hash.ToLowerInvariant() -cne $filterRootHeaderHash){throw 'Filter root header changed during build'}
 if($collectionClosure){$m.collection_reboot=$collectionClosure}
 $m.filter_root_header=@{path='tools/winpe/filter-root-transition.h';sha256=$filterRootHeaderHash;enabled=($null -ne $policy.filter_root_transitions -and @($policy.filter_root_transitions).Count -gt 0)}
 if($Gpu377Output -or $Gpu378Keypad -or $Followup231 -or $SignedRenderUpdate){
  if((Get-FileHash $displayHeader -Algorithm SHA256).Hash.ToLowerInvariant() -cne $displayHeaderHash){throw 'Power header changed during build'}
  $m.development_display_on=1
  $m.compile_closure=@{source=@{path='tools/winpe/mt8167-peripheral-update.c';sha256=$m.source_sha256};display_idle_policy=@{path='tools/winpe/display-idle-policy.h';sha256=$displayHeaderHash};macro='A04_DEVELOPMENT_DISPLAY_ON=1';target_execution_verified=$false}
  if($Gpu378Keypad -or $Followup231 -or $SignedRenderUpdate){
   if((Get-FileHash $keypadHeader -Algorithm SHA256).Hash.ToLowerInvariant() -cne $keypadHeaderHash){throw 'Keypad checkpoint header changed during build'}
   $m.compile_closure.keypad_checkpoint=@{path='tools/winpe/keypad13-checkpoint-policy.h';sha256=$keypadHeaderHash}
   $m.compile_closure.profile_macros=@('A04_KEYPAD13_CHECKPOINT=1','A04_DISPLAY_IDLE_BEFORE_CLIENTS=1')
  }
 }
 if($SignedPeripherals){
  if((Get-FileHash $keypadHeader -Algorithm SHA256).Hash.ToLowerInvariant() -cne $keypadHeaderHash){throw 'Keypad header changed during compile'}
  $m.compile_closure=@{source=@{path='tools/winpe/mt8167-peripheral-update.c';sha256=$m.source_sha256};keypad_checkpoint=@{path='tools/winpe/keypad13-checkpoint-policy.h';sha256=$keypadHeaderHash};profile_macros=@('A04_KEYPAD13_CHECKPOINT=1','A04_KEYPAD14_CHECKPOINT=1');GPU_diagnostics=0;target_execution_verified=$false}
 }
 if($Followup231 -or $SignedRenderUpdate){
  if((Get-FileHash $vidpnHeader -Algorithm SHA256).Hash.ToLowerInvariant() -cne $vidpnHeaderHash){throw 'VidPn trace policy header changed during build'}
  $m.compile_closure.vidpn_callback_trace=@{path='tools/winpe/vidpn-callback-trace-policy.h';sha256=$vidpnHeaderHash;writer_source='b9fb91571b587ab98c01ed1a118c847b634b21ca';reader_sha256=(Get-FileHash "$root\tools\recovery\read-vidpn-callback-trace.ps1" -Algorithm SHA256).Hash.ToLowerInvariant()}
  $m.compile_closure.profile_macros+=@('A04_VIDPN_CALLBACK_TRACE=1')
 }
 if($SignedRenderUpdate){$m.compile_closure.profile_macros+=@('A04_KEYPAD14_CHECKPOINT=1')}
 if($Mode -eq 'Service'){
  $m.profile=$policy.profile
  foreach($p in $policy.packages.PSObject.Properties.Value){$d=Join-Path $Workspace $p.directory;foreach($n in (@($p.names.inf,$p.names.sys)+@($p.extra_members|Where-Object {$_ -and $_.name}|ForEach-Object {$_.name}))){& $sign verify /pa /c (Join-Path $d $p.names.cat) (Join-Path $d $n);if($LASTEXITCODE){throw 'Candidate CAT membership failed'}};$cs=Get-AuthenticodeSignature (Join-Path $d $p.names.cat);if($cs.Status -ne 'Valid' -or $cs.SignerCertificate.Thumbprint -cne $thumb){throw 'Candidate CAT signer mismatch'}}
  foreach($c in $policy.diagnostic_clients){$sigc=Get-AuthenticodeSignature (Join-Path (Join-Path $Workspace $c.directory) $c.exe);if($sigc.Status -ne 'Valid' -or $sigc.SignerCertificate.Thumbprint -cne $thumb){throw 'Pinned client signature mismatch'};foreach($dep in $c.dependencies){$sigc=Get-AuthenticodeSignature (Join-Path (Join-Path $Workspace $c.directory) $dep.exe);if($sigc.Status -ne 'Valid' -or $sigc.SignerCertificate.Thumbprint -cne $thumb){throw 'Pinned client dependency signature mismatch'}}}
  if($SignedRenderUpdate){
   $btVerifier=Join-Path $PSScriptRoot 'retained_bt_membership.py'
   $btVerifierSha=(Get-FileHash $btVerifier -Algorithm SHA256).Hash.ToLowerInvariant()
   $btPlanPath=Join-Path $out 'retained-bt-membership-plan.json'
   & python -B $btVerifier --workspace $Workspace --policy "$out\peripheral-policy.json" --out $btPlanPath
   if($LASTEXITCODE){throw 'Retained BT policy/manifest/actual byte binding failed'}
   $btPlan=Get-Content -LiteralPath $btPlanPath -Raw|ConvertFrom-Json
   $btCatalog=Join-Path $Workspace $btPlan.catalog.path
   foreach($member in $btPlan.members){& $sign verify /pa /c $btCatalog (Join-Path $Workspace $member.path);if($LASTEXITCODE){throw 'Retained BT CAT membership failed'}}
   $bs=Get-AuthenticodeSignature $btCatalog
   if($bs.Status -ne 'Valid' -or $bs.SignerCertificate.Thumbprint -cne $thumb){throw 'Retained BT CAT signer mismatch'}
   & python -B $btVerifier --workspace $Workspace --policy "$out\peripheral-policy.json" --verify-plan $btPlanPath
   if($LASTEXITCODE -or (Get-FileHash $btVerifier -Algorithm SHA256).Hash.ToLowerInvariant() -cne $btVerifierSha){throw 'Retained BT verifier/input changed during verification'}
   $m.retained_bt_membership=@{plan=$btPlan;plan_sha256=(Get-FileHash $btPlanPath -Algorithm SHA256).Hash.ToLowerInvariant();verifier_source_sha256=$btVerifierSha;Windows_CAT_membership_verified=$true;signer_thumbprint=$bs.SignerCertificate.Thumbprint}
  }else{
   $bt=$policy.bt_first_install;$bd=Join-Path $Workspace $bt.directory
   foreach($member in @('mt6630-combo-bluetooth.inf','mt6630-combo-bluetooth.sys')){& $sign verify /pa /c (Join-Path $bd 'mt6630-combo-bluetooth.cat') (Join-Path $bd $member);if($LASTEXITCODE){throw 'BT2013 membership failed'}}
   $bs=Get-AuthenticodeSignature (Join-Path $bd 'mt6630-combo-bluetooth.cat');if($bs.Status -ne 'Valid' -or $bs.SignerCertificate.Thumbprint -cne $thumb){throw 'BT2013 signer mismatch'}
  }
  $m.observer_contract=$policy.observer_contract;$m.source_TUs=1;$m.wlan_client_rebuilt=$false;$m.policy=$policy;$m.policy_manifest_sha256=(Get-FileHash "$out\peripheral-policy.json" -Algorithm SHA256).Hash.ToLowerInvariant();$m.preservation_plan_sha256=(Get-FileHash "$out\peripheral-preservation-plan.json" -Algorithm SHA256).Hash.ToLowerInvariant()
 }
 $manifest=if($Mode -eq 'CaptureClient'){'capture-client-manifest.json'}else{'peripheral-service-manifest.json'}
 $m|ConvertTo-Json -Depth 30|Set-Content -Encoding UTF8 "$out\$manifest"
 & (Join-Path (Split-Path (Get-Command clang.exe).Source) 'llvm-readobj.exe') --file-headers --coff-imports "$out\$name.exe"|Set-Content "$out\pe-imports.txt";if($LASTEXITCODE){throw 'PE import inspection failed'}
 Write-Output ($Mode+'_SIGNED_PATH_VERIFIED')
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
