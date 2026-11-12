# SPDX-License-Identifier: BSD-2-Clause-Patent
# Synthetic append-only signing layout; no certificate store or EXE execution.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$path=Join-Path $PSScriptRoot '..\tools\ci\build-atomic-stage-replace.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Builder parse failed'}
$definition=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'SignedPayload'},$true)
if($null -eq $definition){throw 'SignedPayload definition missing'}
. ([ScriptBlock]::Create($definition.Extent.Text))
$unsigned=New-Object byte[] 512
[BitConverter]::GetBytes([int]128).CopyTo($unsigned,60)
[BitConverter]::GetBytes([uint16]0x10b).CopyTo($unsigned,152)
$unsigned[400]=65
$signed=New-Object byte[] 544
[Array]::Copy($unsigned,$signed,512)
[BitConverter]::GetBytes([uint32]512).CopyTo($signed,280)
[BitConverter]::GetBytes([uint32]32).CopyTo($signed,284)
$signed[216]=37 # PE checksum may change during signing.
$signed[520]=48 # Synthetic PKCS7-like payload; no cryptographic validity claim.
$result=SignedPayload $unsigned $signed
if(!$result.unchanged -or $result.unsigned_bytes -ne 512 -or $result.certificate_offset -ne 512 -or
   $result.normalized_signed_sha256 -cne $result.normalized_unsigned_sha256){throw 'Valid signing layout rejected'}
Write-Output 'PASS append-only signature/checksum changes'
$mutated=[byte[]]$signed.Clone();$mutated[400]=66
$rejected=$false
try{$null=SignedPayload $unsigned $mutated}catch{$rejected=$true}
if(!$rejected){throw 'Payload mutation admitted'}
Write-Output 'PASS changed production payload rejected'
$padded=New-Object byte[] 552
[Array]::Copy($signed,$padded,512)
[BitConverter]::GetBytes([uint32]520).CopyTo($padded,280)
[BitConverter]::GetBytes([uint32]32).CopyTo($padded,284)
$padded[512]=1
$rejected=$false
try{$null=SignedPayload $unsigned $padded}catch{$rejected=$true}
if(!$rejected){throw 'Nonzero signing padding admitted'}
Write-Output 'PASS nonzero signing padding rejected'
$alreadySigned=[byte[]]$unsigned.Clone()
[BitConverter]::GetBytes([uint32]512).CopyTo($alreadySigned,280)
$rejected=$false
try{$null=SignedPayload $alreadySigned $signed}catch{$rejected=$true}
if(!$rejected){throw 'Already signed input admitted'}
Write-Output 'PASS already signed input rejected'
