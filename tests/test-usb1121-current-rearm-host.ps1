param([Parameter(Mandatory=$true)][string]$RawIdentity,[Parameter(Mandatory=$true)][string]$OutDirectory)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath("$PSScriptRoot/..")
New-Item -ItemType Directory $OutDirectory -ErrorAction Stop|Out-Null
$helper=Get-Content "$repo/tools/recovery/Usb1121OfflineHiveNative.cs" -Raw
# 実Native helperのraw比較/hashを使う。Registry API境界だけhostのメモリキーへ置換する。
$hostSource=@'
namespace HostTests {
using System;
using System.Collections.Generic;
using Microsoft.Win32;
using MT8167Usb1121OfflineHive;
public class HostKey {
 public static Dictionary<string,HostKey> Keys=new Dictionary<string,HostKey>(StringComparer.OrdinalIgnoreCase);
 public static int Writes,Creates;
 public string Path,Security="unchanged-owner-group-dacl";
 public SortedDictionary<string,RawValue> Values=new SortedDictionary<string,RawValue>(StringComparer.OrdinalIgnoreCase);
 public HostKey(string path) { Path=path;Keys.Add(path,this); }
 public void Dispose() {} public void Flush() {}
 public string[] GetSubKeyNames() {
  List<string> names=new List<string>();foreach(string path in Keys.Keys) {
   if(path.StartsWith(Path+"\\",StringComparison.OrdinalIgnoreCase)) {
    string child=path.Substring(Path.Length+1);if(!child.Contains("\\")) names.Add(child);
   }
  }return names.ToArray();
 }
 public string[] GetValueNames() { List<string> names=new List<string>(Values.Keys);return names.ToArray(); }
 public HostKey CreateSubKey(string name) { Creates++;return new HostKey(Path+"\\"+name); }
 public object GetValue(string name) { return BitConverter.ToInt32(Bytes(Values[name].raw_hex),0); }
 public RegistryValueKind GetValueKind(string name) { return (RegistryValueKind)Values[name].type; }
 public static byte[] Bytes(string hex) { byte[] b=new byte[hex.Length/2];for(int i=0;i<b.Length;i++) b[i]=Convert.ToByte(hex.Substring(2*i,2),16);return b; }
 public void SetValue(string name,object value,RegistryValueKind kind) {
  Writes++;byte[] b=kind==RegistryValueKind.DWord?BitConverter.GetBytes(Convert.ToInt32(value)):System.Text.Encoding.Unicode.GetBytes((string)value+"\0");
  Values[name]=new RawValue {type=(uint)kind,bytes=b.Length,raw_hex=BitConverter.ToString(b).Replace("-","").ToLowerInvariant()};
 }
}
public static class HostNative {
 public static HostKey OpenPath(HostKey root,string path,bool write) { HostKey key;return HostKey.Keys.TryGetValue(path,out key)?key:null; }
 public static string ValueFingerprint(HostKey key) { return Native.FingerprintValues(key.Values,null); }
 public static string ValueFingerprintExceptStart(HostKey key) { return Native.FingerprintValues(key.Values,"Start"); }
 public static string SecurityFingerprint(HostKey key) { return key.Security; }
 public static SortedDictionary<string,RawValue> RawValues(HostKey key) {
  SortedDictionary<string,RawValue> copy=new SortedDictionary<string,RawValue>(StringComparer.OrdinalIgnoreCase);
  foreach(var kv in key.Values) copy.Add(kv.Key,new RawValue {type=kv.Value.type,bytes=kv.Value.bytes,raw_hex=kv.Value.raw_hex});return copy;
 }
 public static void VerifyServiceIdentity(IDictionary<string,RawValue> values,int start) { Native.VerifyServiceIdentity(values,start); }
}
}
'@
Add-Type -TypeDefinition ($helper+$hostSource)

$parseErrors=$null;$tokens=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile("$repo/tools/recovery/prepare-usb1121-system198.ps1",[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw ($parseErrors|Out-String)}
foreach($name in @('Snapshot','PatchService')){
 $node=$ast.Find({param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -ceq $name},$true)
 if(!$node){throw "production function missing: $name"}
 Invoke-Expression ($node.Extent.Text.Replace('[MT8167Usb1121OfflineHive.Native]','[HostTests.HostNative]'))
}
$identity=Get-Content $RawIdentity -Raw|ConvertFrom-Json
$script:Cases=New-Object 'System.Collections.Generic.List[string]'
function Check($ok,$message){if(!$ok){throw $message}}
function Values($key){return [HostTests.HostNative]::RawValues($key)}
function ResetKeys($existing){
 [HostTests.HostKey]::Keys.Clear();[HostTests.HostKey]::Writes=0;[HostTests.HostKey]::Creates=0
 $root=New-Object HostTests.HostKey 'ROOT'
 $null=New-Object HostTests.HostKey 'ControlSet001\Services'
 $other=New-Object HostTests.HostKey 'ControlSet001\Services\Other';$other.SetValue('Start',4,[Microsoft.Win32.RegistryValueKind]::DWord)
 $graphics=New-Object HostTests.HostKey 'ControlSet001\Control\GraphicsDrivers';$graphics.SetValue('DisableAutoAcpiPostDeivce',0,[Microsoft.Win32.RegistryValueKind]::DWord)
 $null=New-Object HostTests.HostKey 'ControlSet001\Control\GraphicsDrivers\Child'
 if($existing){
  $usb=New-Object HostTests.HostKey 'ControlSet001\Services\MT8167Usb1121Install198'
  foreach($f in $identity.PSObject.Properties){$usb.Values.Add($f.Name,[MT8167Usb1121OfflineHive.RawValue]@{type=$f.Value.type;bytes=$f.Value.bytes;raw_hex=$f.Value.raw_hex})}
  $usb.SetValue('ExtraUnrelated','keep',[Microsoft.Win32.RegistryValueKind]::String)
  $child=New-Object HostTests.HostKey 'ControlSet001\Services\MT8167Usb1121Install198\Parameters';$child.SetValue('Start',4,[Microsoft.Win32.RegistryValueKind]::DWord)
 }
 [HostTests.HostKey]::Writes=0;[HostTests.HostKey]::Creates=0
 return $root
}
function USB {return [HostTests.HostKey]::Keys['ControlSet001\Services\MT8167Usb1121Install198']}
function RejectPatch($name,$existing,$mode,$mutate){
 $root=ResetKeys $existing;$script:RearmExistingService=$mode
 if($mutate){& $mutate}
 [HostTests.HostKey]::Writes=0
 $rejected=$false;try{$null=PatchService $root}catch{$rejected=$true}
 Check ($rejected -and [HostTests.HostKey]::Writes -eq 0 -and [HostTests.HostKey]::Creates -eq 0) $name
 $script:Cases.Add($name)
}
$root=ResetKeys $false;$script:RearmExistingService=$false;$p=PatchService $root
[MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity((Values (USB)),2)
Check (!$p.service_existed -and [HostTests.HostKey]::Creates -eq 1 -and [HostTests.HostKey]::Writes -eq 6 -and $p.before -ceq (Snapshot $root)) 'default new-service compatibility'
$script:Cases.Add('default-new-service-creates-six-values')
RejectPatch 'default-rejects-existing-service' $true $false $null
RejectPatch 'explicit-existing-rejects-absent-service' $false $true $null
$root=ResetKeys $true;$script:RearmExistingService=$true;$original=Values (USB);$p=PatchService $root;$after=Values (USB)
[MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($after,2)
Check ($p.service_existed -and [HostTests.HostKey]::Writes -eq 1 -and [HostTests.HostKey]::Creates -eq 0 -and $p.before -ceq (Snapshot $root)) 'existing Start-only patch'
Check ($p.raw_before['Start'].raw_hex -ceq '04000000' -and $after['Start'].raw_hex -ceq '02000000') 'raw before/after preserved'
foreach($name in $original.Keys){if($name -cne 'Start'){Check ($original[$name].raw_hex -ceq $after[$name].raw_hex) ('other value changed: '+$name)}}
$script:Cases.Add('actual233-disabled-start-only-extra-value-child-acl-preserved')
foreach($start in @(0,2,3)){$v=$start;RejectPatch "reject-start-$v" $true $true { (USB).SetValue('Start',$v,[Microsoft.Win32.RegistryValueKind]::DWord) }}
foreach($name in @('Type','ErrorControl','ImagePath','ObjectName','DisplayName')){
 $field=$name;RejectPatch "reject-identity-$field" $true $true { (USB).Values[$field].raw_hex='0'*(USB).Values[$field].raw_hex.Length }
 $field=$name;RejectPatch "reject-identity-type-$field" $true $true { (USB).Values[$field].type=3 }
}
RejectPatch 'reject-start-type' $true $true { (USB).Values['Start'].type=1 }
RejectPatch 'reject-start-byte-length' $true $true { (USB).Values['Start'].bytes=8 }
function DetectChange($name,$mutate){
 $root=ResetKeys $true;$script:RearmExistingService=$true;$p=PatchService $root;& $mutate
 Check ($p.before -cne (Snapshot $root)) $name;$script:Cases.Add($name)
}
DetectChange 'detect-existing-extra-value-change' { (USB).SetValue('ExtraUnrelated','changed',[Microsoft.Win32.RegistryValueKind]::String) }
DetectChange 'detect-existing-child-start-change' { [HostTests.HostKey]::Keys['ControlSet001\Services\MT8167Usb1121Install198\Parameters'].SetValue('Start',2,[Microsoft.Win32.RegistryValueKind]::DWord) }
DetectChange 'detect-existing-child-created' { $null=New-Object HostTests.HostKey 'ControlSet001\Services\MT8167Usb1121Install198\OtherChild' }
DetectChange 'detect-existing-child-deleted' { $null=[HostTests.HostKey]::Keys.Remove('ControlSet001\Services\MT8167Usb1121Install198\Parameters') }
DetectChange 'detect-existing-service-acl-change' { (USB).Security='changed' }
DetectChange 'detect-child-acl-change' { [HostTests.HostKey]::Keys['ControlSet001\Services\MT8167Usb1121Install198\Parameters'].Security='changed' }
DetectChange 'detect-other-service-change' { [HostTests.HostKey]::Keys['ControlSet001\Services\Other'].SetValue('Start',2,[Microsoft.Win32.RegistryValueKind]::DWord) }
DetectChange 'detect-graphics-child-change' { [HostTests.HostKey]::Keys['ControlSet001\Control\GraphicsDrivers\Child'].SetValue('Unrelated',1,[Microsoft.Win32.RegistryValueKind]::DWord) }
foreach($path in @('tools/usb/run-usb1121-service.ps1','tools/ci/build-usb1121-ps-image-component.ps1')){
 $errors=$null;$null=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $path),[ref]$tokens,[ref]$errors);Check (!$errors.Count) ('parse failed: '+$path)
}
@{status='passed';cases=$script:Cases.Count;names=@($script:Cases);actual_production_patch_and_snapshot_functions=$true;actual_CSharp_raw_identity_and_fingerprint=$true;registry_API_boundary='in-memory host keys';native_hive_execution=$false;VM=$false;hardware=$false;raw_identity_sha256=(Get-FileHash $RawIdentity).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 8|Set-Content "$OutDirectory/result.json" -Encoding utf8
"USB_CURRENT_REARM_HOST_PASS cases=$($script:Cases.Count)"
