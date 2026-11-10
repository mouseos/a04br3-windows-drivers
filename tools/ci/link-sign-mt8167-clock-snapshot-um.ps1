# Incremental UM link/sign only. Never installs SDKs, executes the ARM image,
# rebuilds parent/thermal, exports a key or changes an existing output.
param([Parameter(Mandatory=$true)][string]$CompileDirectory,
 [Parameter(Mandatory=$true)][string]$ArmUserLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out) -or @(& git -C $root status --porcelain).Count){throw 'Clean source and new task-local output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$c=Get-Content "$PSScriptRoot\clock-snapshot-um-contract.json" -Raw|ConvertFrom-Json
$m=Get-Content "$CompileDirectory\compile-recipe.json" -Raw|ConvertFrom-Json
$rows=@($m.commands|Where-Object{$_.source -ceq 'tools/clock/mt8167-clock-snapshot.c'})
if(!$m.compiled -or $rows.Count -ne 1){throw 'Actual single UM compilation required'}
$row=$rows[0];$object="$CompileDirectory\mt8167-clock-snapshot.obj"
if((Sha $object) -cne $c.object_sha256 -or $row.source_sha256 -cne (Sha "$root\$($row.source)")){throw 'UM source/object mismatch'}
foreach($d in $c.source_dependencies.PSObject.Properties){if((Sha (Join-Path $root $d.Name)) -cne $d.Value){throw 'UM include/source drift'}}
$libs=@();foreach($l in $c.libraries.PSObject.Properties){$p=Join-Path $ArmUserLibraryDirectory $l.Name;if((Sha $p) -cne $l.Value){throw 'Reuse existing UM library required'};$libs+=$p}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$exe="$out\mt8167-clock-snapshot.exe"
$args_=@('/subsystem:console,6.3','/machine:arm','/entry:mainCRTStartup','/nodefaultlib','/dynamicbase','/nxcompat',$object)+$libs+@("/out:$exe","/map:$out\client.map")
& lld-link @args_;if($LASTEXITCODE){throw 'UM incremental link failed'}
$pe=& llvm-readobj --file-headers --coff-imports $exe
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 "$out\client-pe.txt"
$table=@{};$dll=$null
foreach($line in $pe){
 if($line -match '^\s*Name:\s+(\S+)'){$dll=$Matches[1].ToLowerInvariant();if(!$c.RT81_exports.$dll){throw 'Unknown import DLL'};$table[$dll]=@()}
 if($line -match '^\s*Symbol:\s+(\S+)\s+\('){$sym=$Matches[1];if(!$dll -or $sym -notin $c.RT81_exports.$dll.symbols){throw 'Unsupported actual RT81 import'};$table[$dll]+=$sym}
}
if($table.Count -ne 2){throw 'Expected kernel32/setupapi imports'}
$unsigned=Sha $exe;$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
if(!(Get-Item "Cert:\CurrentUser\My\$thumb").HasPrivateKey){throw 'Existing key required'}
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $exe;if($LASTEXITCODE){throw 'UM sign failed'}
& $SignTool verify /pa /v $exe;if($LASTEXITCODE -or (Get-AuthenticodeSignature $exe).SignerCertificate.Thumbprint -cne $thumb){throw 'UM trust/signature mismatch'}
@{schema='MT8167_CLOCK_SNAPSHOT_UM_SIGNED_V1';source_commit=(& git -C $root rev-parse HEAD).Trim();compile_source_commit=$m.source_commit;
 compile_command=$row;object_sha256=(Sha $object);unsigned_sha256=$unsigned;exe_sha256=(Sha $exe);signer=$thumb;
 source_dependencies=$c.source_dependencies;libraries=$c.libraries;imports=$table;RT81_exports=$c.RT81_exports;link_args=$args_;
 compiler_invoked=$false;SDK_setup=$false;key_exported=$false;ARM_executed=$false;hardware_verified=$false;raw_observed=$false;
 snapshot_bytes=128;command='mt8167-clock-snapshot.exe --snapshot';output='mt8167-clock-snapshot-v1.bin';create_new=$true;
 effective_CPU_Hz=$null;DDR_Hz=$null;required_parent='2026.11.10.1120';required_thermal='0.0.1.11';query_ABI=1;parent_resource_contract=1117
}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
Compress-Archive "$out\mt8167-clock-snapshot.exe","$out\signed-manifest.json","$out\client-pe.txt","$out\client.map" "$out\clock1120-um-signed.zip"
Get-FileHash "$out\clock1120-um-signed.zip" -Algorithm SHA256|Format-List
'CLOCK_UM_SIGNED_NOT_EXECUTED_OR_CLOCK_OBSERVED'
