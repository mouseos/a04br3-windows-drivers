# RX-observation-only candidate. Existing certificate/store and cached kit only.
# No deployment, key generation/export, SDK setup or automatic failed-stage retry.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint,
    [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateStore='CurrentUser',
    [string]$KitRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1",
    [string]$WdkRoot,
    [string]$SigningToolRoot
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$kit=if($WdkRoot){[IO.Path]::GetFullPath($WdkRoot)}else{[IO.Path]::GetFullPath($KitRoot)}
$signRoot=if($SigningToolRoot){[IO.Path]::GetFullPath($SigningToolRoot)}else{Join-Path $kit 'bin\x86'}
$out=[IO.Path]::GetFullPath($OutputDirectory);$thumb=$CertificateThumbprint.ToUpperInvariant()
$runtime='797121bc8ba5a2c31e09ed3372afa585600d3b4b'
function Git([string[]]$Arguments){$v=& git.exe -C $root @Arguments;if($LASTEXITCODE){throw "Git failed: $Arguments"};return $v}
function Hash([string]$Path){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function FilePin([string]$Path){$p=[IO.Path]::GetFullPath($Path);$i=Get-Item -LiteralPath $p;if($i.PSIsContainer){throw "File required: $p"};return [ordered]@{path=$p;bytes=$i.Length;sha256=(Hash $p)}}
function Save-Manifest { $m|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json') }
function Invoke-Tool([string]$Name,[string]$Tool,[string[]]$Arguments){
    if(@($m.steps|Where-Object {$_.name -ceq $Name}).Count){throw 'No duplicate/replayed stage'}
    $log=Join-Path $out ("logs\$Name.log")
    $step=[ordered]@{name=$Name;tool=$Tool;arguments=$Arguments;exit_code=$null;log_sha256=$null}
    $m.steps+=@($step);Save-Manifest
    & $Tool @Arguments *> $log
    $step.exit_code=$LASTEXITCODE;$step.log_sha256=Hash $log;Save-Manifest
    if($step.exit_code -ne 0){throw "Stage failed; retain completed artifacts: $Name"}
}
function Dependencies([string]$Path){
    # Clang Make deps: join continuations, strip target (drive colon is not
    # followed by whitespace), then unescape only Make path escapes.
    $text=[IO.File]::ReadAllText($Path) -replace '\\\r?\n',' '
    $target=[regex]::Match($text,'(?s)^.*?:\s+');if(!$target.Success){throw 'Malformed dependency target'}
    $body=$text.Substring($target.Length);$answer=@()
    foreach($item in [regex]::Matches($body,'(?:\\.|[^\s\\])+')){
        $name=$item.Value -replace '\\([ #:\\])','$1';$name=$name.Replace('$$','$')
        if(![IO.Path]::IsPathRooted($name)){$name=Join-Path $root $name}
        $answer+=,(FilePin $name)
    }
    if(!$answer.Count){throw 'Empty actual dependency file'};return $answer
}
function PayloadHash([byte[]]$Bytes,[int]$Checksum,[int]$Directory){
    $copy=[byte[]]$Bytes.Clone();for($i=0;$i -lt 4;$i++){$copy[$Checksum+$i]=0};for($i=0;$i -lt 8;$i++){$copy[$Directory+$i]=0}
    $h=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($h.ComputeHash($copy))).Replace('-','').ToLowerInvariant()}finally{$h.Dispose()}
}
function Verify-Payload([string]$Unsigned,[string]$Signed){
    $a=[IO.File]::ReadAllBytes($Unsigned);$b=[IO.File]::ReadAllBytes($Signed)
    if($a.Length -lt 256 -or $b.Length -lt $a.Length -or $a[0] -ne 77 -or $a[1] -ne 90){throw 'Unexpected SYS PE lengths/header'}
    $pe=[BitConverter]::ToUInt32($a,60)
    if($pe -gt ($a.Length-160) -or [BitConverter]::ToUInt32($a,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($a,$pe+4) -ne 0x1c4){throw 'Unsigned ARMNT PE required'}
    $optional=$pe+24;if([BitConverter]::ToUInt16($a,$optional) -ne 0x10b){throw 'PE32 ARM required'}
    $checksum=$optional+64;$directory=$optional+96+32
    if([BitConverter]::ToUInt32($a,$directory) -or [BitConverter]::ToUInt32($a,$directory+4)){throw 'Input SYS must be unsigned'}
    $offset=[BitConverter]::ToUInt32($b,$directory);$size=[BitConverter]::ToUInt32($b,$directory+4)
    if($offset -lt $a.Length -or ($offset%8) -or $size -lt 8 -or ([uint64]$offset+$size) -ne $b.Length){throw 'Unexpected appended certificate layout'}
    for($i=$a.Length;$i -lt $offset;$i++){if($b[$i]){throw 'Nonzero signing padding'}}
    $prefix=New-Object byte[] $a.Length;[Array]::Copy($b,$prefix,$a.Length)
    $before=PayloadHash $a $checksum $directory;$after=PayloadHash $prefix $checksum $directory
    if($before -cne $after){throw 'SYS non-signature payload changed'}
    return [ordered]@{unchanged=$true;normalized_unsigned_sha256=$before;normalized_signed_sha256=$after;certificate_offset=$offset;certificate_bytes=$size}
}
$commit=Git -Arguments @('rev-parse','HEAD')
if(Git -Arguments @('status','--porcelain','--untracked-files=normal')){throw 'Clean committed checkout required'}
if(Test-Path -LiteralPath $out){throw 'OutputDirectory must be new; failed/signed outputs are never overwritten or replayed'}
$prefix='drivers/system/mt8167-toprgu/'
$runtimeFiles=@('wdf_toprgu.c','reset_command.c','reset_command.h','watchdog_policy.h','handoff_policy.h','thermal_stop.h','uart_screen.h','rx_observe.h')
foreach($name in $runtimeFiles){$rel=$prefix+$name;if((Git -Arguments @('rev-parse',"${runtime}:$rel")) -cne (Git -Arguments @('rev-parse',"${commit}:$rel"))){throw "Runtime differs from reviewed source $runtime / $rel"}}
$inputs=@($runtimeFiles|ForEach-Object {$prefix+$_})+@(($prefix+'mt8167-toprgu.inf'),'tools/ci/build-mt8167-toprgu.ps1','tools/ci/build-toprgu-rx-observe.ps1')
$sources=@();foreach($rel in $inputs){$sources+=@{path=$rel;git_blob=(Git -Arguments @('rev-parse',"${commit}:$rel"));sha256=(Hash (Join-Path $root $rel))}}
$tools=[ordered]@{}
foreach($pair in @(@('clang','clang'),@('link','lld-link'),@('readobj','llvm-readobj'))){$c=Get-Command $pair[1] -CommandType Application -ErrorAction Stop;$tools[$pair[0]]=FilePin $c.Source}
$tools.inf2cat=FilePin (Join-Path $signRoot 'Inf2Cat.exe');$tools.signtool=FilePin (Join-Path $signRoot 'signtool.exe')
$libPaths=@('Lib\wdf\kmdf\arm\1.13\wdfdriverentry.lib','Lib\wdf\kmdf\arm\1.13\wdfldr.lib','Lib\winv6.3\km\arm\ntoskrnl.lib','Lib\winv6.3\km\arm\hal.lib','Lib\winv6.3\km\arm\bufferoverflowfastfailk.lib')
$libs=@();foreach($rel in $libPaths){$libs+=,(FilePin (Join-Path $kit $rel))}
foreach($rel in @('Include\km\ntddk.h','Include\km\wdmsec.h','Include\wdf\kmdf\1.13\wdf.h')){[void](FilePin (Join-Path $kit $rel))}
$cert=Get-Item -LiteralPath "Cert:\$CertificateStore\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotBefore -gt (Get-Date) -or $cert.NotAfter -lt (Get-Date)){throw 'Selected existing store certificate must be valid and have its private key'}
$flags=[ordered]@{A04_TOPRGU_CPU1_HANDOFF='1';A04_TOPRGU_UART_SCREEN='1';A04_TOPRGU_THERMAL_STOP='0';A04_TOPRGU_DEADLINE_SECONDS='0';A04_TOPRGU_RX_OBSERVE='1'}
New-Item -ItemType Directory -Path $out|Out-Null
$work=Join-Path $out 'work';$package=Join-Path $out 'package';New-Item -ItemType Directory -Path $work,$package,(Join-Path $out 'logs')|Out-Null
$m=[ordered]@{schema='TOPRGU_RX_OBSERVE_BUILD_SIGN_V1';success=$false;signed=$false;source_commit=$commit;recipe_source_commit=$commit;runtime_source_commit=$runtime;source_dirty=$false;source_files=$sources;critical_flags=$flags;kit_root=$kit;signing_tool_root=$signRoot;target='thumbv7-pc-windows-msvc';wdk='8.1';kmdf='1.13';driver_version='0.0.12.0';inf_driverver='11/12/2026,0.0.12.0';tools=$tools;link_libraries=$libs;compile_options=@('--target=thumbv7-pc-windows-msvc','-w','-fms-extensions','-fno-stack-protector','-MD','-MF');compile_defines=@('_KERNEL_MODE','_ARM_','ARM','KMDF_VERSION_MAJOR=1','KMDF_VERSION_MINOR=13','NTDDI_VERSION=0x06030000','_WIN32_WINNT=0x0603');include_roots=@((Join-Path $kit 'Include\km'),(Join-Path $kit 'Include\shared'),(Join-Path $kit 'Include\um'),(Join-Path $kit 'Include\wdf\kmdf\1.13'),(Join-Path $root $prefix));link_options=@('/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib');steps=@();actual_dependencies=@();objects=@();outputs=@();signatures=@();non_signature_payload=$null;certificate=@{thumbprint=$thumb;store=$CertificateStore;not_before=$cert.NotBefore.ToUniversalTime().ToString('o');not_after=$cert.NotAfter.ToUniversalTime().ToString('o');existing_store_only=$true;private_key_exported=$false};hardware_verified=$false;device_operations=0;SDK_installed=$false;normal_builder_changed=$false;dependency_evidence_scope='Actual -MD paths/bytes hashed after compile and rechecked at completion; not a per-read historical environment proof'}
$saved=@{};foreach($v in @(Get-ChildItem Env:|Where-Object {$_.Name -like 'A04_TOPRGU_*' -or $_.Name -in @('RUNNER_TEMP','TEMP','TMP')})){$saved[$v.Name]=$v.Value}
Push-Location $root
try {
    Save-Manifest
    foreach($v in @(Get-ChildItem Env:|Where-Object {$_.Name -like 'A04_TOPRGU_*'})){Remove-Item "Env:$($v.Name)"}
    foreach($k in $flags.Keys){Set-Item "Env:$k" $flags[$k]};foreach($k in @('RUNNER_TEMP','TEMP','TMP')){Set-Item "Env:$k" $work}
    $build=[ordered]@{name='generic-build';exit_code=$null;log_sha256=$null};$m.steps+=@($build);Save-Manifest
    $buildLog=Join-Path $out 'logs\generic-build.log'
    try{& (Join-Path $PSScriptRoot 'build-mt8167-toprgu.ps1') -KitRoot $KitRoot -WdkRoot $kit -SigningToolRoot $signRoot *> $buildLog;$build.exit_code=0}
    catch{$build.exit_code=1;throw}
    finally{if(Test-Path $buildLog){$build.log_sha256=Hash $buildLog};Save-Manifest}
    $built=Join-Path $work 'mt8167-toprgu';$deps=@{}
    foreach($name in @('wdf_toprgu','reset_command')){
        $m.objects+=,(FilePin (Join-Path $built ($name+'.obj')))
        $rows=@(Dependencies (Join-Path $built ($name+'.d')))
        if(!(($rows|ForEach-Object {$_.path}) -icontains ([IO.Path]::GetFullPath((Join-Path $root ($prefix+$name+'.c')))))){throw 'Source absent from actual compile dependency evidence'}
        foreach($r in $rows){$deps[$r.path]=$r}
    }
    $m.actual_dependencies=@($deps.Keys|Sort-Object|ForEach-Object {$deps[$_]});Save-Manifest
    Copy-Item (Join-Path $built 'mt8167-toprgu.sys') (Join-Path $package 'mt8167-toprgu.sys')
    $inf=Join-Path $package 'mt8167-toprgu.inf';$text=[IO.File]::ReadAllText((Join-Path $root ($prefix+'mt8167-toprgu.inf')))
    if([regex]::Matches($text,'(?m)^DriverVer=[^\r\n]+').Count -ne 1 -or @($text.ToCharArray()|Where-Object {[int]$_ -gt 127}).Count){throw 'One ASCII INF DriverVer required'}
    [IO.File]::WriteAllText($inf,([regex]::Replace($text,'(?m)^DriverVer=[^\r\n]+','DriverVer=11/12/2026,0.0.12.0')),[Text.Encoding]::ASCII)
    $storeArgs=@('/s','My','/sha1',$thumb);if($CertificateStore -eq 'LocalMachine'){$storeArgs+='/sm'}
    $sys=Join-Path $package 'mt8167-toprgu.sys';$cat=Join-Path $package 'mt8167-toprgu.cat'
    Invoke-Tool -Name 'sign-sys' -Tool $tools.signtool.path -Arguments (@('sign','/fd','SHA256')+$storeArgs+@($sys))
    $m.non_signature_payload=Verify-Payload (Join-Path $built 'mt8167-toprgu.sys') $sys;Save-Manifest
    Invoke-Tool -Name 'catalog12' -Tool $tools.inf2cat.path -Arguments @("/driver:$package",'/os:6_3_ARM','/uselocaltime')
    Invoke-Tool -Name 'sign-cat' -Tool $tools.signtool.path -Arguments (@('sign','/fd','SHA256')+$storeArgs+@($cat))
    Invoke-Tool -Name 'verify-sys' -Tool $tools.signtool.path -Arguments @('verify','/pa','/v',$sys)
    Invoke-Tool -Name 'verify-cat' -Tool $tools.signtool.path -Arguments @('verify','/pa','/v',$cat)
    foreach($name in @('mt8167-toprgu.sys','mt8167-toprgu.inf')){Invoke-Tool -Name ('member-'+$name) -Tool $tools.signtool.path -Arguments @('verify','/pa','/v','/c',$cat,(Join-Path $package $name))}
    foreach($path in @($sys,$cat)){$s=Get-AuthenticodeSignature -LiteralPath $path;if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'Selected certificate identity/trust mismatch'};$m.signatures+=@{path=$path;sha256=(Hash $path);status=[string]$s.Status;thumbprint=$s.SignerCertificate.Thumbprint}}
    if((Git -Arguments @('rev-parse','HEAD')) -cne $commit -or (Git -Arguments @('diff','--name-only','HEAD'))){throw 'Source changed during build'}
    foreach($s in $sources){if((Hash (Join-Path $root $s.path)) -cne $s.sha256){throw 'Source/header/CI bytes changed during build'}}
    foreach($r in @($m.actual_dependencies)+@($libs)+@($tools.Values)){if((Hash $r.path) -cne $r.sha256){throw 'Actual dependency/library/tool drift'}}
    $m.success=$true;$m.signed=$true
} catch {
    $m.failure=$_.Exception.Message
    # Leave all successful objects, unsigned SYS and signed stages in place.
    throw
} finally {
    $m.outputs=@(Get-ChildItem -LiteralPath $package -File|Sort-Object Name|ForEach-Object {FilePin $_.FullName})
    $m.partial_build_files=@(Get-ChildItem -LiteralPath $work -File -Recurse|Sort-Object FullName|ForEach-Object {FilePin $_.FullName})
    Save-Manifest
    foreach($v in @(Get-ChildItem Env:|Where-Object {$_.Name -like 'A04_TOPRGU_*' -or $_.Name -in @('RUNNER_TEMP','TEMP','TMP')})){Remove-Item "Env:$($v.Name)"}
    foreach($k in $saved.Keys){Set-Item "Env:$k" $saved[$k]};Pop-Location
}
Write-Output "TOPRGU_RX12_SIGNED_CANDIDATE_NOT_DEVICE_VERIFIED=$out"
