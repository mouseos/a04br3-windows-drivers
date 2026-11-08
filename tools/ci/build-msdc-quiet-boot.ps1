# Fixed boot-profile candidate. No deployment.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint,
 [switch]$DetailedIoTrace,[switch]$DmaPollTrace,[switch]$ReuseRequestMdl)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New output inside this checkout required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$baseline=Get-Content (Join-Path $PSScriptRoot 'msdc-f8f54c9-boot-profile.json') -Raw|ConvertFrom-Json
& git -C $root merge-base --is-ancestor $baseline.source_commit HEAD
if($LASTEXITCODE){throw 'Fixed installed boot source ancestry required'}
$values=@{TEMP="$out\tmp";TMP="$out\tmp";RUNNER_TEMP=$out;A04_MSDC_VERBOSE_IO_TRACE=$(if($DetailedIoTrace){'1'}else{'0'})}
foreach($p in $baseline.flags.PSObject.Properties){$values[$p.Name]=[string]$p.Value}
if($DmaPollTrace){$values.A04_DMA_POLL_TRACE='1'}
$values.A04_MSDC_REQUEST_MDL_REUSE=$(if($ReuseRequestMdl){'1'}else{'0'})
$previous=@{};foreach($k in $values.Keys){$previous[$k]=[Environment]::GetEnvironmentVariable($k,'Process')}
New-Item -ItemType Directory "$out\tmp"|Out-Null
try {
 foreach($k in $values.Keys){[Environment]::SetEnvironmentVariable($k,$values[$k],'Process')}
 & (Join-Path $PSScriptRoot 'build-wdk81-probe.ps1')
 $build=Join-Path $out 'a04br3-wdk81-probe';$pkg=Join-Path $build 'package'
 $unsigned=(Get-FileHash "$pkg\mt8167-msdc.sys" -Algorithm SHA256).Hash
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign=Join-Path $kit 'bin\x86\signtool.exe'
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
 if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Signing key unavailable'}
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$pkg\mt8167-msdc.sys"
 if($LASTEXITCODE){throw 'SYS signing failed'}
 Remove-Item "$pkg\mt8167-msdc.cat"
 & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$pkg" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'INF catalog failed'}
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$pkg\mt8167-msdc.cat"
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($n in @('mt8167-msdc.sys','mt8167-msdc.cat')){
  & $sign verify /pa /v "$pkg\$n";if($LASTEXITCODE){throw 'Signature verification failed'}
  if((Get-AuthenticodeSignature "$pkg\$n").SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'Signer mismatch'}
 }
 foreach($n in @('mt8167-msdc.sys','mt8167-msdc.inf')){
  & $sign verify /pa /v /c "$pkg\mt8167-msdc.cat" "$pkg\$n"
  if($LASTEXITCODE){throw 'Catalog membership failed'}
 }
 $pe=& llvm-readobj --file-headers --coff-imports "$pkg\mt8167-msdc.sys"
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 $pe|Set-Content -Encoding UTF8 "$out\pe-metadata.txt"
 Export-Certificate -Cert $cert -FilePath "$pkg\development.cer"|Out-Null
 $sources=@('miniport.c','io_trace.h','pio.c','cdb.c','scsi_capacity.c','scsi.c','dma.c','gpt_guard.c','io_gate.h')
 [ordered]@{source_commit=$commit;source_dirty=$false;baseline_source_commit=$baseline.source_commit;baseline_signed_sys_sha256=$baseline.signed_sys_sha256;
  driver_version='0.0.1.9';flags=$values;normal_io_trace_enabled=[bool]$DetailedIoTrace;dma_poll_trace_enabled=($values.A04_DMA_POLL_TRACE -eq '1');
  logs_only_change=$false;request_mdl_reuse=[bool]$ReuseRequestMdl;parent1026_unchanged=$true;hardware_verified=$false;boot_speed_change_measured=$false;deployment_performed=$false;
  compiler=((& clang --version)-join "`n");linker=((& lld-link --version)-join "`n");
  unsigned_sys_sha256=$unsigned;signer=$cert.Thumbprint;
  sources=@($sources|ForEach-Object{@{path="drivers/storage/mt8167-msdc/$_";sha256=(Get-FileHash (Join-Path $root "drivers\storage\mt8167-msdc\$_") -Algorithm SHA256).Hash}});
  files=@(Get-ChildItem $pkg -File|ForEach-Object{@{path=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$pkg\signed-manifest.json"
 Compress-Archive -Path "$pkg\*" -DestinationPath "$out\mt8167-msdc-ajtd4r-signed.zip"
 'MSDC_QUIET_SIGNED_COMPLETE'
}finally{foreach($k in $previous.Keys){[Environment]::SetEnvironmentVariable($k,$previous[$k],'Process')}}
