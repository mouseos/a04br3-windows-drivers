# SPDX-License-Identifier: MIT
# Official fixed HLSL compiler inputs, host build only; no device payload broker.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Fixed clean source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh output inside checkout required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.IO;
[ComImport,Guid("8BA5FB08-5195-40e2-AC58-0D989C3A0102"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IGeoBlob { [PreserveSig] IntPtr GetBufferPointer(); [PreserveSig] UIntPtr GetBufferSize(); }
public static class GeoFixedShader {
 [DllImport("d3dcompiler_47.dll",CallingConvention=CallingConvention.StdCall)]
 static extern int D3DCompile(byte[] s,UIntPtr n,string name,IntPtr defines,IntPtr include,string entry,string target,uint flags,uint effects,out IGeoBlob code,out IGeoBlob errors);
 public static void Compile(string source,string target,string output) {
  byte[] b=Encoding.ASCII.GetBytes(source);IGeoBlob c=null,e=null;
  int hr=D3DCompile(b,(UIntPtr)b.Length,"GE8300-standard-client-fixed-HLSL",IntPtr.Zero,IntPtr.Zero,"main",target,0,0,out c,out e);
  try {
   if(hr<0||c==null)throw new Exception("D3DCompile "+hr.ToString("X8")+" "+(e==null?"":Marshal.PtrToStringAnsi(e.GetBufferPointer())));
   byte[] data=new byte[checked((int)c.GetBufferSize().ToUInt64())];Marshal.Copy(c.GetBufferPointer(),data,0,data.Length);File.WriteAllBytes(output,data);
  } finally {if(c!=null)Marshal.ReleaseComObject(c);if(e!=null)Marshal.ReleaseComObject(e);}
 }
}
'@
 $programs=@(
  @{name='position-vs';profile='vs_4_0_level_9_1';source='float4 main(float4 p:POSITION):SV_Position{return p;}'},
  @{name='constant-ps';profile='ps_4_0_level_9_1';source='cbuffer C:register(b0){float4 color;} float4 main():SV_Target{return color;}'},
  @{name='color-vs';profile='vs_4_0_level_9_1';source='struct V{float4 position:SV_Position;float4 color:COLOR0;}; V main(float4 p:POSITION,float4 c:COLOR0){V v;v.position=p;v.color=c;return v;}'},
  @{name='color-ps';profile='ps_4_0_level_9_1';source='float4 main(float4 c:COLOR0):SV_Target{return c;}'}
 )
 $records=@()
 foreach($p in $programs){
  $path=Join-Path $out ($p.name+'.dxbc');[GeoFixedShader]::Compile($p.source,$p.profile,$path)
  $records+=@{name=$p.name;profile=$p.profile;hlsl=$p.source;sha256=(Get-FileHash $path -Algorithm SHA256).Hash}
 }
 @{source_commit=(& git -C $root rev-parse HEAD);compiler_sha256=(Get-FileHash "$env:windir\System32\d3dcompiler_47.dll" -Algorithm SHA256).Hash;programs=$records;gpu_executed=$false}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'shader-manifest.json')
 Compress-Archive -Path "$out\*.dxbc","$out\shader-manifest.json" -DestinationPath "$out\fixed-shaders.zip"
 Get-FileHash "$out\fixed-shaders.zip" -Algorithm SHA256|Format-List
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
