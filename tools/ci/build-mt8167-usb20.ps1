param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
if (!(Test-Path (Join-Path $inc 'km\ntddk.h'))) { throw 'Windows 8.1 WDK is required' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
foreach ($name in @('ep0','ep0_pio')) {
  $source = Join-Path $root "drivers\usb\mt8167-usb20\$name.c"
  $obj = Join-Path $out "mt8167-usb20-$name.obj"
  & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c $source -o $obj
  if ($LASTEXITCODE -ne 0) { throw "ARM32 WDK compilation failed: $name" }
  $headers = & llvm-readobj --file-headers $obj
  if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected COFF machine' }
}
Write-Host 'EP0 object only; no installable driver was built.'
