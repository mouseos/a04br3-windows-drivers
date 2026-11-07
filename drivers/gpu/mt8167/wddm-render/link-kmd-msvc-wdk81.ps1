# SPDX-License-Identifier: MIT
# WDK8.1 ARM displib places UTF16 data in executable sections. Use Microsoft
# LINK: the observed LLD output added a Thumb bit to these data references.
param([Parameter(Mandatory=$true)][string]$KernelLibrary,
 [Parameter(Mandatory=$true)][string]$NativeLibrary,
 [Parameter(Mandatory=$true)][string]$OutputFile,
 [Parameter(Mandatory=$true)][string]$MapFile,
 [Parameter(Mandatory=$true)][string]$MetadataFile)
$ErrorActionPreference='Stop'
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if($LASTEXITCODE -or !$vs){throw 'Existing Microsoft C++ tools required; do not install a replacement automatically'}
$link=$null
foreach($v in @(Get-ChildItem "$vs\VC\Tools\MSVC" -Directory|Sort-Object Name -Descending)){
 $p="$($v.FullName)\bin\Hostx64\x64\link.exe";if(Test-Path $p -PathType Leaf){$link=$p;break}
}
if(!$link){throw 'Existing Microsoft LINK not found'}
$km="${env:ProgramFiles(x86)}\Windows Kits\8.1\Lib\winv6.3\km\arm"
& $link /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 $KernelLibrary $NativeLibrary "$km\displib.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$OutputFile" "/map:$MapFile"
if($LASTEXITCODE){throw 'Microsoft ARM kernel link failed'}
@{name='Microsoft LINK';version=(Get-Item $link).VersionInfo.FileVersion;
 sha256=(Get-FileHash $link -Algorithm SHA256).Hash.ToLowerInvariant();
 output_sha256=(Get-FileHash $OutputFile -Algorithm SHA256).Hash.ToLowerInvariant();
 displib_sha256=(Get-FileHash "$km\displib.lib" -Algorithm SHA256).Hash.ToLowerInvariant()}|
 ConvertTo-Json|Set-Content -Encoding UTF8 $MetadataFile
