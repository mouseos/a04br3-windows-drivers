[CmdletBinding()]
param(
    [string]$SourceDirectory,
    [string]$OutputDirectory,
    [string]$ExpectedSystemSha256,
    [string]$ExpectedLog1Sha256,
    [string]$ExpectedLog2Sha256,
    [ValidateSet('MT8167DeviceInstall','MT8167HardwareProbe','MT8167EngineProbe')][string]$ServiceName='MT8167DeviceInstall',
    [switch]$IncludeHardwareProbe,
    [switch]$RearmFixedGpuDeviceInstall,
    [switch]$InspectFixedGpuDeviceInstall,
    [string]$GpuRearmPlanPath,
    [string]$ExpectedGpuRearmPlanSha256,
    [switch]$RearmEngineProbe,
    [string]$EngineRearmPlanPath,
    [string]$ExpectedEngineRearmPlanSha256,
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
function Fixed-ServiceSpec([string]$Name) {
    switch($Name) {
        'MT8167DeviceInstall' { return [ordered]@{name='MT8167DeviceInstall';display_name='MediaTek MT8167 Device Installation';image_path='"C:\Windows\System32\MT8167DeviceInstall.exe"'} }
        'MT8167HardwareProbe' { return [ordered]@{name='MT8167HardwareProbe';display_name='MediaTek MT8167 Hardware Probe';image_path='"C:\Windows\System32\MT8167HardwareProbe.exe"'} }
        'MT8167EngineProbe' { return [ordered]@{name='MT8167EngineProbe';display_name='MediaTek MT8167 Engine Probe';image_path='"C:\Windows\System32\MT8167EngineProbe.exe"'} }
        default { throw 'Unknown service name; only three fixed one-shot services are allowed' }
    }
}
function Service-Values($Spec) {
    $fixed=Fixed-ServiceSpec ([string]$Spec.name)
    if($Spec.name -cne $fixed.name -or $Spec.display_name -cne $fixed.display_name) { throw 'Service plan differs from the fixed allowlist' }
    return [ordered]@{
        Type=@('DWord',16); Start=@('DWord',2); ErrorControl=@('DWord',1)
        ImagePath=@('ExpandString',[string]$fixed.image_path)
        ObjectName=@('String','LocalSystem'); DisplayName=@('String',[string]$fixed.display_name)
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
            $beforeFingerprint=$null
            $gpuInspect=[bool]$Plan.gpu_inspect
            if($gpuInspect) {
                if($Patch -or $spec.name -cne 'MT8167DeviceInstall' -or !$key -or $key.GetValueNames().Count -ne 7) {
                    if($key) { $key.Dispose() };throw 'GPU inspection requires exactly the existing disabled DeviceInstall registration'
                }
                $values['Start']=@('DWord',4)
                $beforeFingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($key)
                if($Plan.gpu_inspect_fingerprint -and $beforeFingerprint -cne $Plan.gpu_inspect_fingerprint) {
                    $key.Dispose();throw 'DeviceInstall raw fingerprint changed on clean reload'
                }
            }
            $rearm=$Patch -and $null -ne $Plan.engine_rearm
            if($rearm) {
                if($spec.name -cne 'MT8167EngineProbe' -or !$key) { throw 'Rearm requires the existing fixed Engine service' }
                $beforeFingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($key)
                if($beforeFingerprint -cne 'dbbe363d2401dfc022606aba010c0e4888c5feab67c748ea5030b0eb3ba484c1' -or
                   $beforeFingerprint -cne $Plan.engine_rearm.disabled_service_fingerprint -or $key.GetValueNames().Count -ne 7) {
                    $key.Dispose();throw 'Disabled Engine service raw fingerprint differs from the known exact seven-value registration'
                }
            }
            $gpuRearm=$Patch -and $null -ne $Plan.gpu_rearm
            if($gpuRearm) {
                if($rearm -or $spec.name -cne 'MT8167DeviceInstall' -or !$key) { throw 'GPU rearm requires only the existing fixed DeviceInstall service' }
                $beforeFingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($key)
                if($beforeFingerprint -cne $Plan.gpu_rearm.disabled_service_fingerprint -or $key.GetValueNames().Count -ne 7) {
                    $key.Dispose();throw 'Disabled DeviceInstall fingerprint differs from the pinned native GPU plan'
                }
            }
            if($key) {
                # Accept an absent or compatible partial registration only; do not rearm disabled services.
                foreach($name in $key.GetValueNames()) {
                    if(!$values.Contains($name)) { $key.Dispose();throw 'Unexpected existing one-shot service value' }
                    $expected=$values[$name];if(($rearm -or $gpuRearm) -and $name -eq 'Start') { $expected=@('DWord',4) };$kind=[Microsoft.Win32.RegistryValueKind]$expected[0]
                    $actual=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    $compatible=$key.GetValueKind($name) -eq $kind
                    if($kind -eq [Microsoft.Win32.RegistryValueKind]::MultiString) {
                        $compatible=$compatible -and (($actual -join "`0") -ceq ($expected[1] -join "`0"))
                    } elseif($name -eq 'ImagePath') {
                        $compatible=$compatible -and (($actual -ceq $expected[1]) -or (($Patch -or $gpuInspect) -and $actual -ceq ([string]$expected[1]).Trim('"')))
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
                    elseif($gpuInspect -and $name -eq 'ImagePath') {
                        if($actual -cne $expected[1] -and $actual -cne ([string]$expected[1]).Trim('"')) { throw 'GPU inspection ImagePath mismatch' }
                    }
                    elseif($actual -cne $expected[1]) { throw ('Service value mismatch: '+$name) }
                    $actualValues[$name]=@{kind=[string]$key.GetValueKind($name);value=$actual}
                }
                $report+=@{name=$spec.name;values=$actualValues;before_fingerprint=$beforeFingerprint}
            } finally { $key.Dispose() }
        }
        return ,$report
    } finally { $services.Dispose() }
}
function Related-Fingerprints($Root,$Plan) {
    $control='ControlSet{0:D3}\Services\' -f [int]$Plan.control_set
    $paths=[ordered]@{select='Select';device_install=($control+'DeviceInstall');rpc_ss=($control+'RpcSs')}
    $result=[ordered]@{}
    foreach($name in $paths.Keys) {
        $key=[MT8167OfflineHive.Native]::OpenPath($Root,$paths[$name],$false)
        if(!$key) { throw ('Related key absent: '+$paths[$name]) }
        try { $result[$name]=[MT8167OfflineHive.Native]::ValueFingerprint($key) }
        finally { $key.Dispose() }
    }
    return $result
}
function Check-RelatedFingerprints($Actual,$Expected) {
    foreach($name in @('select','device_install','rpc_ss')) {
        if($Actual.$name -cne $Expected.$name) { throw ('Related existing settings changed: '+$name) }
    }
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
            $state.audit_scope='Select and raw values of Services/DeviceInstall and Services/RpcSs only; no subkeys or whole-SYSTEM audit'
            $state.whole_system_audit=$false
            $before=Related-Fingerprints $root $plan
            $state.services=Inspect-Services $root $plan ($InternalMode -eq 'Patch')
            if($plan.engine_rearm) {
                $state.engine_rearm_plan_sha256=$plan.engine_rearm_plan_sha256
                if($InternalMode -eq 'Patch') { $state.disabled_service_fingerprint=$state.services[0].before_fingerprint }
            }
            if($plan.gpu_inspect) { $state.gpu_inspected_service_fingerprint=$state.services[0].before_fingerprint;$state.service_values_written=$false }
            if($plan.gpu_rearm) {
                $state.gpu_rearm_plan_sha256=$plan.gpu_rearm_plan_sha256
                if($InternalMode -eq 'Patch') { $state.disabled_service_fingerprint=$state.services[0].before_fingerprint }
            }
            $after=Related-Fingerprints $root $plan
            Check-RelatedFingerprints $after $before
            if($plan.related_fingerprints) { Check-RelatedFingerprints $after $plan.related_fingerprints }
            $state.related_fingerprints_before=$before
            $state.related_fingerprints_after=$after
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
if($ServiceName -eq 'MT8167EngineProbe' -and $IncludeHardwareProbe) { throw 'EngineProbe cannot register or rearm the legacy HardwareProbe service' }
$engineRearmPlan=$null;$engineRearmPlanHash=$null
if($RearmEngineProbe) {
    if($ServiceName -cne 'MT8167EngineProbe' -or $IncludeHardwareProbe -or !$EngineRearmPlanPath -or
       $ExpectedEngineRearmPlanSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Explicit Engine-only rearm requires a pinned private rearm plan' }
    $engineRearmPlanHash=Hash-File $EngineRearmPlanPath
    if($engineRearmPlanHash -cne $ExpectedEngineRearmPlanSha256.ToLowerInvariant()) { throw 'Engine rearm plan SHA256 mismatch' }
    $engineRearmPlan=Get-Content -Raw -LiteralPath $EngineRearmPlanPath | ConvertFrom-Json
    if($engineRearmPlan.schema -ne 1 -or $engineRearmPlan.service -cne 'MT8167EngineProbe' -or $engineRearmPlan.maximum_rearms -ne 1 -or
       $engineRearmPlan.disabled_service_fingerprint -cne 'dbbe363d2401dfc022606aba010c0e4888c5feab67c748ea5030b0eb3ba484c1') { throw 'Only the known disabled Engine registration can be rearmed' }
} elseif($EngineRearmPlanPath -or $ExpectedEngineRearmPlanSha256) { throw 'Rearm plan requires explicit RearmEngineProbe opt-in' }
$gpuRearmPlan=$null;$gpuRearmPlanHash=$null
if($RearmFixedGpuDeviceInstall) {
    if($RearmEngineProbe -or $ServiceName -cne 'MT8167DeviceInstall' -or $IncludeHardwareProbe -or !$GpuRearmPlanPath -or
       $ExpectedGpuRearmPlanSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU-only rearm requires a pinned native plan' }
    $gpuRearmPlanHash=Hash-File $GpuRearmPlanPath
    if($gpuRearmPlanHash -cne $ExpectedGpuRearmPlanSha256.ToLowerInvariant()) { throw 'GPU rearm plan SHA256 mismatch' }
    $gpuRearmPlan=Get-Content -Raw -LiteralPath $GpuRearmPlanPath | ConvertFrom-Json
    if($gpuRearmPlan.schema -ne 1 -or $gpuRearmPlan.service -cne 'MT8167DeviceInstall' -or $gpuRearmPlan.maximum_rearms -ne 1 -or
       $gpuRearmPlan.candidate_source_commit -cne '04d0e573bd1bee7b619d25441f8ac05d28d5ccd5' -or
       $gpuRearmPlan.disabled_service_fingerprint -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid fixed GPU rearm plan' }
} elseif($GpuRearmPlanPath -or $ExpectedGpuRearmPlanSha256) { throw 'GPU plan requires explicit fixed GPU rearm opt-in' }
if($InspectFixedGpuDeviceInstall) {
    if($ServiceName -cne 'MT8167DeviceInstall' -or $IncludeHardwareProbe -or $RearmFixedGpuDeviceInstall -or $RearmEngineProbe -or
       $GpuRearmPlanPath -or $ExpectedGpuRearmPlanSha256 -or $EngineRearmPlanPath -or $ExpectedEngineRearmPlanSha256) { throw 'GPU inspection cannot register, rearm or inspect another service' }
    foreach($pin in @($ExpectedSystemSha256,$ExpectedLog1Sha256,$ExpectedLog2Sha256)) {
        if($pin -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU inspection requires explicit fresh SYSTEM/LOG1/LOG2 SHA256 pins' }
    }
}
if(!$SourceDirectory -or !$OutputDirectory) { throw 'SourceDirectory and a new OutputDirectory are required' }
$source=(Resolve-Path -LiteralPath $SourceDirectory).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out) { throw 'OutputDirectory must be new' }
if($out.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'OutputDirectory must not be inside the original input' }
New-Item -ItemType Directory -Path $out | Out-Null
$work=Join-Path $out 'work';$verify=Join-Path $out 'verify';$candidate=Join-Path $out 'candidate'
New-Item -ItemType Directory -Path $work,$verify | Out-Null
$selected=@((Fixed-ServiceSpec $ServiceName))
# Backwards-compatible explicit opt-in for both; selecting HardwareProbe alone
# never implicitly creates or rearms MT8167DeviceInstall.
if($IncludeHardwareProbe -and $selected[0].name -ne 'MT8167HardwareProbe') { $selected+=@((Fixed-ServiceSpec 'MT8167HardwareProbe')) }
$names=@($selected | ForEach-Object { $_.name })
$report=[ordered]@{status='failed-do-not-deploy';whole_system_audit=$false;audit_scope='Select + existing DeviceInstall/RpcSs raw values and exact selected target service values only';source_files=[ordered]@{};timeout_seconds=$TimeoutSeconds;services=$names;errors=@();driver_files_modified=$false}
$plan=[ordered]@{control_set=$ExpectedCurrentControlSet;related_fingerprints='';services=$selected}
if($engineRearmPlan) {
    $plan.engine_rearm=$engineRearmPlan;$plan.engine_rearm_plan_sha256=$engineRearmPlanHash
    $report.engine_rearm_plan_sha256=$engineRearmPlanHash
}
if($gpuRearmPlan) {
    $plan.gpu_rearm=$gpuRearmPlan;$plan.gpu_rearm_plan_sha256=$gpuRearmPlanHash
    $report.gpu_rearm_plan_sha256=$gpuRearmPlanHash
}
if($InspectFixedGpuDeviceInstall) { $plan.gpu_inspect=$true;$report.service_values_written=$false }
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
    if($engineRearmPlan) {
        foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')) {
            $pin=$engineRearmPlan.source_files.$name
            if(!$pin -or $pin.sha256.ToLowerInvariant() -cne $report.source_files[$name].sha256 -or $pin.bytes -ne $report.source_files[$name].bytes) { throw 'Rearm plan and fresh native input differ' }
        }
    }
    if($gpuRearmPlan) {
        foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')) {
            $pin=$gpuRearmPlan.source_files.$name
            if(!$pin -or $pin.sha256.ToLowerInvariant() -cne $report.source_files[$name].sha256 -or $pin.bytes -ne $report.source_files[$name].bytes) { throw 'GPU plan and fresh native input differ' }
        }
    }
    $report.input_header=Hive-Header (Join-Path $work 'SYSTEM')
    $firstMode=if($InspectFixedGpuDeviceInstall){'Verify'}else{'Patch'}
    $patch=Invoke-BoundedWorker $firstMode $work 'patch-state' $TimeoutSeconds;$report.patch=$patch
    if(!$patch.finished -or $patch.exit_code -ne 0 -or !$patch.state -or $patch.state.status -ne 'success') {
        $report.cleanup=Invoke-BoundedWorker 'Cleanup' $patch.mount 'cleanup-state' 5
        throw 'Native replay/patch/unload failed or timed out; do not deploy any workcopy'
    }
    $header=Hive-Header (Join-Path $work 'SYSTEM')
    if(!$header.clean) { throw 'Native unload did not produce equal clean sequence numbers' }
    $report.patched_header=$header;$patchedHash=Hash-File (Join-Path $work 'SYSTEM')
    # Reload a separate clean copy WITHOUT logs, proving that the candidate is self-contained.
    [IO.File]::Copy((Join-Path $work 'SYSTEM'),(Join-Path $verify 'SYSTEM'),$false)
    $plan.related_fingerprints=$patch.state.related_fingerprints_after
    if($InspectFixedGpuDeviceInstall) {
        $fingerprint=[string]$patch.state.gpu_inspected_service_fingerprint
        if($fingerprint -cnotmatch '^[0-9a-f]{64}$') { throw 'Native GPU inspection did not return a real raw fingerprint' }
        $plan.gpu_inspect_fingerprint=$fingerprint
    }
    Save-Json $planFile $plan
    $check=Invoke-BoundedWorker 'Verify' $verify 'verify-state' $TimeoutSeconds;$report.verify=$check
    if(!$check.finished -or $check.exit_code -ne 0 -or !$check.state -or $check.state.status -ne 'success') {
        $report.cleanup=Invoke-BoundedWorker 'Cleanup' $check.mount 'verify-cleanup-state' 5
        throw 'Clean SYSTEM without logs did not reload and verify; do not deploy'
    }
    $verifyHeader=Hive-Header (Join-Path $verify 'SYSTEM');if(!$verifyHeader.clean) { throw 'Verification unload left dirty sequence' }
    if((Hash-File (Join-Path $work 'SYSTEM')) -cne $patchedHash) { throw 'Patched workcopy changed during verification' }
    foreach($name in $report.source_files.Keys) { if((Hash-File (Join-Path $source $name)) -cne $report.source_files[$name].sha256) { throw 'Original input changed' } }
    if($InspectFixedGpuDeviceInstall) {
        if($check.state.gpu_inspected_service_fingerprint -cne $fingerprint) { throw 'Native clean reload GPU fingerprint differs' }
        $gpuPlan=[ordered]@{schema=1;service='MT8167DeviceInstall';maximum_rearms=1;
            candidate_source_commit='04d0e573bd1bee7b619d25441f8ac05d28d5ccd5';
            candidate_sys_sha256='13c8a9ca6b6d7fef921afd38d60cf84ea8e37b599b51af82d6e90342d8b9da07';
            candidate_cat_sha256='5d4cfad2d2b0e1758777c0d531aae92e172a1b4d53b206649b5f7f2b08c4d828';
            disabled_service_fingerprint=$fingerprint;source_files=$report.source_files;
            native_inspection_recipe_sha256=(Hash-File $PSCommandPath);native_source_sha256=(Hash-File $nativeSource);
            whole_system_audit=$false;service_values_written=$false}
        $gpuPlanPath=Join-Path $out 'gpu-rearm-plan.json';Save-Json $gpuPlanPath $gpuPlan
        $report.gpu_rearm_plan=@{path='gpu-rearm-plan.json';sha256=(Hash-File $gpuPlanPath);disabled_service_fingerprint=$fingerprint}
        $report.status='verified-disabled-deviceinstall-gpu-plan'
    } else {
    New-Item -ItemType Directory -Path $candidate | Out-Null
    [IO.File]::Copy((Join-Path $work 'SYSTEM'),(Join-Path $candidate 'SYSTEM'),$false)
    $report.candidate=@{path='candidate/SYSTEM';sha256=(Hash-File (Join-Path $candidate 'SYSTEM'));header=(Hive-Header (Join-Path $candidate 'SYSTEM'))}
    if($report.candidate.sha256 -cne $patchedHash) { throw 'Final clean SYSTEM copy hash mismatch' }
    $report.status='verified-clean-system-services-only'
    }
} catch { $report.errors+=@($_.Exception.Message) }
finally {
    $report.recipe_sha256=Hash-File $PSCommandPath;$report.native_source_sha256=Hash-File $nativeSource
    Save-Json (Join-Path $out 'result.json') $report
}
Write-Host ('STATUS='+$report.status)
if($report.status -notin @('verified-clean-system-services-only','verified-disabled-deviceinstall-gpu-plan')) { throw 'Offline SYSTEM preparation failed; inspect private result.json, do not deploy work/verify copies' }
if($InspectFixedGpuDeviceInstall) { Write-Host ('GPU_REARM_PLAN_SHA256='+$report.gpu_rearm_plan.sha256) }
else { Write-Host ('CANDIDATE_SHA256='+$report.candidate.sha256) }
