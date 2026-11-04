# Compile-only diagnostic suite. A scratch LBA of zero is deliberately refused
# by the live GPT guard; a real target requires a separately reviewed build.
param([Parameter(Mandatory=$true)][string]$OutputRoot,
      [Parameter(Mandatory=$true)][string]$SourceCommit)
$ErrorActionPreference='Stop'
if (Test-Path $OutputRoot) { throw 'OutputRoot already exists' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tools = $PSScriptRoot
$oldTemp = $env:RUNNER_TEMP
$keys = @('A04_READONLY_DISK_PROBE','A04_DMA_PROBE','A04_DMA_DISK_PROBE','A04_VIRTUAL_BOUNCE_EXPERIMENT','A04_DMA_MULTIBLOCK_EXPERIMENT','A04_SCRATCH_MULTIBLOCK_EXPERIMENT','A04_BOOT_MULTIBLOCK_EXPERIMENT','A04_PASSIVE_IO','A04_ENABLE_WRITES','A04_SCRATCH_IO','A04_SCRATCH_LBA','A04_SERIAL_QUEUE_EXPERIMENT','A04_ISOLATED_REQUEST_EXPERIMENT','A04_BOOT_EXPERIMENT','A04_BOOT_PASSIVE_EXPERIMENT','A04_DMA_WRITE_TEST','A04_ISOLATED_RESET_TEST')
try {
  foreach ($profile in @('readonly','scratch-compile-only','boot-compile-only')) {
    foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key,$null,'Process') }
    $env:RUNNER_TEMP=Join-Path $OutputRoot $profile
    New-Item -ItemType Directory -Path $env:RUNNER_TEMP | Out-Null
    $env:A04_READONLY_DISK_PROBE='1'; $env:A04_DMA_PROBE='1'; $env:A04_DMA_DISK_PROBE='1'; $env:A04_VIRTUAL_BOUNCE_EXPERIMENT='1'
    if ($profile -eq 'readonly') { $env:A04_PASSIVE_IO='1'; $env:A04_DMA_MULTIBLOCK_EXPERIMENT='1' }
    else {
      $env:A04_PASSIVE_IO='1'; $env:A04_ENABLE_WRITES='1'; $env:A04_SERIAL_QUEUE_EXPERIMENT='1'; $env:A04_ISOLATED_REQUEST_EXPERIMENT='1'
      if ($profile -eq 'scratch-compile-only') { $env:A04_SCRATCH_IO='1'; $env:A04_SCRATCH_LBA='0'; $env:A04_SCRATCH_MULTIBLOCK_EXPERIMENT='1' }
      else { $env:A04_BOOT_EXPERIMENT='1'; $env:A04_BOOT_PASSIVE_EXPERIMENT='1'; $env:A04_BOOT_MULTIBLOCK_EXPERIMENT='1' }
    }
    & "$tools\build-wdk81-probe.ps1"
    & "$tools\sign-probe-test.ps1"
    if ($profile -eq 'readonly') { & "$tools\build-msdc-read-stress.ps1"; $exe=Join-Path $env:RUNNER_TEMP 'msdc-read-stress\msdc-read-stress.exe' }
    elseif ($profile -eq 'scratch-compile-only') { & "$tools\build-msdc-scratch-stress.ps1"; $exe=Join-Path $env:RUNNER_TEMP 'msdc-scratch-stress\msdc-scratch-stress.exe' }
    else { $exe=$null }
    if ($exe) {
      $package=Join-Path $env:RUNNER_TEMP 'a04br3-wdk81-probe\package-test-signed'
      $certFile=Join-Path $package 'a04test.cer'
      $cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certFile)
      & (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My $exe
      if ($LASTEXITCODE -ne 0) { throw 'EXE signing failed' }
      Copy-Item $exe $package
      & (Join-Path $kit 'bin\x86\signtool.exe') verify /pa $exe
      # Self-signed test roots may not be trusted by this runner. Check that
      # Authenticode contains a signature; target trust remains separate.
      if ((Get-AuthenticodeSignature $exe).SignerCertificate.Thumbprint -ne $cert.Thumbprint) { throw 'EXE signature missing or wrong' }
    }
  }
  $files=Get-ChildItem $OutputRoot -Recurse -File | Where-Object { $_.DirectoryName -like '*package-test-signed*' } | ForEach-Object { [ordered]@{path=$_.FullName.Substring($OutputRoot.Length+1); sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash} }
  [ordered]@{source_commit=$SourceCommit; scratch_lba=0; scratch_note='compile-only: LBA 0 is rejected by GPT guard'; warning='Not for deployment; PnP, DMA recovery and virtual miniport ownership unresolved'; files=@($files)} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $OutputRoot 'manifest.json')
} finally {
  $env:RUNNER_TEMP=$oldTemp
  foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key,$null,'Process') }
}
