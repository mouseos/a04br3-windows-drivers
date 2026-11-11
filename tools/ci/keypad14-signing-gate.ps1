# Fixed actual unsigned14 only. Legacy 1.2/1.3 paths do not call this gate.
function Assert-Keypad14SigningInput([string]$Directory,$Manifest,[string]$InputHash,[string]$Thumbprint,[string]$SignTool,[string]$Inf2Cat) {
 $pins=Get-Content (Join-Path $PSScriptRoot 'mt8167-keypad14-signing-inputs.json') -Raw|ConvertFrom-Json
 if($pins.schema -cne 'mt8167-keypad14-strict-signing-v1' -or $InputHash.ToLowerInvariant() -cne $pins.input_zip_sha256 -or $Thumbprint -cne $pins.certificate_thumbprint){throw '1.4 signing input/certificate pin mismatch'}
 foreach($row in $pins.files){
  if($row.path -cnotmatch '^[A-Za-z0-9_./-]+$' -or $row.path.Split('/') -contains '..'){throw '1.4 unsafe fixed input path'}
  $file=Join-Path $Directory $row.path
  if((Get-Item -LiteralPath $file).Length -ne $row.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.sha256){throw ('1.4 input bytes/hash mismatch: '+$row.path)}
 }
 $expected=@($pins.files|ForEach-Object {$_.path}|Sort-Object)
 $actual=@(Get-ChildItem -LiteralPath $Directory -Recurse -File|ForEach-Object {$_.FullName.Substring($Directory.TrimEnd('\','/').Length+1).Replace('\','/')}|Sort-Object)
 if(($actual -join "`n") -cne ($expected -join "`n")){throw '1.4 exact input file-set mismatch'}
 foreach($t in @(@{path=$SignTool;sha=$pins.signtool_sha256},@{path=$Inf2Cat;sha=$pins.inf2cat_sha256})){
  if((Get-FileHash -LiteralPath $t.path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $t.sha){throw '1.4 signing tool pin mismatch'}
 }
 $m=$Manifest
 if($m.schema -cne 'mt8167-keypad14-incremental-unsigned-v1' -or $m.version -cne '0.0.1.4' -or $m.target -cne 'MT6392\Buttons' -or $m.service -cne 'Mt6392Keys' -or $m.source_dirty -or $m.signed -or $m.CAT_created -or $m.functional_fix_claimed -or $m.device_tested){throw '1.4 candidate identity/status mismatch'}
 if($m.source_commit -cne $pins.compiled_source_commit -or $m.recipe_source_commit -cne $pins.link_recipe_commit -or $m.metadata_source_commit -cne $pins.metadata_commit -or $m.observation_source -cne $pins.observation_commit -or $m.baseline_source -cne $pins.baseline_source_commit -or $m.signed13_manifest_sha256 -cne $pins.baseline13_signed_manifest_sha256){throw '1.4 source provenance mismatch'}
 if($m.compiled_TUs -ne 1 -or $m.compiled_TUs_this_execution -ne 0 -or @($m.reused_objects).Count -ne 2 -or @($m.objects).Count -ne 3 -or @($m.commands).Count -ne 3 -or @($m.actual_dependencies).Count -ne 88 -or @($m.libraries).Count -ne 6){throw '1.4 one-TU/retained-two/dependency contract mismatch'}
 if(!$m.runtime_source_closure_identical -or $m.resume_proof.compile_repeated -or !$m.resume_proof.additional_stdint_bound_by_prior_task_admission -or $m.resume_proof.additional_stdint_prehashed_by_this_recipe){throw '1.4 resume limitation/proof mismatch'}
 $repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
 foreach($row in $pins.repo_inputs){
  if((Get-FileHash (Join-Path $repo $row.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.sha256){throw '1.4 current runtime source/header/INF mismatch'}
 }
 foreach($row in $m.objects){
  $b=[IO.File]::ReadAllBytes((Join-Path $Directory $row.path))
  if([BitConverter]::ToUInt16($b,0) -ne 0x1c4 -or (Get-FileHash (Join-Path $Directory $row.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.sha256){throw '1.4 physical object/ARMNT mismatch'}
 }
 foreach($row in $m.reused_objects){
  $name=[IO.Path]::GetFileNameWithoutExtension($row.path)+'.obj'
  if((Get-FileHash (Join-Path $Directory ('obj/'+$name)) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.object_sha256){throw '1.4 retained object differs'}
 }
 $old=Get-Content (Join-Path $Directory 'baseline13-signed-manifest.json') -Raw|ConvertFrom-Json
 if($old.source_commit -cne $pins.baseline_source_commit -or $old.version -cne '0.0.1.3'){throw '1.4 signed13 baseline mismatch'}
 $commands=Get-Content (Join-Path $Directory 'commands.json') -Raw|ConvertFrom-Json
 if(($commands|ConvertTo-Json -Depth 12 -Compress) -cne ($m.commands|ConvertTo-Json -Depth 12 -Compress)){throw '1.4 actual command receipt mismatch'}
 $deps=Get-Content (Join-Path $Directory 'actual-dependencies.json') -Raw|ConvertFrom-Json
 if(($deps|ConvertTo-Json -Depth 12 -Compress) -cne ($m.actual_dependencies|ConvertTo-Json -Depth 12 -Compress)){throw '1.4 actual dependency receipt mismatch'}
 $inf=[IO.File]::ReadAllText((Join-Path $Directory 'package/mt8167-keypad.inf'))
 if(!$inf.Contains('DriverVer=11/11/2026,0.0.1.4') -or !$inf.Contains('MT6392\Buttons') -or !$inf.Contains('mt8167-keypad.sys')){throw '1.4 INF version/hardware/service filename mismatch'}
 $head=(& git -C $repo rev-parse HEAD)
 if($LASTEXITCODE -or @(& git -C $repo status --porcelain).Count){throw '1.4 signing source must be fixed/clean'}
 return @{pins=$pins;signing_source_commit=$head}
}

function Get-Keypad14Payload([string]$File,[int]$ReferenceBytes,[bool]$RequireSigned) {
 $b=[IO.File]::ReadAllBytes($File)
 if($ReferenceBytes -lt 256 -or $b.Length -lt $ReferenceBytes){throw '1.4 payload length'}
 $pe=[BitConverter]::ToInt32($b,60);$opt=$pe+24
 if($pe -lt 64 -or $opt+136 -gt $ReferenceBytes -or [BitConverter]::ToUInt32($b,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($b,$pe+4) -ne 0x1c4 -or [BitConverter]::ToUInt16($b,$opt) -ne 0x10b){throw '1.4 payload PE32/ARMNT shape'}
 $security=$opt+128;$certAt=[BitConverter]::ToUInt32($b,$security);$certSize=[BitConverter]::ToUInt32($b,$security+4)
 if($RequireSigned){
  if($certAt -lt $ReferenceBytes -or $certAt-$ReferenceBytes -gt 7 -or !$certSize -or [UInt64]$certAt+$certSize -ne $b.Length){throw '1.4 appended signature layout'}
  for($i=$ReferenceBytes;$i -lt $certAt;$i++){if($b[$i]){throw '1.4 nonzero pre-certificate padding'}}
 }elseif($certAt -or $certSize -or $b.Length -ne $ReferenceBytes){throw '1.4 input is not the fixed unsigned image'}
 $payload=New-Object byte[] $ReferenceBytes;[Array]::Copy($b,$payload,$ReferenceBytes)
 [Array]::Clear($payload,$opt+64,4);[Array]::Clear($payload,$security,8)
 $h=[Security.Cryptography.SHA256]::Create();try{$digest=([BitConverter]::ToString($h.ComputeHash($payload))).Replace('-','').ToLowerInvariant()}finally{$h.Dispose()}
 return @{sha256=$digest;reference_bytes=$ReferenceBytes;certificate_offset=$certAt;certificate_bytes=$certSize}
}
