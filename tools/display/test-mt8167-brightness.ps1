#requires -Version 3.0
# Standard integrated-panel WMI client; no driver, DDC/CI or raw MMIO access.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$InstanceName,
    [ValidateRange(1,10)][uint32]$OperationTimeoutSec=3,
    [ValidateRange(1,10)][int]$ReadbackAttempts=4
)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
$prefix=$root.TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar
if(-not $out.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout/deployment root'}
if(Test-Path -LiteralPath $out){throw 'Use a fresh OutputDirectory for each single run'}
New-Item -ItemType Directory -Path $out | Out-Null
$report=[ordered]@{
    schema=1;api='local root\wmi WmiMonitorBrightness/WmiMonitorBrightnessMethods';
    status='initializing';instance_name=$null;original_percent=$null;
    mutation_attempted=$false;restore_attempted=$false;restore_verified=$false;
    raw_pwm_observed=$false;restoration_scope='original standard percent, not original raw PWM bits';readback_kind='WMI CurrentBrightness; raw PWM is not exposed by this API';
    operation_timeout_sec=$OperationTimeoutSec;readback_attempts=$ReadbackAttempts;
    client_sha256=$null;steps=@();error=$null;restore_error=$null
}
# Get-FileHash is PS4+. SHA256.Create also works on Windows PowerShell 3.
$sha=[Security.Cryptography.SHA256]::Create()
try {$report.client_sha256=([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($PSCommandPath)))).Replace('-','').ToLowerInvariant()}
finally {$sha.Dispose()}
function Save-Report {
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'result.json') -Encoding UTF8
}
function Add-Step([string]$Phase,$Requested,$Observed,$ReturnValue) {
    $report.steps+= [ordered]@{
        utc=[DateTime]::UtcNow.ToString('o');phase=$Phase;requested_percent=$Requested;
        current_brightness=$Observed;method_return=$ReturnValue;
        # Prediction only: source 0c73f27 Set() rounds percent*1023/100.
        expected_duty_from_source=$(if($null -ne $Requested){[int][Math]::Floor(([int]$Requested*1023+50)/100)}else{$null})
    }
    Save-Report
}
function Set-Level($Method,[byte]$Level,[string]$Phase) {
    # Named, explicitly typed MOF arguments avoid positional argument ordering.
    $reply=Invoke-CimMethod -InputObject $Method -MethodName WmiSetBrightness `
        -Arguments @{Timeout=[uint32]0;Brightness=[byte]$Level} -OperationTimeoutSec $OperationTimeoutSec -ErrorAction Stop
    if($null -eq $reply -or $null -eq $reply.ReturnValue){throw "$Phase did not return a WMI method status"}
    Add-Step $Phase $Level $null $reply.ReturnValue
    if([uint32]$reply.ReturnValue -ne 0){throw "$Phase WmiSetBrightness returned $($reply.ReturnValue)"}
}
function Read-Level($Monitor,[byte]$Expected,[string]$Phase) {
    for($i=0;$i -lt $ReadbackAttempts;$i++) {
        # Refresh the exact keyed instance; never reuse the initial snapshot.
        $fresh=@(Get-CimInstance -InputObject $Monitor -OperationTimeoutSec $OperationTimeoutSec -ErrorAction Stop)
        if($fresh.Count -ne 1 -or -not $fresh[0].Active -or $fresh[0].InstanceName -ne $report.instance_name -or $null -eq $fresh[0].CurrentBrightness){throw "$Phase monitor disappeared or changed"}
        $value=[int]$fresh[0].CurrentBrightness
        Add-Step $Phase $Expected $value $null
        if($value -eq $Expected){return}
        if($i+1 -lt $ReadbackAttempts){Start-Sleep -Milliseconds 250}
    }
    throw "$Phase readback never matched $Expected percent"
}
$monitor=$null;$method=$null;$original=$null
try {
    $report.status='discovering';Save-Report
    $monitors=@(Get-CimInstance -Namespace root\wmi -ClassName WmiMonitorBrightness -OperationTimeoutSec $OperationTimeoutSec -ErrorAction Stop | Where-Object {$_.Active})
    if($InstanceName){$monitors=@($monitors | Where-Object {$_.InstanceName -eq $InstanceName})}
    if($monitors.Count -ne 1){throw "Expected one active brightness instance, got $($monitors.Count); specify exact InstanceName if ambiguous"}
    $monitor=$monitors[0];$report.instance_name=[string]$monitor.InstanceName
    if(-not $report.instance_name -or $null -eq $monitor.CurrentBrightness){throw 'Missing instance key or initial brightness'}
    $original=[int]$monitor.CurrentBrightness;$report.original_percent=$original
    $levels=@($monitor.Level | ForEach-Object {[int]$_})
    $report['supported_levels']=$levels
    if($levels.Count -ne [int]$monitor.Levels -or $original -lt 1 -or $original -gt 100 -or $levels -notcontains $original -or $levels -notcontains 30 -or $levels -notcontains 70){throw 'Original value and both requested levels must be supported; no rounding or nearest-level substitution'}
    $methods=@(Get-CimInstance -Namespace root\wmi -ClassName WmiMonitorBrightnessMethods -OperationTimeoutSec $OperationTimeoutSec -ErrorAction Stop | Where-Object {$_.Active -and $_.InstanceName -eq $report.instance_name})
    if($methods.Count -ne 1){throw 'Expected exactly one active methods instance with the same InstanceName'}
    $method=$methods[0];Add-Step 'initial' $null $original $null
    # Persist restoration data BEFORE the first potentially effective call.
    $report.mutation_attempted=$true;$report.status='testing';Save-Report
    foreach($level in @(30,70)) {
        Set-Level $method ([byte]$level) "set-$level"
        Read-Level $monitor ([byte]$level) "read-$level"
    }
    $report.status='test-levels-verified'
} catch {
    $report.error=$_.Exception.Message;$report.status='failed'
} finally {
    if($report.mutation_attempted) {
        $report.restore_attempted=$true
        try {
            # Even a timed-out/error Set may have changed hardware: restore.
            Set-Level $method ([byte]$original) 'restore-set'
            Read-Level $monitor ([byte]$original) 'restore-read'
            $report.restore_verified=$true
        } catch {$report.restore_error=$_.Exception.Message;$report.status='failed'}
    }
    if($report.status -eq 'test-levels-verified' -and $report.restore_verified){$report.status='standard-api-verified-and-restored'}
    Save-Report
}
Write-Output (Join-Path $out 'result.json')
if($report.status -ne 'standard-api-verified-and-restored'){throw "Brightness probe failed: $($report.error); restore: $($report.restore_error)"}
