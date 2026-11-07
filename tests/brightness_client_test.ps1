# Deterministic host tests: all CIM calls are replaced, no Windows/device access.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$client=Join-Path $root 'tools/display/test-mt8167-brightness.ps1'
$tokens=$null;$errors=$null
[void][System.Management.Automation.Language.Parser]::ParseFile($client,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
function Assert($Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Get-CimInstance {
    [CmdletBinding()]
    param($Namespace,$ClassName,$InputObject,$OperationTimeoutSec)
    Assert ($OperationTimeoutSec -eq 3) 'Missing per-operation timeout'
    if($null -ne $InputObject) {
        $global:A04BrightnessMockState.reads++
        $current=$global:A04BrightnessMockState.value
        if($global:A04BrightnessMockState.mode -eq 'mismatch' -and $current -eq 70){$current=69}
        return [pscustomobject]@{Active=$true;InstanceName='DISPLAY\MT8167_0';CurrentBrightness=$current}
    }
    Assert ($Namespace -eq 'root\wmi') 'Wrong WMI namespace'
    if($ClassName -eq 'WmiMonitorBrightness') {
        $levels=@(1..100);if($global:A04BrightnessMockState.mode -eq 'unsupported'){$levels=@(50,70)}
        $monitor=[pscustomobject]@{Active=$true;InstanceName='DISPLAY\MT8167_0';CurrentBrightness=50;Levels=$levels.Count;Level=$levels}
        if($global:A04BrightnessMockState.mode -eq 'ambiguous'){return @($monitor,$monitor)}
        return $monitor
    }
    Assert ($ClassName -eq 'WmiMonitorBrightnessMethods') 'Wrong class'
    if($global:A04BrightnessMockState.mode -eq 'no-method'){return}
    return [pscustomobject]@{Active=$true;InstanceName='DISPLAY\MT8167_0'}
}
function Invoke-CimMethod {
    [CmdletBinding()]
    param($InputObject,$MethodName,$Arguments,$OperationTimeoutSec)
    Assert ($OperationTimeoutSec -eq 3 -and $MethodName -eq 'WmiSetBrightness') 'Wrong method/timeout'
    Assert ($InputObject.InstanceName -eq 'DISPLAY\MT8167_0') 'Wrong target'
    Assert ($Arguments.Timeout -is [uint32] -and $Arguments.Timeout -eq 0 -and $Arguments.Brightness -is [byte]) 'Wrong MOF argument types'
    $value=[int]$Arguments.Brightness
    [void]$global:A04BrightnessMockState.calls.Add($value)
    # Model error/timeout after a side effect: finally must still restore.
    $global:A04BrightnessMockState.value=$value
    if($global:A04BrightnessMockState.mode -eq 'throw-after-write' -and $value -eq 30){throw 'simulated provider timeout after write'}
    if(($global:A04BrightnessMockState.mode -eq 'return-error' -and $value -eq 70) -or ($global:A04BrightnessMockState.mode -eq 'restore-error' -and $value -eq 50)){return [pscustomobject]@{ReturnValue=5}}
    return [pscustomobject]@{ReturnValue=0}
}
foreach($mode in @('success','return-error','throw-after-write','mismatch','unsupported','ambiguous','no-method','restore-error')) {
    $global:A04BrightnessMockState=@{mode=$mode;value=50;reads=0;calls=(New-Object 'System.Collections.Generic.List[int]')}
    $out=Join-Path $OutputDirectory $mode;$failed=$false
    try {& $client -OutputDirectory $out | Out-Null}catch{$failed=$true}
    $r=Get-Content -LiteralPath (Join-Path $out 'result.json') -Raw | ConvertFrom-Json
    Assert (-not $r.raw_pwm_observed) "$mode falsely claimed raw PWM"
    if($mode -in @('unsupported','ambiguous','no-method')) {
        Assert ($failed -and $global:A04BrightnessMockState.calls.Count -eq 0 -and -not $r.mutation_attempted) "$mode must not write"
    } else {
        Assert ($global:A04BrightnessMockState.calls[$global:A04BrightnessMockState.calls.Count-1] -eq 50 -and $r.restore_attempted) "$mode failed to attempt restoration"
        if($mode -eq 'success') {
            Assert (-not $failed -and $r.status -eq 'standard-api-verified-and-restored' -and $r.restore_verified) 'Success/readback/restore failed'
            Assert (($global:A04BrightnessMockState.calls -join ',') -eq '30,70,50' -and $global:A04BrightnessMockState.reads -eq 3) 'Wrong sequence or cached readback'
            $duties=@($r.steps | Where-Object {$_.phase -eq 'set-30' -or $_.phase -eq 'set-70'} | ForEach-Object {$_.expected_duty_from_source})
            Assert (($duties -join ',') -eq '307,716') 'Incorrect predicted duties'
        } else {
            Assert ($failed -and $r.status -eq 'failed') "$mode falsely succeeded"
            Assert ($r.restore_verified -eq ($mode -ne 'restore-error')) "$mode wrong restore result"
            if($mode -eq 'mismatch'){Assert ($global:A04BrightnessMockState.reads -eq 6) 'Readback retry was not bounded'}
        }
    }
    Write-Output "$mode PASS"
}
