param(
  [Parameter(Mandatory=$true)][string]$Source,
  [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Source -PathType Leaf)) { throw 'Source BCD does not exist' }
if (Test-Path $Output) { throw 'Output already exists' }
Copy-Item $Source $Output
& bcdedit /store $Output /set '{default}' testsigning on
if ($LASTEXITCODE -ne 0) { throw 'Failed to enable test signing in BCD copy' }
& bcdedit /store $Output /enum '{default}'
if ($LASTEXITCODE -ne 0) { throw 'Failed to inspect BCD copy' }
