param([Parameter(Mandatory=$true)][string]$FixtureDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/recovery/SignedRenderNativeAdmission.ps1')
$cases=Get-Content -LiteralPath (Join-Path $FixtureDirectory 'cases.json') -Raw|ConvertFrom-Json
foreach($c in $cases){
    $accepted=$false
    try {
        $mode=Assert-SignedRenderNativeAdmission -PolicyPath (Join-Path $FixtureDirectory $c.path) -ExpectedSha256 $c.sha256 -Capture $c.capture
        $accepted=$true
    } catch { $accepted=$false }
    if($accepted -ne $c.accepted){throw ('Unexpected native gate admission: '+$c.name)}
}
# 正常policyでもSHA不一致はfail closed。hive APIは一切使わない。
$positive=$cases|Where-Object {$_.name -ceq 'collection'}
$rejected=$false
try {$null=Assert-SignedRenderNativeAdmission -PolicyPath (Join-Path $FixtureDirectory $positive.path) -ExpectedSha256 ('0'*64) -Capture $positive.capture} catch {$rejected=$true}
if(!$rejected){throw 'Wrong policy SHA accepted'}
'WINDOWS_NATIVE_ADMISSION_MOCK_PASS_NO_HIVE_IO'
