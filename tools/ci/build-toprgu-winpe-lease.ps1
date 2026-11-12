# WinPE専用lease候補。既cache/証明書のみ。SDK準備・通常Windows導入は行わない。
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceCommit,
 [Parameter(Mandatory=$true)][string]$WdkRoot,
 [Parameter(Mandatory=$true)][string]$WdfIncludeRoot,
 [Parameter(Mandatory=$true)][string]$WdfLibraryRoot,
 [Parameter(Mandatory=$true)][string]$SigningToolRoot,
 [string]$KernelLibraryRoot,
 [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateStore='CurrentUser',
 [ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570'
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath("$PSScriptRoot\..\..").TrimEnd('\');$out=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$prefix='drivers/system/mt8167-toprgu/';$dir=Join-Path $root $prefix
function Git([string[]]$Arguments){$v=@(& git.exe -C $root @Arguments);if($LASTEXITCODE){throw 'Git source pin check failed'};return $v}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin([string]$Path){$i=Get-Item -LiteralPath ([IO.Path]::GetFullPath($Path));if($i.PSIsContainer){throw 'File required'};[ordered]@{path=$i.FullName;bytes=$i.Length;sha256=(Hash $i.FullName)}}
function Save{$m|ConvertTo-Json -Depth 20|Set-Content -LiteralPath (Join-Path $out 'manifest.json') -Encoding UTF8}
function Tool([string]$Name,[string]$Exe,[string[]]$Arguments){
 if(@($m.steps|Where-Object name -CEQ $Name).Count){throw 'No repeated tool stage'}
 $log=Join-Path $out ("logs\$Name.log");$step=[ordered]@{name=$Name;exit_code=$null;log_sha256=$null;tool=$Exe;arguments=$Arguments};$m.steps+=@($step);Save
 & $Exe @Arguments *> $log
 $step.exit_code=$LASTEXITCODE;$step.log_sha256=Hash $log;Save
 if($step.exit_code -ne 0){throw ("Stage failed; keep partial output: $Name")}
}
function Dependencies([string]$Path){
 $text=[IO.File]::ReadAllText($Path) -replace '\\\r?\n',' ';$target=[regex]::Match($text,'(?s)^.*?:\s+')
 if(!$target.Success){throw 'Malformed actual Make dependency target'}
 $answer=@();foreach($item in [regex]::Matches($text.Substring($target.Length),'(?:\\.|[^\s\\])+')){
  $name=($item.Value -replace '\\([ #:\\])','$1').Replace('$$','$');if(![IO.Path]::IsPathRooted($name)){$name=Join-Path $root $name};$answer+=,(Pin $name)
 };if(!$answer.Count){throw 'Actual compile dependencies required'};return $answer
}
function Marker([string]$Path){
 $bytes=[IO.File]::ReadAllBytes($Path);$asciiText=[Text.Encoding]::ASCII.GetString($bytes)
 if(!$asciiText.Contains('A04RGU_WINPE_LEASE_V1:1800'+[char]0)){throw 'Consumer requires the exact NUL-terminated ASCII lease marker'}
 $text=$asciiText+"`n"+[Text.Encoding]::Unicode.GetString($bytes)
 $matches=@([regex]::Matches($text,'A04RGU_WINPE_LEASE_V1:[0-9]+')|ForEach-Object Value|Sort-Object -Unique)
 if($matches.Count -ne 1 -or $matches[0] -cne 'A04RGU_WINPE_LEASE_V1:1800'){throw 'Exact linked SYS WinPE lease marker required'}
 return [ordered]@{text=$matches[0];SYS_sha256=(Hash $Path);present=$true;runtime_execution_verified=$false}
}
function Normalize-ToprguLeaseInf([string]$Original){
 if(@($Original.ToCharArray()|Where-Object {[int]$_ -gt 127}).Count){throw 'ASCII source INF required'}
 $literalCRLF=([string][char]92)+'r'+[char]92+'n'
 $literalCount=[regex]::Matches($Original,[regex]::Escape($literalCRLF)).Count
 $normalized=$Original.Replace($literalCRLF,"`r`n").Replace("`r`n","`n").Replace("`r","`n").Replace("`n","`r`n")
 $pattern='(?m)^DriverVer=[^\r\n]+'
 if([regex]::Matches($normalized,$pattern).Count -ne 1 -or [regex]::Matches($normalized,'(?m)^CatalogFile=mt8167-toprgu\.cat\r?$').Count -ne 1){throw 'One DriverVer and fixed CatalogFile required'}
 $text=[regex]::Replace($normalized,$pattern,'DriverVer=11/06/2026,0.0.13.0')
 if([regex]::Replace($normalized,$pattern,'DriverVer=CHECK') -cne [regex]::Replace($text,$pattern,'DriverVer=CHECK')){throw 'Unexpected non-DriverVer metadata change'}
 return @{Text=$text;LiteralCount=$literalCount}
}
function PackageRows{
 @(Get-ChildItem -LiteralPath $package -File|Sort-Object Name|ForEach-Object{[ordered]@{name=$_.Name;path=$_.FullName;bytes=$_.Length;sha256=(Hash $_.FullName)}})
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

if(!$out.StartsWith($root+'\out\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'Fresh checkout-local out directory required'}
$walk=$out;while($walk){if(Test-Path -LiteralPath $walk){if(((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Output ancestor reparse point forbidden'}};$next=[IO.Path]::GetDirectoryName($walk);if($next -eq $walk){break};$walk=$next}
$commit=(Git -Arguments @('rev-parse','HEAD')) -join ''
if($commit -cne $ExpectedSourceCommit -or (Git -Arguments @('status','--porcelain','--untracked-files=normal'))){throw 'Pinned clean committed source required'}
$relative=@(Git -Arguments @('ls-files','--',($prefix+'*.c'),($prefix+'*.h'),($prefix+'mt8167-toprgu.inf')))
if(@($relative|Where-Object {$_ -like '*.c'}).Count -ne 2 -or $relative -cnotcontains ($prefix+'winpe_lease.h') -or $relative -cnotcontains ($prefix+'wdf_toprgu.c') -or $relative -cnotcontains ($prefix+'reset_command.c')){throw 'Exactly two tracked TUs and tracked winpe_lease.h required'}
$relative+=@('tools/ci/build-toprgu-winpe-lease.ps1');$sources=@()
foreach($rel in $relative){
 $path=Join-Path $root $rel;$blob=(Git -Arguments @('rev-parse',("${commit}:$rel"))) -join '';$actual=(Git -Arguments @('hash-object','--no-filters','--',$path)) -join ''
 if($actual -cne $blob){throw ('Worktree bytes differ from pinned Git blob: '+$rel)}
 $sources+=@{path=$rel;git_blob=$blob;sha256=(Hash $path);bytes=(Get-Item -LiteralPath $path).Length}
}
$kit=[IO.Path]::GetFullPath($WdkRoot);$wdf=[IO.Path]::GetFullPath($WdfIncludeRoot);$wdfLib=[IO.Path]::GetFullPath($WdfLibraryRoot);$sign=[IO.Path]::GetFullPath($SigningToolRoot)
$km=if($KernelLibraryRoot){[IO.Path]::GetFullPath($KernelLibraryRoot)}else{Join-Path $kit 'Lib\winv6.3\km\arm'}
$thumb=$CertificateThumbprint.ToUpperInvariant();if($thumb -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Existing BCBF identity only'}
$cert=Get-Item -LiteralPath "Cert:\$CertificateStore\My\$thumb";$trusted=Get-Item -LiteralPath "Cert:\$CertificateStore\Root\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotBefore -gt (Get-Date) -or $cert.NotAfter -lt (Get-Date) -or [Convert]::ToBase64String($cert.RawData) -cne [Convert]::ToBase64String($trusted.RawData)){throw 'Existing same BCBF My/Root certificate and valid private identity required'}
$infDate=[DateTime]::ParseExact('11/06/2026','MM/dd/yyyy',[Globalization.CultureInfo]::InvariantCulture)
if($infDate.Date -gt (Get-Date).Date){throw 'INF DriverVer must not be postdated'}
$tools=[ordered]@{};foreach($pair in @(@('clang','clang'),@('link','lld-link'),@('readobj','llvm-readobj'))){$tools[$pair[0]]=Pin (Get-Command $pair[1] -CommandType Application -ErrorAction Stop).Source}
$tools.signtool=Pin (Join-Path $sign 'signtool.exe');$tools.inf2cat=Pin (Join-Path $sign 'Inf2Cat.exe')
$libs=@();foreach($name in @('wdfdriverentry.lib','wdfldr.lib')){$libs+=,(Pin (Join-Path $wdfLib $name))};foreach($name in @('ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib')){$libs+=,(Pin (Join-Path $km $name))}
$critical=@();foreach($path in @((Join-Path $kit 'Include\km\ntddk.h'),(Join-Path $kit 'Include\km\wdmsec.h'),(Join-Path $wdf 'wdf.h'))){$critical+=,(Pin $path)}
$flags=[ordered]@{A04_TOPRGU_WINPE_LEASE_SECONDS=1800;A04_TOPRGU_CPU1_HANDOFF=1;A04_TOPRGU_UART_SCREEN=1;A04_TOPRGU_UART_VOLUME=1;A04_TOPRGU_THERMAL_STOP=0;A04_TOPRGU_DEADLINE_SECONDS=0}
$includes=@((Join-Path $kit 'Include\km'),(Join-Path $kit 'Include\km\crt'),(Join-Path $kit 'Include\shared'),(Join-Path $kit 'Include\um'),$wdf,$dir)
$defines=@('_KERNEL_MODE','_ARM_','ARM','KMDF_VERSION_MAJOR=1','KMDF_VERSION_MINOR=13','NTDDI_VERSION=0x06030000','_WIN32_WINNT=0x0603')
New-Item -ItemType Directory -Path $out|Out-Null
$work=Join-Path $out 'work';$package=Join-Path $out 'package';New-Item -ItemType Directory -Path $work,$package,(Join-Path $out 'logs')|Out-Null
$m=[ordered]@{schema='TOPRGU_WINPE_LEASE_PACKAGE_V1';success=$false;signed=$false;source_commit=$commit;source_dirty=$false;source_files=$sources;critical_flags=$flags;lease_seconds=1800;force_swrst=$true;force_swrst_runtime_verified=$false;winpe_only=$true;normal_windows_install_allowed=$false;driver_version='0.0.13.0';inf_driverver='11/06/2026,0.0.13.0';certificate=@{thumbprint=$thumb;store=$CertificateStore;root_thumbprint=$trusted.Thumbprint;existing_identity=$true;root_and_My_DER_equal=$true;new_identity_created=$false;private_key_exported=$false;public_DER_only=$true};tools=$tools;link_libraries=$libs;critical_headers=$critical;WDK_root=$kit;WDF_include_root=$wdf;WDF_library_root=$wdfLib;kmdf='1.13';include_roots=$includes;compile_defines=$defines;steps=@();actual_dependencies=@();objects=@();outputs=@();signatures=@();compile_count=0;link_count=0;marker=$null;SDK_install_count=0;GPU_build_count=0;other_driver_build_count=0;device_operations=0;hardware_verified=$false;dependency_evidence_scope='Actual -MD dependencies observed after each compile, hashed and rechecked at completion; not historical per-read environment proof'}
$previous=@{};foreach($name in @('TMP','TEMP','TMPDIR','RUNNER_TEMP')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
Push-Location $root
try{
 Save;foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$work,'Process')}
 $objects=@();$deps=@{}
 foreach($name in @('wdf_toprgu','reset_command')){
  $object=Join-Path $work ($name+'.obj');$dep=Join-Path $work ($name+'.d')
  $argsList=@('--target=thumbv7-pc-windows-msvc','-w','-fms-extensions','-fno-stack-protector','-MD','-MF',$dep)
  foreach($d in $defines){$argsList+=('-D'+$d)};foreach($d in $flags.Keys){$argsList+=('-D'+$d+'='+$flags[$d])};foreach($inc in $includes){$argsList+=('-I'+$inc)}
  $argsList+=@('-c',(Join-Path $dir ($name+'.c')),'-o',$object)
  Tool ('compile-'+$name) $tools.clang.path $argsList;$m.compile_count++;$m.objects+=,(Pin $object);$objects+=$object
  $rows=@(Dependencies $dep);$expected=[IO.Path]::GetFullPath((Join-Path $dir ($name+'.c')))
  if(@($rows|Where-Object path -IEQ $expected).Count -ne 1){throw 'Actual TU missing from compile dependencies'}
  if($name -ceq 'wdf_toprgu' -and !(@($rows|Where-Object path -IEQ (Join-Path $dir 'winpe_lease.h')).Count)){throw 'Lease header absent from actual compile dependencies'}
  foreach($row in $rows){$deps[$row.path]=$row}
 }
 $m.actual_dependencies=@($deps.Keys|Sort-Object|ForEach-Object {$deps[$_]});Save
 $unsigned=Join-Path $work 'mt8167-toprgu.sys';$sys=Join-Path $package 'mt8167-toprgu.sys';$inf=Join-Path $package 'mt8167-toprgu.inf';$cat=Join-Path $package 'mt8167-toprgu.cat';$cer=Join-Path $package 'mt8167-toprgu.cer'
 Tool 'link' $tools.link.path (@('/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib')+$objects+@($libs|ForEach-Object path)+@('/out:'+$unsigned));$m.link_count=1
 Tool 'ARMNT' $tools.readobj.path @('--file-headers',$unsigned)
 if((Get-Content -LiteralPath (Join-Path $out 'logs\ARMNT.log') -Raw) -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT SYS required'}
 $m.marker=Marker $unsigned
 Copy-Item -LiteralPath $unsigned -Destination $sys
 # 主tree INFのliteral backslash-r/backslash-nを実改行に変換する。原INFは変更しない。
 $original=[IO.File]::ReadAllText((Join-Path $dir 'mt8167-toprgu.inf'))
 $infResult=Normalize-ToprguLeaseInf $original
 [IO.File]::WriteAllText($inf,$infResult.Text,[Text.Encoding]::ASCII)
 $m.inf_normalization=@{source_sha256=(Hash (Join-Path $dir 'mt8167-toprgu.inf'));literal_CRLF_sequences_expanded=$infResult.LiteralCount;actual_CRLF=$true;metadata_changes_only=@('DriverVer=11/06/2026,0.0.13.0');source_INF_modified=$false}
 [IO.File]::WriteAllBytes($cer,$cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert))
 if([Convert]::ToBase64String([IO.File]::ReadAllBytes($cer)) -cne [Convert]::ToBase64String($trusted.RawData)){throw 'Public CER differs from existing BCBF Root'}
 $storeArgs=@('/s','My','/sha1',$thumb);if($CertificateStore -ceq 'LocalMachine'){$storeArgs+='/sm'}
 Tool 'sign-sys' $tools.signtool.path (@('sign','/fd','SHA256')+$storeArgs+@($sys))
 $m.non_signature_payload=Verify-Payload $unsigned $sys;$m.marker=Marker $sys
 Tool 'catalog' $tools.inf2cat.path @("/driver:$package",'/os:6_3_ARM','/uselocaltime')
 Tool 'sign-cat' $tools.signtool.path (@('sign','/fd','SHA256')+$storeArgs+@($cat))
 Tool 'verify-sys' $tools.signtool.path @('verify','/pa','/v',$sys)
 Tool 'verify-cat' $tools.signtool.path @('verify','/pa','/v',$cat)
 Tool 'member-mt8167-toprgu.sys' $tools.signtool.path @('verify','/pa','/v','/c',$cat,$sys)
 Tool 'member-mt8167-toprgu.inf' $tools.signtool.path @('verify','/pa','/v','/c',$cat,$inf)
 foreach($path in @($sys,$cat)){$sig=Get-AuthenticodeSignature -LiteralPath $path;if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Actual Windows signature/BCBF mismatch'};$m.signatures+=@{name=[IO.Path]::GetFileName($path);status=[string]$sig.Status;thumbprint=$sig.SignerCertificate.Thumbprint;sha256=(Hash $path)}}
 if(((Git -Arguments @('rev-parse','HEAD')) -join '') -cne $commit -or (Git -Arguments @('status','--porcelain','--untracked-files=normal'))){throw 'Git source changed during build'}
 foreach($s in $sources){if((Hash (Join-Path $root $s.path)) -cne $s.sha256){throw 'Pinned source byte drift'}}
 foreach($p in @($m.actual_dependencies)+@($libs)+@($critical)+@($tools.Values)){if((Hash $p.path) -cne $p.sha256){throw 'Actual cache/header/library/tool byte drift'}}
 if([Convert]::ToBase64String((Get-Item "Cert:\$CertificateStore\Root\$thumb").RawData) -cne [Convert]::ToBase64String($trusted.RawData)){throw 'Root trust certificate changed'}
 $m.outputs=PackageRows
 if(($m.outputs.name -join ',') -cne 'mt8167-toprgu.cat,mt8167-toprgu.cer,mt8167-toprgu.inf,mt8167-toprgu.sys' -or $m.compile_count -ne 2 -or $m.link_count -ne 1){throw 'Exact four-leaf package and two-TU build required'}
 $m.signed=$true;$m.success=$true
}catch{$m.success=$false;$m.failure=$_.Exception.Message;throw}
finally{
 try{$m.outputs=PackageRows;Save}finally{foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')};Pop-Location}
}
'TOPRGU_WINPE_LEASE_PACKAGE_READY_NOT_DEVICE_VERIFIED'
