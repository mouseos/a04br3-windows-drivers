// SPDX-License-Identifier: BSD-2-Clause-Patent
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;
using MT8167OfflineHive;
namespace MT8167PostPolicy360 {
 public static class Policy {
  const string Path="ControlSet001\\Control\\GraphicsDrivers";
  const string Name="DisableAutoAcpiPostDeivce";
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegEnumValue(IntPtr key,uint index,StringBuilder name,ref uint chars,IntPtr reserved,out uint type,byte[] data,ref uint bytes);
  static string Hex(byte[] b) {return BitConverter.ToString(b).Replace("-","").ToLowerInvariant();}
  // Raw semantic snapshot: omit exactly the single permitted value, retain every child.
  static void Visit(RegistryKey root,string path,SortedDictionary<string,string> rows,int depth) {
   if(depth>64 || rows.Count>100000) throw new InvalidOperationException("Snapshot bound");
   using(RegistryKey k=Native.OpenPath(root,path,false)) {
    if(k==null) throw new InvalidOperationException("Missing snapshot key");
    rows.Add("K:"+path,"");
    using(SHA256 h=SHA256.Create()) for(uint i=0;;i++) {
     StringBuilder n=new StringBuilder(16384);uint chars=16384,type,bytes=4096;byte[] data=new byte[4096];
     int rc=RegEnumValue(k.Handle.DangerousGetHandle(),i,n,ref chars,IntPtr.Zero,out type,data,ref bytes);
     if(rc==259) break;
     if(rc==234){if(bytes>64*1024*1024)throw new InvalidOperationException("Value bound");data=new byte[bytes];chars=16384;n.Length=0;rc=RegEnumValue(k.Handle.DangerousGetHandle(),i,n,ref chars,IntPtr.Zero,out type,data,ref bytes);}
     if(rc!=0)throw new Win32Exception(rc);
     if(path==Path && String.Equals(n.ToString(),Name,StringComparison.OrdinalIgnoreCase))continue;
     rows.Add("V:"+path+"\\"+n,type+":"+bytes+":"+Hex(h.ComputeHash(data,0,(int)bytes)));
    }
    foreach(string child in k.GetSubKeyNames()) Visit(root,path+"\\"+child,rows,depth+1);
   }
  }
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegOpenKeyEx(IntPtr key,string name,int options,int access,out IntPtr result);
  static void FullVisit(RegistryKey k,string path,SortedDictionary<string,string> rows,int depth,bool excludeService) {
   if(depth>64 || rows.Count>500000)throw new InvalidOperationException("SYSTEM snapshot bound");
   if(excludeService && path=="ControlSet001\\Services\\MT8167CombinedFollowup184")return;
   rows.Add("K:"+path,"");bool link=false;
   using(SHA256 h=SHA256.Create())for(uint i=0;;i++) {
    StringBuilder n=new StringBuilder(16384);uint chars=16384,type,bytes=4096;byte[] data=new byte[4096];
    int rc=RegEnumValue(k.Handle.DangerousGetHandle(),i,n,ref chars,IntPtr.Zero,out type,data,ref bytes);
    if(rc==259)break;
    if(rc==234){if(bytes>64*1024*1024)throw new InvalidOperationException("Value bound");data=new byte[bytes];chars=16384;n.Length=0;rc=RegEnumValue(k.Handle.DangerousGetHandle(),i,n,ref chars,IntPtr.Zero,out type,data,ref bytes);}
    if(rc!=0)throw new Win32Exception(rc);
    if(type==6 && String.Equals(n.ToString(),"SymbolicLinkValue",StringComparison.OrdinalIgnoreCase))link=true;
    if(path==Path && String.Equals(n.ToString(),Name,StringComparison.OrdinalIgnoreCase))continue;
    rows.Add("V:"+path+"\\"+n,type+":"+bytes+":"+Hex(h.ComputeHash(data,0,(int)bytes)));
   }
   if(link)return; // Retain raw link target; never follow it outside the private hive.
   foreach(string child in k.GetSubKeyNames()) {
    IntPtr handle;int rc=RegOpenKeyEx(k.Handle.DangerousGetHandle(),child,8,0x20019|0x100,out handle);
    if(rc!=0)throw new Win32Exception(rc);
    using(RegistryKey next=RegistryKey.FromHandle(new SafeRegistryHandle(handle,true),RegistryView.Registry64))FullVisit(next,path.Length==0?child:path+"\\"+child,rows,depth+1,excludeService);
   }
  }
  public static string WholeSnapshot(RegistryKey root,bool excludeService) {
   SortedDictionary<string,string> rows=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
   FullVisit(root,"",rows,0,excludeService);
   using(SHA256 h=SHA256.Create()){
    foreach(KeyValuePair<string,string> row in rows){byte[] b=Encoding.UTF8.GetBytes(row.Key.ToUpperInvariant()+"\0"+row.Value+"\n");h.TransformBlock(b,0,b.Length,b,0);}
    h.TransformFinalBlock(new byte[0],0,0);return Hex(h.Hash);
   }
  }
  public static string Snapshot(RegistryKey root) {
   SortedDictionary<string,string> rows=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
   Visit(root,Path,rows,0);
   using(SHA256 h=SHA256.Create()) {
    foreach(KeyValuePair<string,string> row in rows){byte[] b=Encoding.UTF8.GetBytes(row.Key.ToUpperInvariant()+"\0"+row.Value+"\n");h.TransformBlock(b,0,b.Length,b,0);}
    h.TransformFinalBlock(new byte[0],0,0);return Hex(h.Hash);
   }
  }
  static void Binding(RegistryKey root,string id,string service,string version) {
   using(RegistryKey k=Native.OpenPath(root,"ControlSet001\\Enum\\ACPI\\"+id+"\\0",false)) {
    if(k==null || (string)k.GetValue("Service")!=service)throw new InvalidOperationException("Actual binding mismatch");
    string driver=k.GetValue("Driver") as string;
    if(driver==null)throw new InvalidOperationException("Missing selected driver");
    using(RegistryKey d=Native.OpenPath(root,"ControlSet001\\Control\\Class\\"+driver,false)) {
     if(d==null || (string)d.GetValue("DriverVersion")!=version)throw new InvalidOperationException("Actual selected version mismatch");
    }
   }
  }
  public static string Read(RegistryKey root) {
   using(RegistryKey k=Native.OpenPath(root,Path,false)){if(k==null)throw new InvalidOperationException("Missing GraphicsDrivers");return Native.ReadFixedRawValue(k,Name,4,4);}
  }
  // Caller must load a disposable clone of the hash-pinned fresh184 trio, never live HKLM.
  public static string Apply(string mount) {
   using(RegistryKey root=Native.OpenRoot(mount)) return ApplyRoot(root);
  }
  static string ApplyRoot(RegistryKey root) {
   using(RegistryKey s=Native.OpenPath(root,"Select",false)) {
    if(s==null || Native.ReadFixedRawValue(s,"Current",4,4)!="01000000" || Native.ReadFixedRawValue(s,"Default",4,4)!="01000000")throw new InvalidOperationException("Select baseline mismatch");
   }
   Binding(root,"MOU0006","GE8300Render","0.0.3.59");
   Binding(root,"MOU0007","MT8167DisplayTransport","0.0.3.57");
   if(Read(root)!="01000000")throw new InvalidOperationException("One-shot DWORD1 baseline required");
   string before=WholeSnapshot(root,false);
   using(RegistryKey k=Native.OpenPath(root,Path,true)){k.SetValue(Name,0,RegistryValueKind.DWord);k.Flush();}
   if(Read(root)!="00000000" || WholeSnapshot(root,false)!=before)throw new InvalidOperationException("Policy preservation mismatch; discard clone");
   return before;
  }
 }
}
