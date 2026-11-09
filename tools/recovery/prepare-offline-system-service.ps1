[CmdletBinding()]
param(
    [string]$SourceDirectory,
    [string]$OutputDirectory,
    [string]$ExpectedSystemSha256,
    [string]$ExpectedLog1Sha256,
    [string]$ExpectedLog2Sha256,
    [ValidateSet('MT8167DeviceInstall','MT8167HardwareProbe','MT8167EngineProbe','MT8167RenderProbe','MT8167RenderStage','MT8167RenderSingleGpu','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')][string]$ServiceName='MT8167DeviceInstall',
    [switch]$IncludeHardwareProbe,
    [switch]$RearmFixedGpuDeviceInstall,
    [switch]$InspectFixedGpuDeviceInstall,
    [string]$GpuCandidateDirectory,
    [string]$ExpectedGpuCandidateManifestSha256,
    [string]$GpuCandidateSourceCommit,
    [string]$ExpectedGpuPolicySha256,
    [string]$GpuRearmPlanPath,
    [string]$ExpectedGpuRearmPlanSha256,
    [string]$PeripheralPreservationPlanPath,
    [string]$ExpectedPeripheralPreservationPlanSha256,
    [string]$EnginePreservationPlanPath,
    [string]$ExpectedEnginePreservationPlanSha256,
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
. (Join-Path $PSScriptRoot 'Read-SingleGpuLastDiagnostic.ps1')
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
        'MT8167RenderProbe' { return [ordered]@{name='MT8167RenderProbe';display_name='PowerVR GE8300 Render Probe';image_path='"C:\Windows\System32\MT8167RenderProbe.exe"'} }
        'MT8167RenderStage' { return [ordered]@{name='MT8167RenderStage';display_name='PowerVR GE8300 Driver Update';image_path='"C:\Windows\System32\MT8167RenderStage.exe"'} }
        'MT8167PeripheralNext122' { return [ordered]@{name='MT8167PeripheralNext122';display_name='MediaTek next peripheral batch';image_path='"C:\Windows\System32\MT8167PeripheralNext122.exe"'} }
        'MT8167CombinedFollowup154' { return [ordered]@{name='MT8167CombinedFollowup154';display_name='MediaTek matched thermal1117 GPU343 verification';image_path='"C:\Windows\System32\MT8167CombinedFollowup154.exe"'} }
        'MT8167CombinedFollowup152' { return [ordered]@{name='MT8167CombinedFollowup152';display_name='MediaTek GPU342 verification';image_path='"C:\Windows\System32\MT8167CombinedFollowup152.exe"'} }
        'MT8167CombinedFollowup150' { return [ordered]@{name='MT8167CombinedFollowup150';display_name='MediaTek combined features150';image_path='"C:\Windows\System32\MT8167CombinedFollowup150.exe"'} }
        'MT8167CombinedFollowup148' { return [ordered]@{name='MT8167CombinedFollowup148';display_name='MediaTek combined features148';image_path='"C:\Windows\System32\MT8167CombinedFollowup148.exe"'} }
        'MT8167CombinedFollowup146' { return [ordered]@{name='MT8167CombinedFollowup146';display_name='MediaTek combined features146';image_path='"C:\Windows\System32\MT8167CombinedFollowup146.exe"'} }
        'MT8167CombinedFollowup144' { return [ordered]@{name='MT8167CombinedFollowup144';display_name='MediaTek combined features144';image_path='"C:\Windows\System32\MT8167CombinedFollowup144.exe"'} }
        'MT8167CombinedFollowup142' { return [ordered]@{name='MT8167CombinedFollowup142';display_name='MediaTek combined audio142';image_path='"C:\Windows\System32\MT8167CombinedFollowup142.exe"'} }
        'MT8167CombinedFollowup140' { return [ordered]@{name='MT8167CombinedFollowup140';display_name='MediaTek combined peripherals140';image_path='"C:\Windows\System32\MT8167CombinedFollowup140.exe"'} }
        'MT8167CombinedFollowup138' { return [ordered]@{name='MT8167CombinedFollowup138';display_name='MediaTek combined peripherals138';image_path='"C:\Windows\System32\MT8167CombinedFollowup138.exe"'} }
        'MT8167CombinedFollowup136' { return [ordered]@{name='MT8167CombinedFollowup136';display_name='MediaTek GPU335 UMD update';image_path='"C:\Windows\System32\MT8167CombinedFollowup136.exe"'} }
        'MT8167CombinedFollowup134' { return [ordered]@{name='MT8167CombinedFollowup134';display_name='MediaTek GPU332 combined update';image_path='"C:\Windows\System32\MT8167CombinedFollowup134.exe"'} }
        'MT8167CombinedFollowup132' { return [ordered]@{name='MT8167CombinedFollowup132';display_name='MediaTek RTC AFE readonly followup';image_path='"C:\Windows\System32\MT8167CombinedFollowup132.exe"'} }
        'MT8167CombinedFollowup130' { return [ordered]@{name='MT8167CombinedFollowup130';display_name='MediaTek combined followup batch';image_path='"C:\Windows\System32\MT8167CombinedFollowup130.exe"'} }
        'MT8167CombinedFollowup128' { return [ordered]@{name='MT8167CombinedFollowup128';display_name='MediaTek combined followup batch';image_path='"C:\Windows\System32\MT8167CombinedFollowup128.exe"'} }
        'MT8167CombinedFollowup126' { return [ordered]@{name='MT8167CombinedFollowup126';display_name='MediaTek combined followup batch';image_path='"C:\Windows\System32\MT8167CombinedFollowup126.exe"'} }
        'MT8167CombinedFollowup124' { return [ordered]@{name='MT8167CombinedFollowup124';display_name='MediaTek combined followup batch';image_path='"C:\Windows\System32\MT8167CombinedFollowup124.exe"'} }
        'MT8167PeripheralBatch' { return [ordered]@{name='MT8167PeripheralBatch';display_name='MediaTek fixed peripheral batch';image_path='"C:\Windows\System32\MT8167PeripheralBatch.exe"'} }
        'MT8167RenderSingleGpu' { return [ordered]@{name='MT8167RenderSingleGpu';display_name='PowerVR GE8300 Single GPU Update';image_path='"C:\Windows\System32\MT8167RenderSingleGpu.exe"'} }
        default { throw 'Unknown fixed one-shot service name' }
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
# Read ONLY this fixed service's known logger subtree. Never mutate its values.
# Raw value fingerprints include names/types/data; directory shape is bounded.
function Engine-DiagnosticFingerprint($ServiceKey,$Plan,[bool]$SingleGpu=$false) {
    $context=if($SingleGpu){$Plan.engine_preserve.single_gpu_rearm}elseif($Plan.engine_rearm){$Plan.engine_rearm}elseif($Plan.engine_preserve){$Plan.engine_preserve}else{throw 'Explicit Engine preservation context required'}
    $allowed=@($context.diagnostic_policies)
    $partialPolicies=@()
    if($SingleGpu){
        if($context.partial_diagnostic_policy){throw 'Use explicit partial_diagnostic_policies array instead of singular policy'}
        if($null -ne $context.partial_diagnostic_policies){
            if($context.partial_diagnostic_policies -isnot [Array]){throw 'Partial diagnostic policy pins must be an array'}
            $partialPolicies=@($context.partial_diagnostic_policies)
        }
    }
    Assert-Mt8167SingleGpuPartialPolicies $partialPolicies $allowed
    $parts=New-Object 'Collections.Generic.List[string]'
    $serviceChildren=@($ServiceKey.GetSubKeyNames())
    if($serviceChildren.Count -eq 0) { return 'absent' }
    if($serviceChildren.Count -ne 1 -or $serviceChildren[0] -cne 'Parameters') { throw 'Unexpected Engine subkey outside fixed Parameters' }
    $parameters=$ServiceKey.OpenSubKey('Parameters',$false)
    try {
        if($parameters.GetValueNames().Count -or @($parameters.GetSubKeyNames()).Count -ne 1 -or $parameters.GetSubKeyNames()[0] -cne 'LastDiagnostic') { throw 'Unexpected Engine Parameters shape' }
        $last=$parameters.OpenSubKey('LastDiagnostic',$false)
        try {
            if($last.GetValueNames().Count) { throw 'Unexpected LastDiagnostic root values' }
            $policies=@($last.GetSubKeyNames()|Sort-Object)
            if(!$policies.Count -or $policies.Count -gt 16) { throw 'Bounded diagnostic history exceeded' }
            foreach($policy in $policies) {
                if($policy -cnotmatch '^[0-9a-f]{64}$' -or $policy -cnotin $allowed) { throw 'Unpinned Engine diagnostic generation' }
                $generation=$last.OpenSubKey($policy,$false)
                try {
                    if($generation.GetValueNames().Count) { throw 'Unexpected diagnostic generation values' }
                    $phases=@($generation.GetSubKeyNames()|Sort-Object)
                    if(!$phases.Count -or $phases.Count -gt 2 -or @($phases|Where-Object {$_ -cnotin @('Install','Diagnostic')}).Count) { throw 'Unknown diagnostic phase' }
                    foreach($phase in $phases) {
                        $leaf=$generation.OpenSubKey($phase,$false)
                        try {
                            if($leaf.GetSubKeyNames().Count -or $leaf.GetValueNames().Count -gt 64) { throw 'Diagnostic leaf bound exceeded' }
                            if($SingleGpu) {
                                $partial=Read-Mt8167SingleGpuPartialHeader $leaf $phase $policy $partialPolicies $allowed
                                if($null -ne $partial) {
                                    $parts.Add($policy+'/'+$phase+'='+$partial.raw_fingerprint)
                                    continue
                                }
                            }
                            $schema=[int]$leaf.GetValue('Schema',-1);$count=[int]$leaf.GetValue('TargetCount',-1);$clients=[int]$leaf.GetValue('ClientCount',-1)
                            $singleRender=$SingleGpu -and $schema -eq 2 -and $count -eq 1 -and $clients -eq 1
                            $codecRender=$schema -eq 2 -and $count -eq 4 -and $clients -eq 2
                            $legacyEngine=$schema -in @(1,2) -and $count -ge 5 -and $count -le $(if($schema -eq 1){6}else{7}) -and $clients -in @(2,3)
                            if(($SingleGpu -and !$singleRender) -or (!$SingleGpu -and !$codecRender -and !$legacyEngine) -or $leaf.GetValue('PolicyHash') -cne $policy) { throw 'Diagnostic schema/count/policy mismatch' }
                            if($singleRender) {
                                if($leaf.GetValueKind('Node0HardwareId') -ne [Microsoft.Win32.RegistryValueKind]::String -or $leaf.GetValue('Node0HardwareId') -cne 'ACPI\MOU0006'){throw 'Fixed single GPU identity required'}
                            }
                            if($codecRender) {
                                $codecIds=@('ACPI\BEN0001','MT8167\AFE','ACPI\MOU0001','ACPI\MOU0006')
                                for($codecIndex=0;$codecIndex -lt 4;$codecIndex++) {
                                    $codecName="Node${codecIndex}HardwareId"
                                    if($leaf.GetValueKind($codecName) -ne [Microsoft.Win32.RegistryValueKind]::String -or $leaf.GetValue($codecName) -cne $codecIds[$codecIndex]) { throw 'Diagnostic fixed codec/render node mismatch' }
                                }
                            }
                            foreach($name in $leaf.GetValueNames()) {
                                $kind=$leaf.GetValueKind($name);$v=$leaf.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                                if($name -cmatch '^(Schema|Complete|RunId|Phase|TargetCount|ClientCount|ServiceResult|FileLogError|UartError|SnapshotWriteError)$' -or ($schema -eq 2 -and $name -cmatch '^(SystemVolumeFlushError|CompletionResult|PnpBatchResult|InstallFailureValid|InstallFailureTarget|InstallFailureStep|AllInstallStepsSucceeded|DiagnosticArmed|RebootPending|DeferredPendingStatus|DiagnosticAttemptedStatus|RebootAttemptedStatus)$')) {
                                    if($kind -ne [Microsoft.Win32.RegistryValueKind]::DWord) { throw 'Diagnostic DWORD type mismatch' }
                                } elseif($codecRender -and $name -cmatch '^(Mt8167MsdcFileReadBytes|Mt8167MsdcFileReadDigest|Mt8167MsdcFileReadElapsedMs|Mt8167MsdcFileReadError|PhysicalEmmcThroughputMeasured|UartErrorPathExercised)$') {
                                    if($kind -ne [Microsoft.Win32.RegistryValueKind]::DWord) { throw 'Codec MSDC DWORD type mismatch' }
                                } elseif($codecRender -and $name -cmatch '^Retained([0-2])HardwareId$') {
                                    $retainIds=@('MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007')
                                    if($kind -ne [Microsoft.Win32.RegistryValueKind]::String -or $v -cne $retainIds[[int]$Matches[1]]) { throw 'Fixed retained identity/type mismatch' }
                                } elseif($name -ceq 'PolicyHash') {
                                    if($kind -ne [Microsoft.Win32.RegistryValueKind]::String -or $v -cne $policy) { throw 'Diagnostic policy value mismatch' }
                                } elseif($name -cmatch '^Node([0-6])HardwareId$') {
                                    $index=[int]$Matches[1]
                                    if($index -ge $count -or $kind -ne [Microsoft.Win32.RegistryValueKind]::String) { throw 'Diagnostic node ID bound/type mismatch' }
                                    $ids=@('ACPI\BEN0001','MT8167\AFE','MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007')
                                    if($singleRender) { if($index -ne 0 -or $v -cne 'ACPI\MOU0006'){throw 'Single GPU node mismatch'} }
                                    elseif($codecRender) { if($v -cne $codecIds[$index]) { throw 'Diagnostic fixed codec/render node mismatch' } }
                                    elseif($index -lt 5) { if($v -cne $ids[$index]) { throw 'Diagnostic fixed node mismatch' } }
                                    elseif($index -eq 5) { if($v -cnotin @('ACPI\MOU0006','ACPI\MOU0013') -or ($schema -eq 1 -and $v -cne 'ACPI\MOU0006')) { throw 'Diagnostic optional node mismatch' } }
                                    elseif($v -cne 'ACPI\MOU0013' -or $leaf.GetValue('Node5HardwareId') -cne 'ACPI\MOU0006') { throw 'Diagnostic GPU/SD order mismatch' }
                                } else {
                                    $size=-1
                                    if($name -cmatch '^Node([0-6])(Before|After)$') { if([int]$Matches[1] -ge $count) { throw 'Node bound exceeded' };$size=44 }
                                    elseif($name -cmatch '^Client([0-2])$') { if([int]$Matches[1] -ge $clients) { throw 'Client bound exceeded' };$size=28 }
                                    elseif($name -cmatch '^Client([0-2])(Stdout|Stderr)Info$') { if([int]$Matches[1] -ge $clients) { throw 'Stream bound exceeded' };$size=20 }
                                    elseif($name -cmatch '^Client([0-2])(Stdout|Stderr)$') { if([int]$Matches[1] -ge $clients) { throw 'Stream bound exceeded' };$size=-2 }
                                    elseif($schema -eq 2 -and $name -ceq 'SystemVolumeFlush') { $size=28 }
                                    if($size -eq -1 -or $kind -ne [Microsoft.Win32.RegistryValueKind]::Binary -or $v.Length -gt 65536 -or ($size -ge 0 -and $v.Length -ne $size)) { throw 'Unknown diagnostic value/type/size' }
                                }
                            }
                            $parts.Add($policy+'/'+$phase+'='+[MT8167OfflineHive.Native]::ValueFingerprint($leaf))
                        } finally { $leaf.Dispose() }
                    }
                } finally { $generation.Dispose() }
            }
        } finally { $last.Dispose() }
    } finally { $parameters.Dispose() }
    $hash=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes(($parts -join "`n"))))).Replace('-','').ToLowerInvariant() } finally { $hash.Dispose() }
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
            if($spec.name -cin @('MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142') -and $Patch -and $key){$key.Dispose();throw 'Dedicated peripheral service must be absent before initial arm'}
            if($spec.name -ceq 'MT8167RenderSingleGpu' -and (!$key -or !$Plan.engine_preserve.single_gpu_rearm)){if($key){$key.Dispose()};throw 'Pinned existing single GPU registration required'}
            $beforeFingerprint=$null;$diagnosticFingerprint=$null
            $singleRearm=$Patch -and $spec.name -ceq 'MT8167RenderSingleGpu'
            if($singleRearm -and $key.GetValueNames().Count -ne 7){$key.Dispose();throw 'Single GPU exact seven values required'}
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
                    $expected=$values[$name];if(($rearm -or $gpuRearm -or $singleRearm) -and $name -eq 'Start') { $expected=@('DWord',4) };$kind=[Microsoft.Win32.RegistryValueKind]$expected[0]
                    $actual=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    $compatible=$key.GetValueKind($name) -eq $kind
                    if($kind -eq [Microsoft.Win32.RegistryValueKind]::MultiString) {
                        $compatible=$compatible -and (($actual -join "`0") -ceq ($expected[1] -join "`0"))
                    } elseif($name -eq 'ImagePath') {
                        $compatible=$compatible -and (($actual -ceq $expected[1]) -or (($Patch -or $gpuInspect) -and $actual -ceq ([string]$expected[1]).Trim('"')))
                    } else { $compatible=$compatible -and ($actual -ceq $expected[1]) }
                    if(!$compatible) { $key.Dispose();throw ('Conflicting existing service value: '+$spec.name+'/'+$name) }
                }
                if($spec.name -ceq 'MT8167EngineProbe' -and $Plan.engine_rearm) {
                    try { $diagnosticFingerprint=Engine-DiagnosticFingerprint $key $Plan } catch { $key.Dispose();throw }
                    if($Plan.engine_diagnostic_fingerprint -and $diagnosticFingerprint -cne $Plan.engine_diagnostic_fingerprint) { $key.Dispose();throw 'Engine diagnostics changed on clean reload' }
                } elseif($spec.name -ceq 'MT8167RenderSingleGpu') {
                    try {$diagnosticFingerprint=Engine-DiagnosticFingerprint $key $Plan $true} catch {$key.Dispose();throw}
                } elseif($key.GetSubKeyNames().Count -ne 0) { $key.Dispose();throw 'Unexpected one-shot service subkeys' }
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
                $report+=@{name=$spec.name;values=$actualValues;before_fingerprint=$beforeFingerprint;diagnostic_fingerprint=$diagnosticFingerprint}
            } finally { $key.Dispose() }
        }
        return ,$report
    } finally { $services.Dispose() }
}
# Fixed old-service subtrees only. Keep every raw value; no whole-hive traversal.
function Peripheral-ServiceSnapshot($Root,[string]$Path) {
    $key=[MT8167OfflineHive.Native]::OpenPath($Root,$Path,$false)
    if(!$key){return [ordered]@{present=$false;root_sha256=$null;subtree_sha256=$null}}
    $parts=New-Object 'Collections.Generic.List[string]';$count=0
    function Visit($current,[string]$relative,[int]$depth){
        if($depth -gt 6 -or $parts.Count -ge 128 -or $current.GetValueNames().Count -gt 64 -or $current.GetSubKeyNames().Count -gt 32){throw 'Bounded preserved-service subtree exceeded'}
        foreach($n in $current.GetValueNames()){
            $kind=$current.GetValueKind($n);if($kind -notin @('String','ExpandString','Binary','DWord','MultiString','QWord','None')){throw 'Unexpected preserved-service value type'}
            $v=$current.GetValue($n,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $size=if($v -is [byte[]]){$v.Length}elseif($v -is [string]){2*($v.Length+1)}elseif($v -is [string[]]){2*(($v -join '').Length+$v.Count+1)}else{8}
            if($size -gt 65536){throw 'Preserved-service value size exceeded'}
        }
        $parts.Add($relative+'='+[MT8167OfflineHive.Native]::ValueFingerprint($current))
        foreach($n in @($current.GetSubKeyNames()|Sort-Object)){
            $child=[MT8167OfflineHive.Native]::OpenPath($Root,($Path+$(if($relative){'\'+$relative}else{''})+'\'+$n),$false)
            if(!$child){throw 'Preserved subtree changed during read'}
            try{Visit $child $(if($relative){$relative+'\'+$n}else{$n}) ($depth+1)}finally{$child.Dispose()}
        }
    }
    try{
        $rootHash=[MT8167OfflineHive.Native]::ValueFingerprint($key);Visit $key '' 0
        $hash=[Security.Cryptography.SHA256]::Create()
        try{$digest=([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes(($parts -join "`n"))))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}
        return [ordered]@{present=$true;root_sha256=$rootHash;subtree_sha256=$digest}
    }finally{$key.Dispose()}
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
    if($Plan.engine_preserve){
        $engine=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'MT8167EngineProbe'),$false)
        if(!$engine){throw 'Preserved Engine absent'}
        try {
            if($engine.GetValueNames().Count -ne 7 -or [MT8167OfflineHive.Native]::ValueFingerprint($engine) -cne 'dbbe363d2401dfc022606aba010c0e4888c5feab67c748ea5030b0eb3ba484c1'){throw 'Preserved Engine is not the known disabled registration'}
            $result.engine_service=[MT8167OfflineHive.Native]::ValueFingerprint($engine)
            $result.engine_ledger=Engine-DiagnosticFingerprint $engine $Plan
        } finally {$engine.Dispose()}
    }
    if($Plan.engine_preserve.single_gpu_rearm){
        $sg=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'MT8167RenderSingleGpu'),$false)
        if(!$sg){throw 'Single GPU registration missing'}
        try {$result.single_gpu_ledger=Engine-DiagnosticFingerprint $sg $Plan $true}finally{$sg.Dispose()}
    }
    if($Plan.peripheral_preserve){
        $old=[ordered]@{}
        $oldNames=@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs')
        if($Plan.peripheral_preserve.service -cin @('MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167PeripheralBatch'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167PeripheralNext122'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup124'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+=@('MT8167CombinedFollowup126','Mt6392Keys')}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup128'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup130'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup132'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup134'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup136'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup140'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup142'}
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){$oldNames+='MT8167CombinedFollowup144'}
        foreach($n in $oldNames){
            $snapshot=Peripheral-ServiceSnapshot $Root ($control+$n)
            $expected=$Plan.peripheral_preserve.service_preservation.$n
            if($snapshot.present -ne $expected.present -or $snapshot.root_sha256 -cne $expected.root_sha256 -or $snapshot.subtree_sha256 -cne $expected.subtree_sha256){throw 'Old service snapshot differs from explicitly pinned fresh input'}
            $old[$n]=$snapshot
        }
        $result.peripheral_old_services=($old|ConvertTo-Json -Depth 8 -Compress)
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){
            $result.peripheral_captured_values=(Peripheral-CapturedValues $Root $Plan.control_set $Plan.peripheral_preserve.captured_registry_values | ConvertTo-Json -Depth 8 -Compress)
        }
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){
            $failed=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'MT8167CombinedFollowup148'),$false)
            if($failed){$failed.Dispose();throw 'Rejected149 service must remain absent, not armed'}
            $result.peripheral_failed149_service_absent='MT8167CombinedFollowup148=absent'
        }
        if($Plan.peripheral_preserve.service -cin @('MT8167CombinedFollowup124','MT8167CombinedFollowup126')){
            $key=[MT8167OfflineHive.Native]::OpenPath($Root,('ControlSet{0:D3}\Enum\MT6392\Buttons\3&7ce9159&0&0' -f [int]$Plan.control_set),$false)
            if(!$key){throw 'Pinned fresh124 Buttons child missing'}
            try{
                if($key.GetValueNames().Count -gt 64 -or $key.GetValueKind('ConfigFlags') -ne [Microsoft.Win32.RegistryValueKind]::DWord -or $key.GetValue('ConfigFlags') -ne $(if($Plan.peripheral_preserve.service -ceq 'MT8167CombinedFollowup126'){96}else{64}) -or $key.GetValueKind('HardwareID') -ne [Microsoft.Win32.RegistryValueKind]::MultiString){throw 'Unknown unbound keypad shape'}
                $ids=@($key.GetValue('HardwareID'));if($ids.Count -ne 1 -or $ids[0] -cne 'MT6392\Buttons'){throw 'Unknown keypad identity'}
                foreach($name in @('Service','ClassGUID','Driver')){if($name -cin $key.GetValueNames()){throw 'Pinned keypad is already selected; never reinstall'}}
                $result.followup_keypad=[MT8167OfflineHive.Native]::ValueFingerprint($key)
            }finally{$key.Dispose()}
            $legacy=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'Mt6392Keys'),$false);if($legacy){$legacy.Dispose();throw 'Old keypad provider must be absent, never overlap scanner owners'}
        }

    }
    return $result
}
# Named observations remain distinct from full root/subtree fingerprints.
function Peripheral-CapturedValues($Root,$ControlSet,$Expected) {
    if(@($Expected).Count -notin @(10,15,20)){throw 'Exactly10 or15 captured named values required'}
    $result=[ordered]@{};$seen=@{}
    foreach($v in $Expected){
        if($v.key -ceq 'SYSTEM\CurrentControlSet\Services\MT8167CombinedFollowup146' -or (@($Expected).Count -in @(15,20) -and $v.key -ceq 'SYSTEM\CurrentControlSet\Services\MT8167CombinedFollowup150') -or (@($Expected).Count -eq 20 -and $v.key -ceq 'SYSTEM\CurrentControlSet\Services\MT8167CombinedFollowup152')){
            if($v.name -cnotin @('Start','ImagePath','Type','ObjectName','DependOnService')){throw 'Unknown prior-service value'}
        }elseif($v.key -ceq 'SYSTEM\CurrentControlSet\Enum\ROOT\SYSTEM\0001'){
            if($v.name -cnotin @('Service','ClassGUID','Driver','HardwareID','ConfigFlags')){throw 'Unknown BT-root value'}
        }else{throw 'Unknown captured registry key'}
        $identity=$v.key+'\'+$v.name;if($seen.ContainsKey($identity)){throw 'Duplicate named observation'};$seen[$identity]=$true
        if($v.type -notin @(1,2,4,7) -or $v.raw_hex -cnotmatch '^([0-9a-f]{2}){1,4096}$'){throw 'Unknown named observation type/length'}
        $path=('ControlSet{0:D3}\' -f [int]$ControlSet)+$v.key.Substring('SYSTEM\CurrentControlSet\'.Length)
        $k=[MT8167OfflineHive.Native]::OpenPath($Root,$path,$false);if(!$k){throw 'Captured key missing'}
        try{$hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,$v.name,[uint32]$v.type,[uint32]($v.raw_hex.Length/2)).ToLowerInvariant();if($hex -cne $v.raw_hex){throw 'Captured named value changed'};$result[$identity]=[ordered]@{type=$v.type;bytes=($hex.Length/2);raw_hex=$hex}}finally{$k.Dispose()}
    }
    return $result
}
function Check-RelatedFingerprints($Actual,$Expected) {
    $relatedNames=@('select','device_install','rpc_ss');if($Expected.engine_service){$relatedNames+=@('engine_service','engine_ledger')}
    if($Expected.single_gpu_ledger){$relatedNames+='single_gpu_ledger'}
    if($Expected.peripheral_old_services){$relatedNames+='peripheral_old_services'}
    if($Expected.peripheral_failed149_service_absent){$relatedNames+='peripheral_failed149_service_absent'}
    if($Expected.peripheral_captured_values){$relatedNames+='peripheral_captured_values'}
    if($Expected.followup_keypad){$relatedNames+='followup_keypad'}
    foreach($name in $relatedNames) {
        if($Actual.$name -cne $Expected.$name) { throw ('Related existing settings changed: '+$name) }
    }
}
function Get-GpuVersion([string]$Value) {
    if($Value -notmatch '^(\d{1,2}/\d{1,2}/\d{4}),(\d+)\.(\d+)\.(\d+)\.(\d+)$') { throw 'GPU DriverVer must contain a date and four version components' }
    $date=[DateTime]::ParseExact($Matches[1],'M/d/yyyy',[Globalization.CultureInfo]::InvariantCulture)
    $parts=@([int]$Matches[2],[int]$Matches[3],[int]$Matches[4],[int]$Matches[5])
    foreach($part in $parts) { if($part -lt 0 -or $part -gt 65535) { throw 'GPU version component outside UINT16' } }
    return @{date=$date;version=(New-Object Version($parts[0],$parts[1],$parts[2],$parts[3]));driver_ver=($date.ToString('MM/dd/yyyy')+','+($parts -join '.'))}
}
function Get-GpuBindingHash($Binding) {
    $hash=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes(($Binding|ConvertTo-Json -Depth 8 -Compress))))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}
function Read-GpuCandidateBinding([string]$Directory,[string]$ManifestSha256,[string]$SourceCommit) {
    if($ManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $SourceCommit -notmatch '^[0-9a-fA-F]{40}$') { throw 'GPU input requires explicit full manifest SHA256 and hexadecimal source commit' }
    $directory=(Resolve-Path -LiteralPath $Directory).Path
    if([IO.File]::GetAttributes($directory) -band [IO.FileAttributes]::ReparsePoint) { throw 'GPU candidate directory must not be a reparse point' }
    $manifestPath=Join-Path $directory 'signed-manifest.json'
    if((Hash-File $manifestPath) -cne $ManifestSha256.ToLowerInvariant()) { throw 'GPU signed manifest SHA256 mismatch' }
    $signed=Get-Content -Raw -LiteralPath $manifestPath|ConvertFrom-Json
    $source=$SourceCommit.ToLowerInvariant()
    if($signed.source_commit -cne $source -or $signed.native_clear_activate -ne $true -or $signed.signing_thumbprint -notmatch '^[0-9a-fA-F]{40}$') { throw 'GPU candidate source/activation/signing identity mismatch' }
    $allowed=@('mt8167-ge8300.inf','mt8167-ge8300.sys','mt8167-ge8300.cat','ge8300-mips.fw','build-config.json','provider-package-manifest.json','mt8167-development.cer')
    $files=[ordered]@{}
    foreach($item in $signed.files) {
        $name=[string]$item.path
        if($name -cnotin $allowed -or $files.Contains($name) -or $item.sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU manifest must contain only the fixed unique package files' }
        $path=Join-Path $directory $name
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint)) { throw 'GPU package file absent or reparse' }
        $length=(Get-Item -LiteralPath $path).Length
        if($length -le 0 -or $length -gt 16777216 -or (Hash-File $path) -cne $item.sha256.ToLowerInvariant()) { throw ('GPU final package hash/size mismatch: '+$name) }
        $files[$name]=[ordered]@{sha256=$item.sha256.ToLowerInvariant();bytes=$length}
    }
    if($files.Count -ne $allowed.Count) { throw 'GPU final signed manifest is incomplete' }
    $provider=Get-Content -Raw -LiteralPath (Join-Path $directory 'provider-package-manifest.json')|ConvertFrom-Json
    $config=Get-Content -Raw -LiteralPath (Join-Path $directory 'build-config.json')|ConvertFrom-Json
    if($provider.source_commit -cne $source -or $config.source_commit -cne $source -or $provider.fingerprint_state -cne 'final-signed-package' -or $provider.signature_status -cne 'Valid' -or $provider.signer_thumbprint -cne $signed.signing_thumbprint) { throw 'GPU final-signed provider/config provenance mismatch' }
    $cert=[IO.File]::ReadAllBytes((Join-Path $directory 'mt8167-development.cer'))
    $hasher=[Security.Cryptography.SHA1]::Create()
    try { $thumb=([BitConverter]::ToString($hasher.ComputeHash($cert))).Replace('-','') } finally { $hasher.Dispose() }
    if($thumb -ine $signed.signing_thumbprint) { throw 'GPU public certificate thumbprint mismatch' }
    if(@($provider.files.PSObject.Properties.Name).Count -ne 4) { throw 'GPU provider manifest must name exactly four package files' }
    foreach($name in $allowed[0..3]) {
        $entry=$provider.files.$name
        if(!$entry -or $entry.sha256.ToLowerInvariant() -cne $files[$name].sha256 -or $entry.bytes -ne $files[$name].bytes) { throw 'GPU provider and signed file records differ' }
    }
    if($config.installed_sys_sha256.ToLowerInvariant() -cne $provider.selected_installed_sys_sha256.ToLowerInvariant() -or $config.installed_driver_ver -cne $provider.selected_installed_driver_ver) { throw 'GPU build config baseline differs from final provider metadata' }
    $inf=[IO.File]::ReadAllText((Join-Path $directory 'mt8167-ge8300.inf'))
    if($inf -notmatch '(?im)^DriverVer\s*=\s*([^\r\n;]+)') { throw 'GPU INF DriverVer is missing' }
    $candidate=Get-GpuVersion $Matches[1].Trim()
    $baseline=Get-GpuVersion ([string]$provider.selected_installed_driver_ver)
    if($candidate.version.CompareTo($baseline.version) -le 0 -or $candidate.date -lt $baseline.date) { throw 'GPU candidate must have a higher version and non-older date than the declared installed baseline' }
    if($provider.driver_ver -cne $candidate.driver_ver -or $config.driver_ver -cne $candidate.driver_ver -or $config.inf_sha256.ToLowerInvariant() -cne $files['mt8167-ge8300.inf'].sha256 -or $provider.selected_installed_sys_sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU candidate/baseline version metadata mismatch' }
    if($inf -notmatch '(?im)^%Device%\s*=\s*Install\s*,\s*ACPI\\MOU0006\s*$' -or
       $inf -notmatch '(?im)^AddService\s*=\s*MT8167GE8300\s*,' -or
       $inf -notmatch '(?im)^ServiceBinary\s*=\s*%12%\\mt8167-ge8300\.sys\s*$' -or
       $inf -match '(?i)\b(CoInstallers32|RegisterDlls|UnregisterDlls|RunPreSetupCommands|RunPostSetupCommands)\b') { throw 'GPU INF must remain the fixed MOU0006 provider without executable directives' }
    if((Hash-File $manifestPath) -cne $ManifestSha256.ToLowerInvariant()) { throw 'GPU signed manifest changed during inspection' }
    foreach($name in $allowed) { if((Hash-File (Join-Path $directory $name)) -cne $files[$name].sha256) { throw 'GPU package changed during inspection' } }
    if($files['mt8167-ge8300.sys'].sha256 -ceq $provider.selected_installed_sys_sha256.ToLowerInvariant()) { throw 'GPU candidate SYS must differ from the installed baseline' }
    $transitionHasher=[Security.Cryptography.SHA256]::Create()
    try { $transition=([BitConverter]::ToString($transitionHasher.ComputeHash([Text.Encoding]::ASCII.GetBytes($source+$provider.selected_installed_sys_sha256.ToLowerInvariant()+$files['mt8167-ge8300.sys'].sha256+$files['mt8167-ge8300.inf'].sha256)))).Replace('-','').ToLowerInvariant() } finally { $transitionHasher.Dispose() }
    $suffix='\MT8167GpuUpdate-'+$source+'-'+$transition
    return [ordered]@{source_commit=$source;transition_id=$transition;data_directory_suffix=$suffix;data_directory=('C:\Windows\System32'+$suffix);hardware_id='ACPI\MOU0006';instance_id='ACPI\MOU0006\0';driver_service='MT8167GE8300';
        driver_ver=$candidate.driver_ver;installed_baseline_driver_ver=$baseline.driver_ver;
        installed_baseline_sys_sha256=$provider.selected_installed_sys_sha256.ToLowerInvariant();
        signed_manifest_sha256=$ManifestSha256.ToLowerInvariant();signer_thumbprint=$thumb;files=$files;
        signature_trust_verified_by_native=$false;live_loaded_image_verified=$false}
}
function Read-GpuInstalledSelection($Root,$Plan) {
    $control='ControlSet{0:D3}' -f [int]$Plan.control_set
    $key=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'\Enum\ACPI\MOU0006\0'),$false)
    if(!$key) { throw 'Fixed MOU0006 instance is missing from replayed SYSTEM' }
    try {
        if($key.GetValueKind('HardwareID') -ne [Microsoft.Win32.RegistryValueKind]::MultiString -or @($key.GetValue('HardwareID')) -inotcontains 'ACPI\MOU0006' -or
           $key.GetValueKind('Service') -ne [Microsoft.Win32.RegistryValueKind]::String -or $key.GetValue('Service') -ine 'MT8167GE8300' -or
           $key.GetValueKind('Driver') -ne [Microsoft.Win32.RegistryValueKind]::String) { throw 'Fixed GPU hardware/service registration differs' }
        $driver=[string]$key.GetValue('Driver')
        if($driver -notmatch '^\{4d36e97d-e325-11ce-bfc1-08002be10318\}\\[0-9]{4}$') { throw 'GPU selected class key is not the fixed System class' }
    } finally { $key.Dispose() }
    $class=[MT8167OfflineHive.Native]::OpenPath($Root,($control+'\Control\Class\'+$driver),$false)
    if(!$class) { throw 'GPU selected class registration is absent' }
    try {
        foreach($name in @('InfPath','DriverVersion','DriverDate')) { if($class.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::String) { throw 'GPU class metadata type mismatch' } }
        $inf=[string]$class.GetValue('InfPath');$version=[string]$class.GetValue('DriverVersion');$date=[string]$class.GetValue('DriverDate')
        if($inf -notmatch '^oem[0-9]+\.inf$' -and $inf -cne 'mt8167-ge8300.inf') { throw 'GPU selected INF is neither an OEM package nor the fixed GPU INF basename' }
        $parsedDate=[DateTime]::ParseExact($date,[string[]]@('M-d-yyyy','MM-dd-yyyy','M/d/yyyy','MM/dd/yyyy','yyyy-MM-dd'),[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::None)
        $installed=Get-GpuVersion ($parsedDate.ToString('MM/dd/yyyy')+','+$version)
        return [ordered]@{instance_id='ACPI\MOU0006\0';driver_class_key=$driver;inf_path=$inf;driver_ver=$installed.driver_ver;
            class_fingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($class);replayed_system_only=$true;live_loaded_image_verified=$false}
    } finally { $class.Dispose() }
}
function Check-GpuCandidateProgress($Binding,$Selection) {
    $candidate=Get-GpuVersion ([string]$Binding.driver_ver)
    $installed=Get-GpuVersion ([string]$Selection.driver_ver)
    if($Binding.installed_baseline_driver_ver -cne $installed.driver_ver -or $candidate.version.CompareTo($installed.version) -le 0 -or $candidate.date -lt $installed.date) { throw 'GPU candidate is not a roll-forward from the replayed installed selection' }
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
            if($plan.peripheral_preserve){$state.audit_scope='Select/DeviceInstall/RpcSs + dedicated peripheral service; fixed old service bounded subtree fingerprints'}
            $before=Related-Fingerprints $root $plan
            if($plan.gpu_candidate_binding) {
                $state.audit_scope='Select + DeviceInstall/RpcSs + exact selected service seven values; fixed MOU0006 Enum and selected System-class key read-only'
                $state.gpu_installed_selection=Read-GpuInstalledSelection $root $plan
                Check-GpuCandidateProgress $plan.gpu_candidate_binding $state.gpu_installed_selection
                if($plan.gpu_installed_selection -and (Get-GpuBindingHash $state.gpu_installed_selection) -cne (Get-GpuBindingHash $plan.gpu_installed_selection)) { throw 'GPU replayed selection changed from the pinned inspection' }
                $state.gpu_candidate_binding_sha256=Get-GpuBindingHash $plan.gpu_candidate_binding
                $state.gpu_policy_sha256=$plan.gpu_candidate_binding.policy_sha256
            }
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
$peripheralPreserve=$null
if($ServiceName -cin @('MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154')){
    if($IncludeHardwareProbe -or $RearmEngineProbe -or $RearmFixedGpuDeviceInstall -or $InspectFixedGpuDeviceInstall -or $EnginePreservationPlanPath -or !$PeripheralPreservationPlanPath -or $ExpectedPeripheralPreservationPlanSha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Peripheral-only explicit preservation plan required'}
    if((Hash-File $PeripheralPreservationPlanPath) -cne $ExpectedPeripheralPreservationPlanSha256){throw 'Peripheral preservation plan pin mismatch'}
    $peripheralPreserve=Get-Content $PeripheralPreservationPlanPath -Raw|ConvertFrom-Json
    if($ServiceName -ceq 'MT8167CombinedFollowup154'){
        if($peripheralPreserve.profile -cne 'combined-features-fresh154-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'a26e82c937733617d932bb92201a593885575f27e89f26791612f791ca17b8da' -or $peripheralPreserve.recovery_boot_id -cne '9d795d5d-8d58-47a2-b208-e5c0149ecd19' -or $peripheralPreserve.capture_selection.capture_number -ne 154 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.deferred_unbound_mask -ne 0){throw 'Exact sealed fresh148 run149 contract required'}
        if((@($peripheralPreserve.stage_order) -join ',') -cne 'rtc,thermal,usb,wifi,sd,display,parent,keypad,battery,audio,render,msdc' -or (@($peripheralPreserve.install_targets) -join ',') -cne 'parent,render' -or $peripheralPreserve.gpu_client_execute -ne $true -or $peripheralPreserve.gpu_client_count -ne 1 -or $peripheralPreserve.bt_first_install_execute -ne $false -or @($peripheralPreserve.diagnostic_clients).Count -ne 2 -or $peripheralPreserve.diagnostic_clients[0].name -cne 'thermal_status' -or $peripheralPreserve.diagnostic_clients[1].name -cne 'gpu_suite' -or $peripheralPreserve.diagnostic_clients[0].phase -ne 0 -or $peripheralPreserve.diagnostic_clients[1].phase -ne 1 -or (@($peripheralPreserve.captured_registry_values).Count -ne 20)){throw '12 targets/GPU0/retained BT/5+5 named values required'}
        if(@($peripheralPreserve.service_preservation.PSObject.Properties).Count -ne 18){throw '18 original full service fingerprints required'}
        if($peripheralPreserve.boot_transition.boot_write_required -ne $true -or $peripheralPreserve.boot_transition.boot_sha256 -cne '2e6cba6ab96cd5b586cefbd4bd004c6cee0484866ca6710ea454144da568c451' -or $peripheralPreserve.packages.parent.hashes.sys -cne 'bbced952c499ba0588c77b861b20141043929773d703cdd327ffa92e61990979' -or $peripheralPreserve.packages.render.hashes.sys -cne 'e884f33652078618ead6e02faaeccf606e15e9d3074e0cffabe681c8eb1db898'){throw 'Matched parent1117 boot2e6 GPU343 tuple required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup152'){
        if($peripheralPreserve.profile -cne 'combined-features-fresh152-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'fee4081fc3842390e854753aa6b2ce4659fc5b58645246c406ed205679c55ba3' -or $peripheralPreserve.recovery_boot_id -cne '72ea1a1d-06b9-4671-88d2-f957198f3362' -or $peripheralPreserve.capture_selection.capture_number -ne 152 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.deferred_unbound_mask -ne 0){throw 'Exact sealed fresh148 run149 contract required'}
        if((@($peripheralPreserve.stage_order) -join ',') -cne 'rtc,thermal,usb,wifi,sd,display,parent,keypad,battery,audio,render,msdc' -or (@($peripheralPreserve.install_targets) -join ',') -cne 'render' -or $peripheralPreserve.gpu_client_execute -ne $true -or $peripheralPreserve.gpu_client_count -ne 1 -or $peripheralPreserve.bt_first_install_execute -ne $false -or @($peripheralPreserve.diagnostic_clients).Count -ne 1 -or $peripheralPreserve.diagnostic_clients[0].name -cne 'gpu_suite' -or $peripheralPreserve.diagnostic_clients[0].phase -ne 1 -or (@($peripheralPreserve.captured_registry_values).Count -ne 15)){throw '12 targets/GPU0/retained BT/5+5 named values required'}
        if(@($peripheralPreserve.service_preservation.PSObject.Properties).Count -ne 18){throw '18 original full service fingerprints required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup150'){
        if($peripheralPreserve.profile -cne 'combined-features-fresh150-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'ce6c67d6fc90c8ddae99067700efa4aba3da0e1e6c72136a5ab72062deb1c4c6' -or $peripheralPreserve.recovery_boot_id -cne '161b63f1-d476-462e-9bfd-16bddfa09911' -or $peripheralPreserve.capture_selection.capture_number -ne 150 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.deferred_unbound_mask -ne 0){throw 'Exact sealed fresh148 run149 contract required'}
        if((@($peripheralPreserve.stage_order) -join ',') -cne 'rtc,thermal,usb,wifi,sd,display,parent,keypad,battery,audio,render,msdc' -or (@($peripheralPreserve.install_targets) -join ',') -cne 'parent,thermal,msdc' -or $peripheralPreserve.gpu_client_execute -ne $false -or $peripheralPreserve.gpu_client_count -ne 0 -or $peripheralPreserve.bt_first_install_execute -ne $false -or @($peripheralPreserve.diagnostic_clients).Count -ne 2 -or $peripheralPreserve.diagnostic_clients[0].name -cne 'thermal_status' -or $peripheralPreserve.diagnostic_clients[1].name -cne 'audio_consumer' -or @($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -ne 0}).Count -ne 0 -or (@($peripheralPreserve.captured_registry_values).Count -ne 10)){throw '12 targets/GPU0/retained BT/5+5 named values required'}
        if(@($peripheralPreserve.service_preservation.PSObject.Properties).Count -ne 18){throw '18 original full service fingerprints required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup148'){
        if($peripheralPreserve.profile -cne 'combined-features-fresh148-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '38d60c8d9f84a08e0c0284505fee17b0e3b32b56060a6a030643bd78f671f6df' -or $peripheralPreserve.recovery_boot_id -cne '5b5b69d3-70d1-4100-927b-e29eb8bd3026' -or $peripheralPreserve.capture_selection.capture_number -ne 148 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.deferred_unbound_mask -ne 0){throw 'Exact sealed fresh148 run149 contract required'}
        if((@($peripheralPreserve.stage_order) -join ',') -cne 'rtc,thermal,usb,wifi,sd,display,parent,keypad,battery,audio,render,msdc' -or (@($peripheralPreserve.install_targets) -join ',') -cne 'parent,thermal,msdc' -or $peripheralPreserve.gpu_client_execute -ne $false -or $peripheralPreserve.gpu_client_count -ne 0 -or $peripheralPreserve.bt_first_install_execute -ne $false -or @($peripheralPreserve.diagnostic_clients).Count -ne 2 -or $peripheralPreserve.diagnostic_clients[0].name -cne 'thermal_status' -or $peripheralPreserve.diagnostic_clients[1].name -cne 'audio_consumer' -or @($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -ne 0}).Count -ne 0 -or (@($peripheralPreserve.captured_registry_values).Count -ne 10)){throw '12 targets/GPU0/retained BT/5+5 named values required'}
        if(@($peripheralPreserve.service_preservation.PSObject.Properties).Count -ne 18){throw '18 original full service fingerprints required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup146'){

        if($peripheralPreserve.profile -cne 'combined-features-fresh146-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '7759162af34e8c440f28155d8d24acd77469f8f3911528eb62ee382b95c15392' -or $peripheralPreserve.recovery_boot_id -cne 'f5fd8256-eba5-463c-8f34-0951330c4ee9' -or $peripheralPreserve.capture_selection.capture_number -ne 146 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh142 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'audio,render' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $true -or $peripheralPreserve.gpu_client_count -ne 1 -or @($peripheralPreserve.diagnostic_clients).Count -ne 2 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 1)){throw 'Exact audio1033/WiFi1021/render DIF and one final GPU client required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup144'){
        if($peripheralPreserve.profile -cne 'combined-features-fresh144-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '0faa9fd09cce293d411ad025e64e54c27d5975948b54935c00e25a3feff15a60' -or $peripheralPreserve.recovery_boot_id -cne 'c35ceece-d15b-4e07-98f9-5f1d2accd64f' -or $peripheralPreserve.capture_selection.capture_number -ne 144 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh142 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140','MT8167CombinedFollowup142')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'thermal,wifi,render' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $true -or $peripheralPreserve.gpu_client_count -ne 1 -or @($peripheralPreserve.diagnostic_clients).Count -ne 1 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 1)){throw 'Exact audio1033/WiFi1021/render DIF and one final GPU client required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup142'){
        if($peripheralPreserve.profile -cne 'combined-audio-classfix-fresh142-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'f5ee192fd738b6db129064e71e43bc0a80bf35c6341e0177d468e3bbd6283e37' -or $peripheralPreserve.recovery_boot_id -cne '1eb91bc2-cb82-4433-85f1-a70b3570529e' -or $peripheralPreserve.capture_selection.capture_number -ne 142 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh142 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'audio,wifi,render' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $true -or $peripheralPreserve.gpu_client_count -ne 1 -or @($peripheralPreserve.diagnostic_clients).Count -ne 6 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 1)){throw 'Exact audio1033/WiFi1021/render DIF and one final GPU client required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup140'){
        if($peripheralPreserve.profile -cne 'combined-peripherals-fresh140-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'af6df53daf6b70248e88007bd23642a43b09e28ca240bee526bf00674a194bc3' -or $peripheralPreserve.recovery_boot_id -cne '0fb71a3a-9ab6-491a-be62-ff3f46f5843d' -or $peripheralPreserve.capture_selection.capture_number -ne 140 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh140 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'parent,audio' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $false -or $peripheralPreserve.gpu_client_count -ne 0 -or @($peripheralPreserve.diagnostic_clients).Count -ne 5 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 0)){throw 'Exact parent1115/audio1033 DIF and five non-GPU clients required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup138'){
        if($peripheralPreserve.profile -cne 'combined-peripherals-fresh138-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'db49ed4d7b1759541f6021acd4d1f03b531fe4919f97b4d701d7b329f09ccd15' -or $peripheralPreserve.recovery_boot_id -cne 'bb058389-f139-4728-b90a-47b2cbe20a8f' -or $peripheralPreserve.capture_selection.capture_number -ne 138 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh138 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'parent,audio' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $false -or $peripheralPreserve.gpu_client_count -ne 0 -or @($peripheralPreserve.diagnostic_clients).Count -ne 5 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 0)){throw 'Exact parent1115/audio1033 DIF and five non-GPU clients required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup136'){
        if($peripheralPreserve.profile -cne 'combined-gpu335-fresh136-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'bdd612ee0914d2ac853100c89f856534eb4acca198d61a24e6660a257247d0c7' -or $peripheralPreserve.recovery_boot_id -cne '07b0b9b6-fb4e-49d6-af7a-6695d97652d1' -or $peripheralPreserve.capture_selection.capture_number -ne 136 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh136 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'render' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $true -or @($peripheralPreserve.diagnostic_clients).Count -ne 5 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 1)){throw 'Exact332 render-only install and one final GPU client contract required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup134'){
        if($peripheralPreserve.profile -cne 'combined-gpu332-fresh134-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '9d4b078f989a589166ad7ef53a211251628e6d32a93c8c66fe4858233f29bee2' -or $peripheralPreserve.recovery_boot_id -cne '3d353a0c-b6e9-4697-957d-aa2b8b82cbe7' -or $peripheralPreserve.capture_selection.capture_number -ne 134 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh134 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','Mt6392Keys')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if((@($peripheralPreserve.install_targets) -join ',') -cne 'render' -or $peripheralPreserve.diagnostic_only -ne $false -or $peripheralPreserve.gpu_client_execute -ne $true -or @($peripheralPreserve.diagnostic_clients).Count -ne 5 -or (@($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -eq 1}).Count -ne 1)){throw 'Exact332 render-only install and one final GPU client contract required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup132'){
        if($peripheralPreserve.profile -cne 'combined-rtc-afe-fresh132-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '3bb3a33715675aa1382f91f13e328eb6f5690a0da66b77996ae81da153b18159' -or $peripheralPreserve.recovery_boot_id -cne '0623766b-23c8-4c54-b256-e874278087e3' -or $peripheralPreserve.capture_selection.capture_number -ne 132 -or $peripheralPreserve.planned_reboots -ne 0 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh132 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','Mt6392Keys')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}

        if(@($peripheralPreserve.install_targets).Count -ne 0 -or !$peripheralPreserve.diagnostic_only -or $peripheralPreserve.gpu_client_execute -ne $false -or @($peripheralPreserve.diagnostic_clients).Count -ne 4 -or @($peripheralPreserve.diagnostic_clients|Where-Object {$_.phase -ne 0}).Count){throw 'Readonly132 no-DIF/no-GPU exact four-client contract required'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup130'){
        if($peripheralPreserve.profile -cne 'combined-peripheral-gpu327-fresh130-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne '9f4e3602192cd79cd1d389e864b5745ba169e60b1c52643af9f7dfe1bc2f0447' -or $peripheralPreserve.recovery_boot_id -cne '216e0c46-af74-4015-b9f3-3f2271d81ec0' -or $peripheralPreserve.capture_selection.capture_number -ne 130 -or $peripheralPreserve.planned_reboots -ne 0 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh130 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','Mt6392Keys')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup128'){
        if($peripheralPreserve.profile -cne 'combined-peripheral-gpu327-fresh128-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'bed13bd516128dd8df75a41e7bd68ecb3c5770f8394c73a70cceb7ced260a99d' -or $peripheralPreserve.recovery_boot_id -cne '336b05be-a058-484b-b5fb-5c8e5842882e' -or $peripheralPreserve.capture_selection.capture_number -ne 128 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh128 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','Mt6392Keys')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup126'){
        if($peripheralPreserve.profile -cne 'combined-peripheral-gpu327-fresh126-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'a07925d0d312500265983fbef8d550175a80a9eb91115b6b479fc4368a579102' -or $peripheralPreserve.recovery_boot_id -cne 'bd1dc7c9-1b01-4b56-a7cf-a9cbd5500e55' -or $peripheralPreserve.capture_selection.capture_number -ne 126 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 0 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh126 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}
    }elseif($ServiceName -ceq 'MT8167CombinedFollowup124'){
        if($peripheralPreserve.profile -cne 'combined-peripheral-gpu327-fresh124-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.backup_manifest_sha256 -cne 'aef54b0e2d3d70c083626c76d1c18cfe16b1eff8ecd6237233e83e257bd801da' -or $peripheralPreserve.recovery_boot_id -cne '0adf9f8e-8773-47b3-8d2d-e8440426f9e7' -or $peripheralPreserve.capture_selection.capture_number -ne 124 -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.first_install_mask -ne 128 -or $peripheralPreserve.battery_baseline_sys_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $peripheralPreserve.battery_baseline_sys_sha256 -ceq ('0'*64)){throw 'Final sealed exact fresh124 followup contract required; draft cannot arm'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122')|Sort-Object) -join ',')){throw 'All seven old services must be pinned'}
    }elseif($ServiceName -ceq 'MT8167PeripheralNext122'){
        if($peripheralPreserve.profile -cne 'peripheral-usb1010-wifi1020-sd8-dod17-parent1102-next-v1' -or $peripheralPreserve.service -cne $ServiceName -or $peripheralPreserve.data_directory -cne $ServiceName -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.planned_reboots -ne 1 -or $peripheralPreserve.capture_selection.capture_number -lt 122 -or $peripheralPreserve.main_approval_sha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Explicit approved capture122+ next peripheral contract required'}
        $names=@($peripheralPreserve.service_preservation.PSObject.Properties.Name|Sort-Object)
        if(($names -join ',') -cne ((@('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch')|Sort-Object) -join ',')){throw 'All six old service snapshots required'}
    } else {
    if($peripheralPreserve.profile -cne 'peripheral-usb1009-mma110-emmc19-parent1101-v1' -or $peripheralPreserve.draft -ne $false -or $peripheralPreserve.normal_install_allowed -ne $true -or $peripheralPreserve.service -cne 'MT8167PeripheralBatch' -or $peripheralPreserve.data_directory -cne 'MT8167PeripheralBatch' -or $peripheralPreserve.planned_reboots -ne 1){throw 'Final main-selected peripheral-only profile required'}
    }
    $selection=$peripheralPreserve.capture_selection
    if($selection.schema -ne 1 -or $selection.profile -cne $peripheralPreserve.profile -or $selection.capture_number -isnot [int] -or $selection.capture_number -lt 114 -or $selection.manifest_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $selection.manifest_sha256 -cne $peripheralPreserve.backup_manifest_sha256 -or $selection.recovery_boot_id -cnotmatch '^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$' -or $selection.recovery_boot_id -cne $peripheralPreserve.recovery_boot_id -or $selection.gpu_policy_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $selection.unchanged_since_capture -ne $true -or $peripheralPreserve.backup_manifest -notmatch ('private-windows-rt-prebackup-run'+$selection.capture_number+'/manifest.json$')){throw 'Explicit capture114+ manifest/policy/boot binding required'}
    if($peripheralPreserve.source_files.SYSTEM.sha256 -cne $ExpectedSystemSha256 -or $peripheralPreserve.source_files.'SYSTEM.LOG1'.sha256 -cne $ExpectedLog1Sha256 -or $peripheralPreserve.source_files.'SYSTEM.LOG2'.sha256 -cne $ExpectedLog2Sha256){throw 'Selected capture trio pins required'}
} elseif($PeripheralPreservationPlanPath -or $ExpectedPeripheralPreservationPlanSha256){throw 'Peripheral plan belongs only to its dedicated service'}
$enginePreservePlan=$null
if($ServiceName -ceq 'MT8167RenderSingleGpu') {
    if($IncludeHardwareProbe -or $RearmEngineProbe -or $RearmFixedGpuDeviceInstall -or $InspectFixedGpuDeviceInstall -or !$EnginePreservationPlanPath -or $ExpectedEnginePreservationPlanSha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Single GPU requires explicit Engine-preservation-only plan'}
    if((Hash-File $EnginePreservationPlanPath) -cne $ExpectedEnginePreservationPlanSha256){throw 'Engine preservation plan hash mismatch'}
    $enginePreservePlan=Get-Content $EnginePreservationPlanPath -Raw|ConvertFrom-Json
    if($enginePreservePlan.schema -ne 1 -or $enginePreservePlan.profile -cne 'render-single-gpu321-run111-v1' -or $enginePreservePlan.backup_manifest_sha256 -cne 'fc2a8277510f8836ca1d74efd15939e48a6dbe734b37cf31510448ceb6efa9d9'){throw 'Fixed fresh110 native preservation required'}
    $sgPlan=$enginePreservePlan.single_gpu_rearm
    if($sgPlan.schema -ne 1 -or $sgPlan.service -cne 'MT8167RenderSingleGpu' -or $sgPlan.maximum_rearms -ne 1 -or $sgPlan.data_directory -cne 'MT8167RenderSingleGpuRun111' -or @($sgPlan.diagnostic_policies).Count -ne 7 -or $sgPlan.diagnostic_policies[0] -cne 'd64c9165297bb602ac04be1d9053998c2f6f8da3581545b6be8b005d237fcdb5' -or $sgPlan.diagnostic_policies[1] -cne '6b00fa9f57696f19ad10ecdace497d0a3b1b4bcb861e98d7a8571e0ce23f19c3' -or $sgPlan.diagnostic_policies[2] -cne '10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64' -or $sgPlan.diagnostic_policies[3] -cne 'b6273b09f165e3e5fcc61d49bc8b874008f9350d30592737c71e31a2999e905f' -or $sgPlan.diagnostic_policies[4] -cne 'bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0' -or $sgPlan.diagnostic_policies[5] -cne '9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b' -or $sgPlan.diagnostic_policies[6] -cne 'a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea' -or @($sgPlan.partial_diagnostic_policies).Count -ne 4 -or $sgPlan.partial_diagnostic_policies[0] -cne '10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64' -or $sgPlan.partial_diagnostic_policies[1] -cne 'bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0' -or $sgPlan.partial_diagnostic_policies[2] -cne '9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b' -or $sgPlan.partial_diagnostic_policies[3] -cne 'a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea'){throw 'Fixed run97/run99/run101 to run111 rearm context required'}
} elseif($EnginePreservationPlanPath -or $ExpectedEnginePreservationPlanSha256){throw 'Engine preserve plan is single-GPU only'}
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
$gpuCandidateBinding=$null;$gpuCandidateBindingHash=$null
if($InspectFixedGpuDeviceInstall -or $RearmFixedGpuDeviceInstall) {
    if(!$GpuCandidateDirectory -or !$ExpectedGpuCandidateManifestSha256 -or !$GpuCandidateSourceCommit -or $ExpectedGpuPolicySha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU native operations require the final signed candidate directory, manifest SHA256, hexadecimal source and signed-installer policy SHA256 pin' }
    $gpuCandidateBinding=Read-GpuCandidateBinding $GpuCandidateDirectory $ExpectedGpuCandidateManifestSha256 $GpuCandidateSourceCommit
    $gpuCandidateBinding['policy_sha256']=$ExpectedGpuPolicySha256.ToLowerInvariant()
    $gpuCandidateBinding['policy_hash_source']='caller pin from signed installer metadata; host must crosslink'
    $gpuCandidateBindingHash=Get-GpuBindingHash $gpuCandidateBinding
} elseif($GpuCandidateDirectory -or $ExpectedGpuCandidateManifestSha256 -or $GpuCandidateSourceCommit -or $ExpectedGpuPolicySha256) { throw 'GPU candidate input is allowed only with explicit GPU inspect/rearm' }
$gpuRearmPlan=$null;$gpuRearmPlanHash=$null
if($RearmFixedGpuDeviceInstall) {
    if($RearmEngineProbe -or $ServiceName -cne 'MT8167DeviceInstall' -or $IncludeHardwareProbe -or !$GpuRearmPlanPath -or
       $ExpectedGpuRearmPlanSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'GPU-only rearm requires a pinned native plan' }
    $gpuRearmPlanHash=Hash-File $GpuRearmPlanPath
    if($gpuRearmPlanHash -cne $ExpectedGpuRearmPlanSha256.ToLowerInvariant()) { throw 'GPU rearm plan SHA256 mismatch' }
    $gpuRearmPlan=Get-Content -Raw -LiteralPath $GpuRearmPlanPath | ConvertFrom-Json
    if($gpuRearmPlan.schema -ne 2 -or $gpuRearmPlan.service -cne 'MT8167DeviceInstall' -or $gpuRearmPlan.maximum_rearms -ne 1 -or
       $gpuRearmPlan.hardware_id -cne 'ACPI\MOU0006' -or $gpuRearmPlan.instance_id -cne 'ACPI\MOU0006\0' -or
       $gpuRearmPlan.candidate_source_commit -cne $gpuCandidateBinding.source_commit -or
       $gpuRearmPlan.candidate_driver_ver -cne $gpuCandidateBinding.driver_ver -or
       $gpuRearmPlan.policy_sha256 -cne $gpuCandidateBinding.policy_sha256 -or
       $gpuRearmPlan.transition_id -cne $gpuCandidateBinding.transition_id -or
       $gpuRearmPlan.data_directory -cne $gpuCandidateBinding.data_directory -or
       $gpuRearmPlan.data_directory_suffix -cne $gpuCandidateBinding.data_directory_suffix -or
       $gpuRearmPlan.candidate_sys_sha256 -cne $gpuCandidateBinding.files.'mt8167-ge8300.sys'.sha256 -or
       $gpuRearmPlan.candidate_cat_sha256 -cne $gpuCandidateBinding.files.'mt8167-ge8300.cat'.sha256 -or
       $gpuRearmPlan.candidate_binding_sha256 -cne $gpuCandidateBindingHash -or
       (Get-GpuBindingHash $gpuRearmPlan.candidate_binding) -cne $gpuCandidateBindingHash -or
       $gpuRearmPlan.installed_selection.instance_id -cne 'ACPI\MOU0006\0' -or
       $gpuRearmPlan.installed_selection.driver_ver -cne $gpuCandidateBinding.installed_baseline_driver_ver -or
       $gpuRearmPlan.installed_selection.class_fingerprint -cnotmatch '^[0-9a-f]{64}$' -or
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
$report=[ordered]@{status='failed-do-not-deploy';whole_system_audit=$false;audit_scope='Select + DeviceInstall/RpcSs raw values + selected service seven values; Engine-only known bounded LastDiagnostic subtree fingerprint when explicitly rearming';source_files=[ordered]@{};timeout_seconds=$TimeoutSeconds;services=$names;errors=@();driver_files_modified=$false}
$plan=[ordered]@{control_set=$ExpectedCurrentControlSet;related_fingerprints='';services=$selected}
if($enginePreservePlan){$plan.engine_preserve=$enginePreservePlan;$report.engine_preservation_plan_sha256=$ExpectedEnginePreservationPlanSha256;$report.single_gpu_rearm=$enginePreservePlan.single_gpu_rearm}
if($engineRearmPlan) {
    $plan.engine_rearm=$engineRearmPlan;$plan.engine_rearm_plan_sha256=$engineRearmPlanHash
    $report.engine_rearm_plan_sha256=$engineRearmPlanHash
}
if($gpuRearmPlan) {
    $plan.gpu_rearm=$gpuRearmPlan;$plan.gpu_rearm_plan_sha256=$gpuRearmPlanHash
    $report.gpu_rearm_plan_sha256=$gpuRearmPlanHash
}
if($InspectFixedGpuDeviceInstall) { $plan.gpu_inspect=$true;$report.service_values_written=$false }
if($gpuCandidateBinding) { $report.audit_scope='Select + DeviceInstall/RpcSs + exact selected service seven values; fixed MOU0006 Enum and selected System-class key read-only';$plan.gpu_candidate_binding=$gpuCandidateBinding;$report.gpu_candidate_binding=$gpuCandidateBinding;$report.gpu_candidate_binding_sha256=$gpuCandidateBindingHash;$report.gpu_policy_sha256=$gpuCandidateBinding.policy_sha256 }
if($gpuRearmPlan) { $plan.gpu_installed_selection=$gpuRearmPlan.installed_selection }
if($peripheralPreserve){$plan.peripheral_preserve=$peripheralPreserve;$report.peripheral_preservation_plan_sha256=$ExpectedPeripheralPreservationPlanSha256;$report.peripheral_old_services_required=$true}
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
    if($enginePreservePlan){foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){
        $pin=$enginePreservePlan.source_files.$name
        if(!$pin -or $pin.sha256 -cne $report.source_files[$name].sha256 -or $pin.bytes -ne $report.source_files[$name].bytes){throw 'Engine preserve plan and fresh input differ'}
    }}
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
    if($engineRearmPlan) {
        $fp=[string]$patch.state.services[0].diagnostic_fingerprint
        if($fp -cne 'absent' -and $fp -cnotmatch '^[0-9a-f]{64}$') { throw 'Missing actual Engine diagnostic fingerprint' }
        $plan.engine_diagnostic_fingerprint=$fp;$report.engine_diagnostic_fingerprint=$fp
    }
    if($InspectFixedGpuDeviceInstall) {
        $fingerprint=[string]$patch.state.gpu_inspected_service_fingerprint
        if($fingerprint -cnotmatch '^[0-9a-f]{64}$') { throw 'Native GPU inspection did not return a real raw fingerprint' }
        $plan.gpu_inspect_fingerprint=$fingerprint
        $plan.gpu_installed_selection=$patch.state.gpu_installed_selection
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
        $gpuPlan=[ordered]@{schema=2;service='MT8167DeviceInstall';maximum_rearms=1;hardware_id='ACPI\MOU0006';instance_id='ACPI\MOU0006\0';
            candidate_source_commit=$gpuCandidateBinding.source_commit;
            candidate_sys_sha256=$gpuCandidateBinding.files.'mt8167-ge8300.sys'.sha256;
            candidate_cat_sha256=$gpuCandidateBinding.files.'mt8167-ge8300.cat'.sha256;
            candidate_driver_ver=$gpuCandidateBinding.driver_ver;policy_sha256=$gpuCandidateBinding.policy_sha256;transition_id=$gpuCandidateBinding.transition_id;data_directory=$gpuCandidateBinding.data_directory;data_directory_suffix=$gpuCandidateBinding.data_directory_suffix;baseline_driver_ver=$gpuCandidateBinding.installed_baseline_driver_ver;baseline_sys_sha256=$gpuCandidateBinding.installed_baseline_sys_sha256;candidate_binding=$gpuCandidateBinding;candidate_binding_sha256=$gpuCandidateBindingHash;
            installed_selection=$check.state.gpu_installed_selection;
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
    if($peripheralPreserve){$report.peripheral_old_services_preserved=$true}
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
