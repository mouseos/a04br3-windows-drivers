# In-memory contract tests only. No real registry, hive load, capture or device.
$ErrorActionPreference='Stop'
if('MT8167OfflineHive.Native' -as [type]){throw 'Fresh process required for mock-only test'}
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using Microsoft.Win32;
namespace MT8167OfflineHive {
 public sealed class MockKey : IDisposable {
  public string[] Children=new string[0];
  public Dictionary<string,object> Values=new Dictionary<string,object>(StringComparer.OrdinalIgnoreCase);
  public int ValueCount {get{return Values.Count;}}
  public int SubKeyCount {get{return Children.Length;}}
  public int Enumerations,Closed;
  public string[] GetValueNames(){var a=new string[Values.Count];Values.Keys.CopyTo(a,0);return a;}
  public string[] GetSubKeyNames(){Enumerations++;return Children;}
  public RegistryValueKind GetValueKind(string n){return Values[n] is uint ? RegistryValueKind.DWord : RegistryValueKind.String;}
  public void Dispose(){Closed++;}
 }
 public static class Native {
  public static Dictionary<string,MockKey> Keys=new Dictionary<string,MockKey>();
  public static List<string> Reads=new List<string>();
  public static MockKey OpenPath(object root,string p,bool writable){if(writable)throw new Exception("write rejected");MockKey k;return Keys.TryGetValue(p,out k)?k:null;}
  public static string ReadFixedRawValue(MockKey k,string n,uint type,int bytes){
   if(type!=4||bytes!=4||!(k.Values[n] is uint))throw new Exception("non-DWORD data read");
   Reads.Add(n);return BitConverter.ToString(BitConverter.GetBytes((uint)k.Values[n])).Replace("-","");
  }
 }
}
'@
. "$PSScriptRoot/../tools/recovery/read-graphics-topology-cache.ps1"
function Assert($v,$message){if(!$v){throw $message}}
function Reset {[MT8167OfflineHive.Native]::Keys.Clear();[MT8167OfflineHive.Native]::Reads.Clear()}
$base='ControlSet001\Control\GraphicsDrivers\'
Reset
$k=New-Object MT8167OfflineHive.MockKey
$k.Values['primsurfsize.cx']=[uint32]800
$k.Values['Position.cx']=[uint32]4294966016
$k.Values['PixelFormat']='wrong type; must not read contents'
$k.Values['SecretSSID']='must never read contents'
[MT8167OfflineHive.Native]::Keys[$base+'Configuration']=$k
$r=Read-GraphicsTopologyCache $null 'ControlSet001'
Assert ($r.complete -and $r.records.Count -eq 2) 'Both fixed roots observed'
Assert ($r.records[0].DWORD_values['PrimSurfSize.cx'].value_uint32 -eq 800) 'Registry value names are case insensitive'
Assert ($r.records[0].DWORD_values['Position.cx'].value_int32 -eq -1280) 'Negative virtual desktop position retained'
Assert ([MT8167OfflineHive.Native]::Reads.Count -eq 2) 'Only whitelisted DWORD contents read'
Assert ($r.records[0].ignored_non_DWORD.Count -eq 1 -and $k.Closed -eq 1) 'Non-DWORD omitted and handle closed'
Assert (!$r.active_topology_verified -and !$r.adapter_owner_verified) 'Cache does not assert live adapter identity'
Reset
$k=New-Object MT8167OfflineHive.MockKey
$k.Children=@(1..126|ForEach-Object {"saved$_"})
[MT8167OfflineHive.Native]::Keys[$base+'Configuration']=$k
foreach($name in $k.Children){[MT8167OfflineHive.Native]::Keys[$base+'Configuration\'+$name]=New-Object MT8167OfflineHive.MockKey}
$r=Read-GraphicsTopologyCache $null 'ControlSet001'
Assert ($r.keys_visited -eq 128 -and $r.complete) '128 scheduled keys including both roots allowed'
Reset
$k=New-Object MT8167OfflineHive.MockKey;$k.Children=@(1..127|ForEach-Object {"saved$_"})
[MT8167OfflineHive.Native]::Keys[$base+'Configuration']=$k
$r=Read-GraphicsTopologyCache $null 'ControlSet001'
Assert ($r.truncated -and !$r.complete -and $r.keys_visited -eq 2) '129th key cannot exceed budget or hide other root'
Assert ($k.Enumerations -eq 0) 'Excessive child names are never enumerated'
Reset
$path=$base+'Configuration'
for($i=0;$i -le 9;$i++){
 $k=New-Object MT8167OfflineHive.MockKey;$k.Children=@('next')
 [MT8167OfflineHive.Native]::Keys[$path]=$k;$path+='\next'
}
$r=Read-GraphicsTopologyCache $null 'ControlSet001'
Assert ($r.truncated -and @($r.records|Where-Object {$_.depth -gt 8}).Count -eq 0) 'Maximum depth eight'
Assert (@($r.records|Where-Object {$_.limit_reason -eq 'max-depth'}).Count -eq 1) 'Depth truncation explicitly recorded'
'GRAPHICS_TOPOLOGY_MOCK_CONTRACTS_PASSED_NO_REGISTRY'
