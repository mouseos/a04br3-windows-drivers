[CmdletBinding()]
param(
    [string]$SourceDirectory,
    [string]$OutputDirectory,
    [string]$ExpectedSystemSha256,
    [string]$ExpectedLog1Sha256,
    [string]$ExpectedLog2Sha256,
    [switch]$IncludeHardwareProbe,
    [ValidateRange(5,120)][int]$TimeoutSeconds=30,
    [ValidateRange(1,999)][int]$ExpectedCurrentControlSet=1,
    [ValidateSet('','Patch','Verify','Cleanup')][string]$InternalMode='',
    [string]$WorkDirectory,
    [string]$StatePath,
    [string]$PlanPath,
    [string]$MountName
)
$ErrorActionPreference='Stop'
$nativeSource=Join-Path $PSScriptRoot 'OfflineSystemHiveNative.cs'
function Save-Json($Path,$Value) {
    $json=$Value | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText($Path,$json,(New-Object Text.UTF8Encoding($false)))
}
function Hash-File($Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Hive-Header($Path) {
    $file=[IO.File]::OpenRead($Path)
    try {
        $b=New-Object byte[] 4096
        if($file.Read($b,0,4096) -ne 4096) { throw 'Truncated hive base block' }
        $length=$file.Length
    } finally { $file.Dispose() }
    if([Text.Encoding]::ASCII.GetString($b,0,4) -cne 'regf') { throw 'Invalid SYSTEM signature' }
    [uint32]$sum=0
    for($i=0;$i -lt 0x1fc;$i+=4) { $sum=[uint32]($sum -bxor [BitConverter]::ToUInt32($b,$i)) }
    if($sum -eq 0) { $sum=1 } elseif($sum -eq [uint32]::MaxValue) { $sum=[uint32]4294967294 }
    $stored=[BitConverter]::ToUInt32($b,0x1fc)
    $major=[BitConverter]::ToUInt32($b,0x14);$minor=[BitConverter]::ToUInt32($b,0x18)
    $seq1=[BitConverter]::ToUInt32($b,4);$seq2=[BitConverter]::ToUInt32($b,8)
    $bins=[BitConverter]::ToUInt32($b,0x28)
    if($major -ne 1 -or $minor -ne 5 -or [BitConverter]::ToUInt32($b,0x1c) -ne 0 -or [BitConverter]::ToUInt32($b,0x20) -ne 1) { throw 'SYSTEM must retain native hive format 1.5 (primary file)' }
    if($stored -ne $sum) { throw 'Hive base block checksum mismatch' }
    if(($bins % 4096) -ne 0 -or $length -lt ([long]4096+$bins)) { throw 'Invalid hive bin extent' }
    return [ordered]@{major=$major;minor=$minor;sequence_primary=$seq1;sequence_secondary=$seq2;clean=($seq1 -eq $seq2);checksum=$stored;checksum_valid=$true;bytes=$length}
}
function Require-Dword($Key,$Name,$Expected) {
    if($Key.GetValueKind($Name) -ne [Microsoft.Win32.RegistryValueKind]::DWord -or [int]$Key.GetValue($Name) -ne $Expected) { throw "Unexpected DWORD: $Name" }
}
function Service-Values($Spec) {
    return [ordered]@{
        Type=@('DWord',16); Start=@('DWord',2); ErrorControl=@('DWord',1)
        ImagePath=@('ExpandString',('"C:\Windows\System32\'+$Spec.name+'.exe"'))
        ObjectName=@('String','LocalSystem'); DisplayName=@('String',[string]$Spec.display_name)
        DependOnService=@('MultiString',[string[]]@('DeviceInstall','RpcSs'))
    }
}
function Inspect-Services($Root,$Plan,[bool]$Patch) {
    $select=[MT8167OfflineHive.Native]::OpenPath($Root,'Select',$false)
    if(!$select) { throw 'Missing Select key' }
    try {
        Require-Dword $select 'Current' ([int]$Plan.control_set)
        Require-Dword $select 'Default' ([int]$Plan.control_set)
    } finally { $select.Dispose() }
    $control='ControlSet{0:D3}\Services' -f [int]$Plan.control_set
    $services=[MT8167OfflineHive.Native]::OpenPath($Root,$control,$true)
    if(!$services) { throw 'Missing services key' }
    try {
        foreach($dependency in @(@('DeviceInstall',3),@('RpcSs',2))) {
            $key=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'\'+$dependency[0]),$false)
            if(!$key) { throw ('Required dependency absent: '+$dependency[0]) }
            try { Require-Dword $key 'Type' 32; Require-Dword $key 'Start' ([int]$dependency[1]) } finally { $key.Dispose() }
        }
        $report=@()
        foreach($spec in $Plan.services) {
            $values=Service-Values $spec
            $key=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'\'+$spec.name),$true)
            if($key) {
                # Accept an absent or compatible partial registration only; do not rearm disabled services.
                foreach($name in $key.GetValueNames()) {
                    if(!$values.Contains($name)) { $key.Dispose();throw 'Unexpected existing one-shot service value' }
                    $expected=$values[$name];$kind=[Microsoft.Win32.RegistryValueKind]$expected[0]
                    $actual=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    $compatible=$key.GetValueKind($name) -eq $kind
                    if($kind -eq [Microsoft.Win32.RegistryValueKind]::MultiString) {
                        $compatible=$compatible -and (($actual -join "`0") -ceq ($expected[1] -join "`0"))
                    } elseif($name -eq 'ImagePath') {
                        $compatible=$compatible -and (($actual -ceq $expected[1]) -or ($Patch -and $actual -ceq ([string]$expected[1]).Trim('"')))
                    } else { $compatible=$compatible -and ($actual -ceq $expected[1]) }
                    if(!$compatible) { $key.Dispose();throw ('Conflicting existing service value: '+$spec.name+'/'+$name) }
                }
                if($key.GetSubKeyNames().Count -ne 0) { $key.Dispose();throw 'Unexpected one-shot service subkeys' }
            } elseif($Patch) { $key=$services.CreateSubKey([string]$spec.name) }
            else { throw 'One-shot service absent after clean-hive reload' }
            try {
                if($Patch) { foreach($name in $values.Keys) { $key.SetValue($name,$values[$name][1],[Microsoft.Win32.RegistryValueKind]$values[$name][0]) };$key.Flush() }
                if($key.GetValueNames().Count -ne $values.Count) { throw 'One-shot service value count mismatch' }
                $actualValues=[ordered]@{}
                foreach($name in $values.Keys) {
                    $expected=$values[$name];$actual=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]$expected[0]) { throw ('Service value type mismatch: '+$name) }
                    if($name -eq 'DependOnService') { if(($actual -join "`0") -cne ($expected[1] -join "`0")) { throw 'Dependency MULTI_SZ mismatch' } }
                    elseif($actual -cne $expected[1]) { throw ('Service value mismatch: '+$name) }
                    $actualValues[$name]=@{kind=[string]$key.GetValueKind($name);value=$actual}
                }
                $report+=@{name=$spec.name;values=$actualValues}
            } finally { $key.Dispose() }
        }
        return ,$report
    } finally { $services.Dispose() }
}
function Allowed-Changes($Plan) {
    $allowed=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach($spec in $Plan.services) {
        $path=('ControlSet{0:D3}\Services\' -f [int]$Plan.control_set)+$spec.name
        [void]$allowed.Add('K|'+$path)
        foreach($name in (Service-Values $spec).Keys) { [void]$allowed.Add('V|'+$path+[char]0+$name) }
    }
    return ,$allowed
}
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or [IntPtr]::Size -ne 8) { throw 'Use an elevated 64-bit Windows PowerShell/pwsh process' }
if($InternalMode) {
    $state=[ordered]@{status='failed';phase='initializing';load_rc=$null;unload_rc=$null;errors=@()}
    $loaded=$false;$root=$null;$privileges=$null
    try {
        if($MountName -notmatch '^MT8167OfflineSYSTEM_[0-9a-f]{32}$') { throw 'Invalid unique mount name' }
        if(-not ('MT8167OfflineHive.Native' -as [type])) { Add-Type -Path $nativeSource }
        $privileges=New-Object MT8167OfflineHive.Privileges
        if($InternalMode -eq 'Cleanup') {
            $state.phase='cleanup-unload-requested';Save-Json $StatePath $state
            $state.unload_rc=[MT8167OfflineHive.Native]::Unload($MountName)
            if($state.unload_rc -notin @(0,2,3)) { throw 'Bounded cleanup could not unload the private alias' }
        } else {
            $plan=Get-Content -Raw -LiteralPath $PlanPath | ConvertFrom-Json
            $state.phase='load-requested';Save-Json $StatePath $state
            $state.load_rc=[MT8167OfflineHive.Native]::Load($MountName,(Join-Path $WorkDirectory 'SYSTEM'))
            if($state.load_rc -ne 0) { throw ('RegLoadKey failed: '+$state.load_rc) }
            $loaded=$true;$state.phase='loaded-replayed';Save-Json $StatePath $state
            $root=[MT8167OfflineHive.Native]::OpenRoot($MountName)
            $allowed=Allowed-Changes $plan
            $before=[MT8167OfflineHive.Native]::Snapshot($root)
            $outsideBefore=[MT8167OfflineHive.Native]::OutsideDigest($before,$allowed)
            $state.services=Inspect-Services $root $plan ($InternalMode -eq 'Patch')
            $after=[MT8167OfflineHive.Native]::Snapshot($root)
            $outsideAfter=[MT8167OfflineHive.Native]::OutsideDigest($after,$allowed)
            if($outsideBefore -cne $outsideAfter) { throw 'Unrelated registry keys or raw values changed' }
            if($plan.outside_digest -and $outsideAfter -cne $plan.outside_digest) { throw 'Clean-hive reload changed unrelated registry content' }
            $state.outside_digest=$outsideAfter;$state.snapshot_entries_before=$before.Count;$state.snapshot_entries_after=$after.Count
            $root.Flush()
        }
        $state.status='api-complete'
    } catch { $state.errors+=@($_.Exception.Message) }
    finally {
        if($root) { $root.Dispose();$root=$null }
        if($loaded) {
            $state.phase='unload-requested';Save-Json $StatePath $state
            $state.unload_rc=[MT8167OfflineHive.Native]::Unload($MountName)
            if($state.unload_rc -ne 0) { $state.errors+=@('RegUnLoadKey did not succeed') }
        }
        if($privileges) { $privileges.Dispose();$state.privilege_restore_error=$privileges.RestoreError;if($privileges.RestoreError) { $state.errors+=@('Privilege restoration failed') } }
    }
    if($state.errors.Count -eq 0 -and $state.status -eq 'api-complete') { $state.status='success';$state.phase='unloaded' } else { $state.status='failed' }
    Save-Json $StatePath $state
    if($state.status -ne 'success') { exit 1 };exit 0
}
if(!$SourceDirectory -or !$OutputDirectory) { throw 'SourceDirectory and a new OutputDirectory are required' }
$source=(Resolve-Path -LiteralPath $SourceDirectory).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out) { throw 'OutputDirectory must be new' }
if($out.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'OutputDirectory must not be inside the original input' }
New-Item -ItemType Directory -Path $out | Out-Null
$work=Join-Path $out 'work';$verify=Join-Path $out 'verify';$candidate=Join-Path $out 'candidate'
New-Item -ItemType Directory -Path $work,$verify | Out-Null
$report=[ordered]@{status='failed-do-not-deploy';source_files=[ordered]@{};timeout_seconds=$TimeoutSeconds;services=@('MT8167DeviceInstall');errors=@();driver_files_modified=$false}
$plan=[ordered]@{control_set=$ExpectedCurrentControlSet;outside_digest='';services=@(@{name='MT8167DeviceInstall';display_name='MediaTek MT8167 Device Installation'})}
if($IncludeHardwareProbe) { $plan.services+=@(@{name='MT8167HardwareProbe';display_name='MediaTek MT8167 Hardware Probe'});$report.services+=@('MT8167HardwareProbe') }
$planFile=Join-Path $out 'plan.json';Save-Json $planFile $plan
$expected=@{SYSTEM=$ExpectedSystemSha256;'SYSTEM.LOG1'=$ExpectedLog1Sha256;'SYSTEM.LOG2'=$ExpectedLog2Sha256}
function Invoke-BoundedWorker($Mode,$Directory,$Name,[int]$Seconds) {
    $stateFile=Join-Path $out ($Name+'.json');$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N')
    if($Mode -eq 'Cleanup') { $mount=$Directory }
    $quote={param($s) "'"+$s.Replace("'","''")+"'"}
    $command='& '+(& $quote $PSCommandPath)+' -InternalMode '+$Mode+' -WorkDirectory '+(& $quote $Directory)+' -PlanPath '+(& $quote $planFile)+' -StatePath '+(& $quote $stateFile)+' -MountName '+(& $quote $mount)
    $psi=New-Object Diagnostics.ProcessStartInfo
    $psi.FileName=(Get-Process -Id $PID).Path
    $psi.Arguments='-NoProfile -NonInteractive -EncodedCommand '+[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.WorkingDirectory=$out
    $psi.EnvironmentVariables['TEMP']=$out;$psi.EnvironmentVariables['TMP']=$out
    $process=New-Object Diagnostics.Process;$process.StartInfo=$psi
    if(!$process.Start()) { throw 'Cannot start the native API worker' }
    $finished=$process.WaitForExit($Seconds*1000)
    if(!$finished) { try { $process.Kill() } catch {};[void]$process.WaitForExit(2000) }
    $state=$null
    if(Test-Path -LiteralPath $stateFile) { try { $state=Get-Content -Raw -LiteralPath $stateFile | ConvertFrom-Json } catch {} }
    $exit=$null;if($process.HasExited) { $exit=$process.ExitCode }
    $process.Dispose()
    return [pscustomobject]@{mode=$Mode;finished=$finished;exit_code=$exit;mount=$mount;state=$state}
}
try {
    foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')) {
        $path=Join-Path $source $name
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint)) { throw "Missing or reparse input: $name" }
        $hash=Hash-File $path
        if($expected[$name] -and $hash -cne $expected[$name].ToLowerInvariant()) { throw "Input hash pin mismatch: $name" }
        $report.source_files[$name]=@{sha256=$hash;bytes=(Get-Item -LiteralPath $path).Length}
        [IO.File]::Copy($path,(Join-Path $work $name),$false)
        if((Hash-File (Join-Path $work $name)) -cne $hash) { throw "Workcopy hash mismatch: $name" }
    }
    $report.input_header=Hive-Header (Join-Path $work 'SYSTEM')
    $patch=Invoke-BoundedWorker 'Patch' $work 'patch-state' $TimeoutSeconds;$report.patch=$patch
    if(!$patch.finished -or $patch.exit_code -ne 0 -or !$patch.state -or $patch.state.status -ne 'success') {
        $report.cleanup=Invoke-BoundedWorker 'Cleanup' $patch.mount 'cleanup-state' 5
        throw 'Native replay/patch/unload failed or timed out; do not deploy any workcopy'
    }
    $header=Hive-Header (Join-Path $work 'SYSTEM')
    if(!$header.clean) { throw 'Native unload did not produce equal clean sequence numbers' }
    $report.patched_header=$header;$patchedHash=Hash-File (Join-Path $work 'SYSTEM')
    # Reload a separate clean copy WITHOUT logs, proving that the candidate is self-contained.
    [IO.File]::Copy((Join-Path $work 'SYSTEM'),(Join-Path $verify 'SYSTEM'),$false)
    $plan.outside_digest=$patch.state.outside_digest;Save-Json $planFile $plan
    $check=Invoke-BoundedWorker 'Verify' $verify 'verify-state' $TimeoutSeconds;$report.verify=$check
    if(!$check.finished -or $check.exit_code -ne 0 -or !$check.state -or $check.state.status -ne 'success') {
        $report.cleanup=Invoke-BoundedWorker 'Cleanup' $check.mount 'verify-cleanup-state' 5
        throw 'Clean SYSTEM without logs did not reload and verify; do not deploy'
    }
    $verifyHeader=Hive-Header (Join-Path $verify 'SYSTEM');if(!$verifyHeader.clean) { throw 'Verification unload left dirty sequence' }
    if((Hash-File (Join-Path $work 'SYSTEM')) -cne $patchedHash) { throw 'Patched workcopy changed during verification' }
    foreach($name in $report.source_files.Keys) { if((Hash-File (Join-Path $source $name)) -cne $report.source_files[$name].sha256) { throw 'Original input changed' } }
    New-Item -ItemType Directory -Path $candidate | Out-Null
    [IO.File]::Copy((Join-Path $work 'SYSTEM'),(Join-Path $candidate 'SYSTEM'),$false)
    $report.candidate=@{path='candidate/SYSTEM';sha256=(Hash-File (Join-Path $candidate 'SYSTEM'));header=(Hive-Header (Join-Path $candidate 'SYSTEM'))}
    if($report.candidate.sha256 -cne $patchedHash) { throw 'Final clean SYSTEM copy hash mismatch' }
    $report.status='verified-clean-system-services-only'
} catch { $report.errors+=@($_.Exception.Message) }
finally {
    $report.recipe_sha256=Hash-File $PSCommandPath;$report.native_source_sha256=Hash-File $nativeSource
    Save-Json (Join-Path $out 'result.json') $report
}
Write-Host ('STATUS='+$report.status)
if($report.status -ne 'verified-clean-system-services-only') { throw 'Offline SYSTEM preparation failed; inspect private result.json, do not deploy work/verify copies' }
Write-Host ('CANDIDATE_SHA256='+$report.candidate.sha256)
