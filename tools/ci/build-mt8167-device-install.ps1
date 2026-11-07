param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\build\mt8167-device-install'),
    [switch]$Sign,
    [string]$CertificateThumbprint = '2655FC89A7082D3BAB3F26EC556EE8A3C52F4358',
    [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateLocation = 'CurrentUser'
)
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$source = Join-Path $PSScriptRoot '..\winpe\mt8167-device-install.c'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
$object = Join-Path $out 'mt8167-device-install.obj'
$exe = Join-Path $out 'MT8167DeviceInstall.exe'
# Fixed output path; no implicit use of TEMP or remote commands.
& clang --target=thumbv7-pc-windows-msvc -O2 -fno-builtin -fno-stack-protector -w -fms-extensions -ffreestanding -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$inc\um" "-I$inc\shared" -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw 'ARM32 installer compilation failed' }
& lld-link /subsystem:console,6.3 /machine:arm /entry:mainCRTStartup /nodefaultlib /dynamicbase /nxcompat $object (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'advapi32.lib') "/out:$exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 installer link failed' }
if ($Sign) {
    $cert = Get-Item "Cert:\$CertificateLocation\My\$CertificateThumbprint"
    if (-not $cert.HasPrivateKey) { throw 'Signing certificate has no private key' }
    $signArgs = @('sign','/fd','SHA256','/sha1',$CertificateThumbprint,'/s','My')
    if ($CertificateLocation -eq 'LocalMachine') { $signArgs += '/sm' }
    # Deliberately no /t or /tr: preserve the development certificate's 2010..2040 interval.
    & (Join-Path $kit 'bin\x86\signtool.exe') @signArgs $exe
    if ($LASTEXITCODE -ne 0) { throw 'ARM32 installer signing failed' }
}
$manifest = [ordered]@{
    source_commit = (& git -C (Join-Path $PSScriptRoot '..\..') rev-parse HEAD)
    source_sha256 = (Get-FileHash -Algorithm SHA256 $source).Hash
    exe_sha256 = (Get-FileHash -Algorithm SHA256 $exe).Hash
    target = 'Windows RT 8.1 ARM32'
    signed = [bool]$Sign
    signing_thumbprint = $(if ($Sign) { $CertificateThumbprint } else { $null })
    service = 'MT8167DeviceInstall'
    actual_device_bind_verified = $false
}
$manifest | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
Write-Host "ARM32 installer: $exe"
