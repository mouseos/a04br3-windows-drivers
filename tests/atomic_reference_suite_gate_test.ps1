# SPDX-License-Identifier: BSD-2-Clause-Patent
# Exercise the builder's real receipt gate without executing the Windows helper.
param([string]$ReceiptPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function ParseSource([string]$Path){
 $tokens=$null;$errors=$null
 $ast=[Management.Automation.Language.Parser]::ParseFile($Path,[ref]$tokens,[ref]$errors)
 if($errors.Count){throw 'Source parse failed'}
 return $ast
}
$builder=ParseSource (Join-Path $PSScriptRoot '..\tools\ci\build-atomic-stage-replace.ps1')
$definition=$builder.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'AssertReferenceCases'},$true)
if($null -eq $definition){throw 'Builder reference gate missing'}
. ([ScriptBlock]::Create($definition.Extent.Text))
$native=ParseSource (Join-Path $PSScriptRoot 'atomic_stage_replace_reference_test.ps1')
$commands=$native.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'RunCase'},$true)
$cases=@($commands|ForEach-Object{
 [pscustomobject]@{name=$_.CommandElements[1].Value;passed=$true;expected_exit=[int]$_.CommandElements[3].Value;actual_exit=[int]$_.CommandElements[3].Value}
})
if($cases.Count -ne 20){throw 'Native suite is not the fixed 20 cases'}
AssertReferenceCases $cases
Write-Output 'PASS actual unchanged native suite admitted'
function Reject([string]$Name,[object[]]$InputCases){
 $rejected=$false
 try{AssertReferenceCases $InputCases}catch{$rejected=$true}
 if(!$rejected){throw ('Invalid suite admitted: '+$Name)}
 Write-Output ('PASS rejected '+$Name)
}
Reject 'three cases only' $cases[0..2]
Reject 'twenty duplicates' @((1..20)|ForEach-Object{$cases[0]})
Reject 'one missing case' $cases[0..18]
Reject 'extra case' @($cases+$cases[0])
$changed=@($cases|ForEach-Object{[pscustomobject]@{name=$_.name;passed=$_.passed;expected_exit=$_.expected_exit;actual_exit=$_.actual_exit}})
$changed[19].name='other-case';Reject 'unknown replacement' $changed
$changed[19].name=$cases[0].name;Reject 'duplicate replacement' $changed
$changed[19].name=$cases[19].name;$changed[0].name=$cases[0].name.ToUpperInvariant();Reject 'name case mismatch' $changed
$changed[0].name=$cases[0].name;$changed[0].expected_exit=999;$changed[0].actual_exit=999;Reject 'wrong expected and actual code' $changed
$changed[0].expected_exit=0;$changed[0].actual_exit=0;$changed[0].passed=$false;Reject 'failed case' $changed
$reversed=@($cases);[Array]::Reverse($reversed)
AssertReferenceCases $reversed
Write-Output 'PASS complete permuted suite admitted'
if($ReceiptPath){
 $receipt=Get-Content -LiteralPath $ReceiptPath -Raw|ConvertFrom-Json
 AssertReferenceCases @($receipt.cases)
 Write-Output 'PASS recovered native receipt admitted by builder gate'
}
