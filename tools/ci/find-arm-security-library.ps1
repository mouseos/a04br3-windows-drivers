$ErrorActionPreference = 'Stop'
$lib = "${env:ProgramFiles(x86)}\Windows Kits\8.1\Lib\winv6.3\km\arm"
Get-ChildItem -Path $lib -Filter '*.lib' | ForEach-Object {
    $matches = & llvm-nm --defined-only $_.FullName 2>$null | Select-String -SimpleMatch '__security_push_cookie'
    if ($matches) { Write-Host $_.Name; $matches | ForEach-Object { Write-Host $_ } }
}
