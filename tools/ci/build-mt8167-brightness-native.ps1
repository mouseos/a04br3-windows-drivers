# Build the fixed native client only. No deployment, runner dispatch or SYS changes.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$KitRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1",
 [string]$CertificateThumbprint=$env:A04_TEST_CERTIFICATE_THUMBPRINT
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(-not $out.StartsWith($root.TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
if(Test-Path -LiteralPath $out){throw 'Use a fresh OutputDirectory'}
$inc=Join-Path $KitRoot 'Include';$lib=Join-Path $KitRoot 'Lib\winv6.3\um\arm'
foreach($file in @((Join-Path $inc 'um\wbemidl.h'),(Join-Path $inc 'um\windows.h'))){if(-not (Test-Path $file)){throw "Missing WDK8.1 header: $file"}}
$libraries=@('kernel32.lib','ole32.lib','oleaut32.lib','wbemuuid.lib') | ForEach-Object {Join-Path $lib $_}
foreach($file in $libraries){if(-not (Test-Path $file)){throw "Missing WDK8.1 ARM library: $file"}}
New-Item -ItemType Directory -Path $out | Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $temporary=Join-Path $out 'tmp';New-Item -ItemType Directory -Path $temporary | Out-Null
 $env:TEMP=$temporary;$env:TMP=$temporary
 $objects=@();$sources=@('brightness_native','brightness_sequence','brightness_wmi_result','brightness_log')
 foreach($source in $sources) {
  $src=Join-Path $root "tools\display\$source.c";$obj=Join-Path $out "$source.obj";$objects+=$obj
  & clang --target=thumbv7-pc-windows-msvc -O2 -fms-extensions -fno-builtin -fno-stack-protector -D_ARM_ -DARM -D_WIN32_DCOM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c $src -o $obj
  if($LASTEXITCODE -ne 0){throw "ARM32 compile failed: $source"}
 }
 $exe=Join-Path $out 'mt8167-brightness-native.exe'
 & lld-link /subsystem:windows,6.3 /machine:arm /entry:WinMainCRTStartup /nodefaultlib /dynamicbase /nxcompat $objects $libraries "/out:$exe"
 if($LASTEXITCODE -ne 0){throw 'ARM32 native brightness link failed'}
 $headers=& llvm-readobj --file-headers $exe
 if($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'Not an ARM32 executable'}
 $signed=$false
 if($CertificateThumbprint) {
  & (Join-Path $KitRoot 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $CertificateThumbprint /s My $exe
  if($LASTEXITCODE -ne 0){throw 'Existing-certificate EXE signing failed'}
  $signed=$true
 }
 @{
  schema=1;status='native-client-build-only-not-device-validated';commit=(& git -C $root rev-parse HEAD);
  dirty=(@(& git -C $root status --porcelain).Count -ne 0);machine='ARMNT';minimum_os='6.3';signed=$signed;
  log_path='C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log';arguments='none';raw_pwm_observed=$false;stdout_tee=$true;stdout_max_bytes=16384;log_generation='Engine-state-archive';legacy_root_log_modified=$false;
  exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;
  source_hashes=@($sources | ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $root "tools\display\$_.c") -Algorithm SHA256).Hash}});
  header_hashes=@(@('brightness_sequence.h','brightness_native.h','brightness_wmi_result.h','brightness_log.h') | ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $root "tools\display\$_") -Algorithm SHA256).Hash}});
  library_hashes=@($libraries | ForEach-Object {@{name=[IO.Path]::GetFileName($_);sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}});
  certificate_trust='must already be configured by main owner';sys_or_cat_modified=$false;wmi_completion='GetCallStatus-before-GetResultObject';method_return_type='CIM_UINT32/VT_I4';input_signature_checked=$true;missing_output_is_provider_success=$false
 } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'manifest.json') -Encoding UTF8
 Write-Host "Native brightness client prepared: $exe; no deployment performed."
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
