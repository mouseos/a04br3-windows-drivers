// SPDX-License-Identifier: BSD-2-Clause-Patent
// 実在するPnP devnodeだけを操作する。root PDO作成・MMIO・SCM全体停止は行わない。
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

namespace MT8167UsbDeployment {
 public sealed class Node {
  public string Instance, HardwareId, Service, ClassGuid, DriverKey, Inf, Version;
  public string[] LowerFilters;
  public uint DevInst, Status, Problem;
  public bool Started { get { return (Status & 8) != 0; } }
 }
 public static class Pnp {
  // WDK8.1 SetupAPI.h: ARM32はpshpack1。CLRの既定8-byte packingにしない。
  [StructLayout(LayoutKind.Sequential, Pack=1)] struct DevInfo {
   public uint cbSize; public Guid ClassGuid; public uint DevInst; public UIntPtr Reserved;
  }
  [StructLayout(LayoutKind.Sequential, Pack=1, CharSet=CharSet.Unicode)] struct InstallParams {
   public uint cbSize, Flags, FlagsEx; public IntPtr hwndParent, InstallMsgHandler, InstallMsgHandlerContext, FileQueue;
   public UIntPtr ClassInstallReserved; public uint Reserved;
   [MarshalAs(UnmanagedType.ByValTStr, SizeConst=260)] public string DriverPath;
  }
  [StructLayout(LayoutKind.Sequential, Pack=1, CharSet=CharSet.Unicode)] struct DriverInfo {
   public uint cbSize, DriverType; public UIntPtr Reserved;
   [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string Description;
   [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string MfgName;
   [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string ProviderName;
   public System.Runtime.InteropServices.ComTypes.FILETIME DriverDate; public ulong DriverVersion;
  }
  [StructLayout(LayoutKind.Sequential, Pack=1)] struct PropChange {
   public uint cbSize, InstallFunction, StateChange, Scope, HwProfile;
  }
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern IntPtr SetupDiGetClassDevsW(IntPtr cls, string enumerator, IntPtr hwnd, uint flags);
  [DllImport("setupapi.dll", SetLastError=true)] static extern IntPtr SetupDiCreateDeviceInfoList(IntPtr cls, IntPtr hwnd);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiEnumDeviceInfo(IntPtr set, uint index, ref DevInfo info);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiOpenDeviceInfoW(IntPtr set, string instance, IntPtr hwnd, uint flags, ref DevInfo info);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiGetDeviceRegistryPropertyW(IntPtr set, ref DevInfo info, uint prop, out uint type, byte[] data, uint bytes, out uint needed);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiGetDeviceInstanceIdW(IntPtr set, ref DevInfo info, StringBuilder value, uint chars, out uint needed);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupCopyOEMInfW(string source, string media, uint sourceType, uint style, StringBuilder destination, uint chars, out uint needed, IntPtr component);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiGetDeviceInstallParamsW(IntPtr set, ref DevInfo info, ref InstallParams p);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiSetDeviceInstallParamsW(IntPtr set, ref DevInfo info, ref InstallParams p);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiBuildDriverInfoList(IntPtr set, ref DevInfo info, uint type);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiEnumDriverInfoW(IntPtr set, ref DevInfo info, uint type, uint index, ref DriverInfo driver);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiSetSelectedDriverW(IntPtr set, ref DevInfo info, ref DriverInfo driver);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiDestroyDriverInfoList(IntPtr set, ref DevInfo info, uint type);
  [DllImport("setupapi.dll", SetLastError=true)] static extern bool SetupDiCallClassInstaller(uint function, IntPtr set, ref DevInfo info);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool SetupDiSetClassInstallParamsW(IntPtr set, ref DevInfo info, ref PropChange p, uint size);
  [DllImport("cfgmgr32.dll")] static extern uint CM_Get_DevNode_Status(out uint status, out uint problem, uint devinst, uint flags);
  [DllImport("cfgmgr32.dll")] static extern uint CM_Get_Parent(out uint parent, uint child, uint flags);
  [DllImport("cfgmgr32.dll")] static extern uint CM_Reenumerate_DevNode(uint devinst, uint flags);

  static DevInfo Info() { DevInfo d=new DevInfo(); d.cbSize=(uint)Marshal.SizeOf(typeof(DevInfo)); return d; }
  public static void AssertArmAbi() {
   if(IntPtr.Size!=4 || Marshal.SizeOf(typeof(DevInfo))!=28 || Marshal.SizeOf(typeof(InstallParams))!=556 ||
      Marshal.SizeOf(typeof(DriverInfo))!=1564 || Marshal.SizeOf(typeof(PropChange))!=20)
    throw new InvalidOperationException("WDK8.1 ARM32 SetupAPI packing不一致");
  }
  static void Check(bool ok) { if(!ok)throw new Win32Exception(Marshal.GetLastWin32Error()); }
  static bool Eq(string a,string b) { return String.Equals(a,b,StringComparison.OrdinalIgnoreCase); }
  static string[] Property(IntPtr set,ref DevInfo d,uint id,bool optional) {
   uint type, needed; byte[] bytes=new byte[65536];
   if(!SetupDiGetDeviceRegistryPropertyW(set,ref d,id,out type,bytes,(uint)bytes.Length,out needed)) {
    int error=Marshal.GetLastWin32Error(); if(optional && (error==13 || error==2))return new string[0];
    throw new Win32Exception(error);
   }
   if((type!=1 && type!=7) || needed>bytes.Length || (needed&1)!=0)throw new InvalidOperationException("PnP property型不一致");
   return Encoding.Unicode.GetString(bytes,0,(int)needed).Split(new char[]{'\0'},StringSplitOptions.RemoveEmptyEntries);
  }
  static string First(string[] s) { return s.Length==0?"":s[0]; }
  static bool Descendant(uint child,uint parent) {
   for(int i=0;i<32;i++){uint next;if(CM_Get_Parent(out next,child,0)!=0)return false;if(next==parent)return true;child=next;}
   throw new InvalidOperationException("PnP祖先深度超過");
  }
  static Node Read(IntPtr set,ref DevInfo d,string hwid) {
   Node n=new Node(); uint needed; StringBuilder instance=new StringBuilder(1024);
   Check(SetupDiGetDeviceInstanceIdW(set,ref d,instance,(uint)instance.Capacity,out needed));
   n.Instance=instance.ToString();n.DevInst=d.DevInst;n.HardwareId=hwid;n.ClassGuid=d.ClassGuid.ToString("B");
   n.Service=First(Property(set,ref d,4,true));n.DriverKey=First(Property(set,ref d,9,true));
   n.LowerFilters=Property(set,ref d,0x12,true);
   if(CM_Get_DevNode_Status(out n.Status,out n.Problem,d.DevInst,0)!=0)throw new InvalidOperationException("PnP status取得失敗");
   n.Inf="";n.Version="";
   if(n.DriverKey!="")using(RegistryKey k=Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\Class\"+n.DriverKey)) {
    if(k==null)throw new InvalidOperationException("選択driver key欠落");
    n.Inf=Convert.ToString(k.GetValue("InfPath",""));n.Version=Convert.ToString(k.GetValue("DriverVersion",""));
   }
   return n;
  }
  public static Node Find(string hwid,uint parent) {
   IntPtr set=SetupDiGetClassDevsW(IntPtr.Zero,null,IntPtr.Zero,6);
   if(set==new IntPtr(-1))throw new Win32Exception();Node result=null;
   try { for(uint i=0;;i++) {
    DevInfo d=Info();if(!SetupDiEnumDeviceInfo(set,i,ref d)){int e=Marshal.GetLastWin32Error();if(e==259)break;throw new Win32Exception(e);}
    string[] ids=Property(set,ref d,1,true);bool match=false;foreach(string id in ids)if(Eq(id,hwid))match=true;
    if(!match || (parent!=0 && !Descendant(d.DevInst,parent)))continue;
    if(result!=null)throw new InvalidOperationException("HWID/親に一致するPDOが複数存在");result=Read(set,ref d,hwid);
   }}finally{SetupDiDestroyDeviceInfoList(set);}return result;
  }
  public static string Stage(string inf) {
   StringBuilder published=new StringBuilder(1024);uint needed;
   Check(SetupCopyOEMInfW(inf,System.IO.Path.GetDirectoryName(inf),1,0,published,(uint)published.Capacity,out needed,IntPtr.Zero));
   return published.ToString();
  }
  static IntPtr Open(Node n,out DevInfo d) {
   IntPtr set=SetupDiCreateDeviceInfoList(IntPtr.Zero,IntPtr.Zero);if(set==new IntPtr(-1))throw new Win32Exception();d=Info();
   if(!SetupDiOpenDeviceInfoW(set,n.Instance,IntPtr.Zero,0,ref d)){int e=Marshal.GetLastWin32Error();SetupDiDestroyDeviceInfoList(set);throw new Win32Exception(e);}return set;
  }
  static InstallParams Params(IntPtr set,ref DevInfo d) {
   InstallParams p=new InstallParams();p.cbSize=(uint)Marshal.SizeOf(typeof(InstallParams));p.DriverPath="";
   Check(SetupDiGetDeviceInstallParamsW(set,ref d,ref p));return p;
  }
  public static bool Bind(Node n,string inf,ulong version,bool deferConfig) {
   DevInfo d;IntPtr set=Open(n,out d);bool list=false;
   try {
    InstallParams p=Params(set,ref d);p.Flags|=0x10000u;if(deferConfig)p.Flags|=0x20000u;p.DriverPath=System.IO.Path.GetFullPath(inf);
    Check(SetupDiSetDeviceInstallParamsW(set,ref d,ref p));Check(SetupDiBuildDriverInfoList(set,ref d,2));list=true;
    DriverInfo chosen=new DriverInfo();int matches=0;
    for(uint i=0;;i++) {
     DriverInfo item=new DriverInfo();item.cbSize=(uint)Marshal.SizeOf(typeof(DriverInfo));
     if(!SetupDiEnumDriverInfoW(set,ref d,2,i,ref item)){int e=Marshal.GetLastWin32Error();if(e==259)break;throw new Win32Exception(e);}
     if(item.DriverVersion==version){chosen=item;matches++;}
    }
    if(matches!=1)throw new InvalidOperationException("指定INF/versionのdriverが一意でない");
    Check(SetupDiSetSelectedDriverW(set,ref d,ref chosen));Check(SetupDiCallClassInstaller(2,set,ref d));
    p=Params(set,ref d);return (p.Flags&0x180u)!=0;
   }finally{if(list)SetupDiDestroyDriverInfoList(set,ref d,2);SetupDiDestroyDeviceInfoList(set);}
  }
  public static bool Change(Node n,uint state) {
   if(state!=1 && state!=2 && state!=3)throw new ArgumentException("PnP state不正");
   DevInfo d;IntPtr set=Open(n,out d);
   try {PropChange p=new PropChange();p.cbSize=8;p.InstallFunction=0x12;p.StateChange=state;p.Scope=state==3?2u:1u;
    Check(SetupDiSetClassInstallParamsW(set,ref d,ref p,(uint)Marshal.SizeOf(typeof(PropChange))));
    Check(SetupDiCallClassInstaller(0x12,set,ref d));InstallParams result=Params(set,ref d);return (result.Flags&0x180u)!=0;
   }finally{SetupDiDestroyDeviceInfoList(set);}
  }
  public static void Rescan(Node parent) {
   uint r=CM_Reenumerate_DevNode(parent.DevInst,1);if(r!=0)throw new InvalidOperationException("親再列挙失敗: "+r);
  }
  public static ulong Version(string version) {
   string[] p=version.Split('.');if(p.Length!=4)throw new ArgumentException("4要素versionが必要");ulong result=0;
   foreach(string part in p){uint v=UInt32.Parse(part);if(v>65535)throw new ArgumentException("version範囲外");result=(result<<16)|v;}return result;
  }
  public static string DeploymentState(Node legacy,Node hcd) {
   if(legacy!=null && legacy.Started)throw new InvalidOperationException("旧providerが開始したまま");
   if(hcd==null)return "await-host-role-or-parent-restart-not-hardware-success";
   if(!Eq(hcd.Service,"MT8167MUSBHcd") || !Eq(hcd.ClassGuid,"{36fc9e60-c465-11cf-8056-444553540000}") ||
      hcd.Version!="2026.11.10.1019" || hcd.LowerFilters==null || hcd.LowerFilters.Length!=0)
    throw new InvalidOperationException("新HCD選択tuple不一致");
   if(!hcd.Started || hcd.Problem!=0)return "hcd-selected-start-unverified";
   return "HCD-selected-and-started-power-delivery-unverified";
  }
 }
}
