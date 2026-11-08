param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$DependencyDirectory,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';if(!$EnableCandidate){throw 'Explicit compiler-only candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Pin mismatch: $p"}}
& python -B "$root\tools\ci\verify-ge8300-fullsource.py";if($LASTEXITCODE){throw 'Normal source gate'}
Pin "$BaselinePackageDirectory\signed-manifest.json" '7e4ccf146cb8081a93e85b7f83ac24a50ea6f66f8888f4403ccaf08f19433663'
Pin "$BaselineBuildDirectory\build-manifest.json" '1c74110363c68d2270513d3754f06f554632b36d184fa3803290f8cd07fc47c6'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$bm=Get-Content "$BaselineBuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
foreach($row in $base.outputs){Pin "$BaselinePackageDirectory\$($row.name)" $row.sha256}
foreach($row in $bm.outputs){Pin "$BaselineBuildDirectory\$($row.name)" $row.sha256}
$compiler="$DependencyDirectory\compiler";$pair="$DependencyDirectory\pair"
Pin "$compiler\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$pair\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
$pm=Get-Content "$pair\manifest.json" -Raw|ConvertFrom-Json;Pin "$pair\pco_pair.obj" $pm.object_sha256
foreach($row in $bm.existing_compiler_libraries){Pin "$compiler\$($row.name)" $row.sha256}
if($bm.existing_compiler_libraries.Count -ne 10){throw 'Exact library closure10'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,"$out\before","$out\after","$out\tmp"|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP
try{
 $env:TMP="$out\tmp";$env:TEMP=$env:TMP
 $oldLib="$compiler\ge8300-pco-arm32.lib"
 $members=@(& llvm-ar t $oldLib|ForEach-Object{[IO.Path]::GetFileName($_)});if($LASTEXITCODE){throw 'Read archive'}
 if(@($members|Sort-Object -Unique).Count -ne $members.Count -or @($members|Where-Object{$_ -ceq 'sm4_adapter.obj'}).Count -ne 1){throw 'Unique original sm4 member required'}
 & llvm-ar x "--output=$out\before" $oldLib;if($LASTEXITCODE){throw 'Extract original archive'}
 foreach($n in $members){Copy-Item "$out\before\$n" "$out\after\$n"}
 $flags=@(($bm.compiled_units|Where-Object{$_.role -ceq 'UMD'}|Select-Object -First 1).arguments)
 if(!$flags.Count){throw 'Recorded ARM compile flags required'}
 $unit='drivers/gpu/mt8167/wddm-render/compiler/sm4.c'
 & clang @flags -c "$root\$unit" -o "$out\after\sm4_adapter.obj";if($LASTEXITCODE){throw 'ARM compiler decoder TU'}
 $rows=@();foreach($n in $members){$before=Sha "$out\before\$n";$after=Sha "$out\after\$n";if($before -cne $after -and $n -cne 'sm4_adapter.obj'){throw 'Unreviewed archive change'};$rows+=@{name=$n;before_sha256=$before;sha256=$after;compiled=($n -ceq 'sm4_adapter.obj')}}
 $paths=@($members|ForEach-Object{"$out\after\$_"});[array]::Reverse($paths)
 & llvm-lib /machine:arm "/out:$out\ge8300-pco-arm32.lib" @paths;if($LASTEXITCODE){throw 'Rebuild bounded archive'}
 $afterMembers=@(& llvm-ar t "$out\ge8300-pco-arm32.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($members -join "`n") -cne ($afterMembers -join "`n")){throw 'Archive member count/order changed'}
 $libs=@($bm.existing_compiler_libraries|ForEach-Object{if($_.name -ceq 'ge8300-pco-arm32.lib'){"$out\ge8300-pco-arm32.lib"}else{"$compiler\$($_.name)"}})
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$BaselineBuildDirectory\umd\umd_entry.obj" "$BaselineBuildDirectory\umd\umd_typed_entry.obj" "$BaselineBuildDirectory\ge8300-native-umd.lib" "$pair\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD link'}
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD sign'}
 & $SignToolPath verify /pa "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD signature verify'}
 $signature=Get-AuthenticodeSignature "$out\ge8300-umd.dll"
 if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}
 # Re-hash every baseline artifact AFTER the build. No SYS/FW/client/CAT changes.
 foreach($row in $base.outputs){Pin "$BaselinePackageDirectory\$($row.name)" $row.sha256}
 [ordered]@{schema='sm4-literals328-compiler-only-signed-v1';source_commit=$source;baseline327_signed_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");compiled_units=@(@{source=$unit;sha256=(Sha "$root\$unit");object_sha256=(Sha "$out\after\sm4_adapter.obj");arguments=$flags});archive=@{name='ge8300-pco-arm32.lib';members=$rows;sha256=(Sha "$out\ge8300-pco-arm32.lib")};retained_baseline_outputs=$base.outputs;libraries=$bm.existing_compiler_libraries;signer_thumbprint=$thumb;SDK_rebuilt=$false;PCO_core_rebuilt=$false;NIR_rebuilt=$false;kernel_changed=$false;native_owner_changed=$false;client_changed=$false;wire_ABI_changed=$false;caps_changed=$false;native_general_shader_admission_implemented=$false;runtime_ready=$false;hardware_verified=$false;outputs=@(@{name='ge8300-umd.dll';sha256=(Sha "$out\ge8300-umd.dll")},@{name='ge8300-pco-arm32.lib';sha256=(Sha "$out\ge8300-pco-arm32.lib")})}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
