// SPDX-License-Identifier: BSD-2-Clause-Patent
// 7ec02fc5 existing-key backup/restore opening, separated for clone-only inventory.
// No registry value/key writer is provided; writable OpenPath requests are rejected.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;

namespace MT8167OfflineHive {
    public sealed class Privileges : IDisposable {
        [StructLayout(LayoutKind.Sequential)] struct Luid { public uint Low; public int High; }
        [StructLayout(LayoutKind.Sequential)] struct TokenPrivileges { public uint Count; public Luid Id; public uint Attributes; }
        [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool CloseHandle(IntPtr handle);
        [DllImport("advapi32.dll", SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
        [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool LookupPrivilegeValue(string system,string name,out Luid luid);
        [DllImport("advapi32.dll", SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr token,bool disable,ref TokenPrivileges state,uint size,out TokenPrivileges previous,out uint needed);
        IntPtr token;
        readonly List<TokenPrivileges> previous = new List<TokenPrivileges>();
        public int RestoreError { get; private set; }
        public Privileges() {
            if (!OpenProcessToken(GetCurrentProcess(),0x28,out token)) throw new Win32Exception(Marshal.GetLastWin32Error());
            try {
                foreach (string name in new string[] {"SeBackupPrivilege","SeRestorePrivilege"}) {
                    Luid id; if (!LookupPrivilegeValue(null,name,out id)) throw new Win32Exception(Marshal.GetLastWin32Error());
                    TokenPrivileges state = new TokenPrivileges(); state.Count=1;state.Id=id;state.Attributes=2;
                    TokenPrivileges old; uint needed;
                    bool ok=AdjustTokenPrivileges(token,false,ref state,(uint)Marshal.SizeOf(typeof(TokenPrivileges)),out old,out needed);
                    int error=Marshal.GetLastWin32Error();
                    if (!ok || error!=0) throw new Win32Exception(error==0 ? 1300 : error);
                    previous.Add(old);
                }
            } catch { Dispose(); throw; }
        }
        public void Dispose() {
            if (token==IntPtr.Zero) return;
            for (int i=previous.Count-1;i>=0;--i) {
                TokenPrivileges old=previous[i], unused; uint needed;
                if (old.Count!=0) {
                    bool ok=AdjustTokenPrivileges(token,false,ref old,(uint)Marshal.SizeOf(typeof(TokenPrivileges)),out unused,out needed);
                    int error=Marshal.GetLastWin32Error();
                    if ((!ok || error!=0) && RestoreError==0) RestoreError=error==0 ? 1300 : error;
                }
            }
            CloseHandle(token);token=IntPtr.Zero;
        }
    }
    public static class Native {
        static readonly HashSet<string> PrivateMounts = new HashSet<string>(StringComparer.Ordinal);
        static readonly HashSet<IntPtr> PrivateRoots = new HashSet<IntPtr>();
        static readonly IntPtr HKLM = new IntPtr(unchecked((int)0x80000002));
        const int Read=0x20019, Write=0x20006, OpenLink=8;
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegLoadKey(IntPtr key,string name,string file);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegUnLoadKey(IntPtr key,string name);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegOpenKeyEx(IntPtr key,string name,int options,int access,out IntPtr result);
        [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr key);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegCreateKeyEx(IntPtr key,string name,int reserved,string cls,int options,int access,IntPtr security,out IntPtr result,out int disposition);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegQueryValueEx(IntPtr key,string name,IntPtr reserved,out uint type,byte[] data,ref uint bytes);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegEnumValue(IntPtr key,uint index,StringBuilder name,ref uint chars,IntPtr reserved,out uint type,byte[] data,ref uint bytes);
        public static int Load(string name,string file) {
            if(!System.Text.RegularExpressions.Regex.IsMatch(name,@"^MT8167OfflineSYSTEM_[a-f0-9]{32}$")) throw new ArgumentException("Invalid private mount");
            string full=System.IO.Path.GetFullPath(file);
            if(System.IO.Path.GetFileName(full)!="SYSTEM" || new System.IO.DirectoryInfo(System.IO.Path.GetDirectoryName(full)).Name!="clone") throw new ArgumentException("Only copied clone/SYSTEM may be loaded");
            int rc=RegLoadKey(HKLM,name,full);if(rc==0)PrivateMounts.Add(name);return rc;
        }
        public static int Unload(string name) {
            if(!PrivateMounts.Contains(name)) throw new ArgumentException("Only our loaded clone may be unloaded");
            int rc=RegUnLoadKey(HKLM,name);if(rc==0){PrivateMounts.Remove(name);PrivateRoots.Clear();}return rc;
        }
        static void CheckLink(IntPtr key) {
            uint type, bytes=0; int rc=RegQueryValueEx(key,"SymbolicLinkValue",IntPtr.Zero,out type,null,ref bytes);
            if ((rc==0 || rc==234) && type==6) throw new InvalidOperationException("Registry link in the modification path");
            if (rc!=0 && rc!=2 && rc!=234) throw new Win32Exception(rc);
        }
        public static RegistryKey OpenRoot(string name) {
            if (!name.StartsWith("MT8167OfflineSYSTEM_",StringComparison.Ordinal) || name.Length!="MT8167OfflineSYSTEM_".Length+32)
                throw new ArgumentException("Invalid private hive mount name");
            if(!PrivateMounts.Contains(name)) throw new ArgumentException("Only our loaded clone may be opened");
            IntPtr h; int rc=RegOpenKeyEx(HKLM,name,OpenLink,Read|0x100,out h);
            if(rc!=0) throw new Win32Exception(rc);
            try { CheckLink(h); RegistryKey result=RegistryKey.FromHandle(new SafeRegistryHandle(h,true),RegistryView.Registry64);PrivateRoots.Add(h);return result; }
            catch { RegCloseKey(h); throw; }
        }
        public static RegistryKey OpenPath(RegistryKey root,string path,bool writable) {
            if(!PrivateRoots.Contains(root.Handle.DangerousGetHandle())) throw new ArgumentException("Only our private clone root is allowed");
            if(writable) throw new InvalidOperationException("Inventory does not allow writable paths");
            bool allowed=path=="Select" || path=="ControlSet001";
            foreach(string prefix in new string[]{"ControlSet001\\Services","ControlSet001\\Enum","ControlSet001\\Control\\Class","ControlSet001\\Control\\GraphicsDrivers","ControlSet001\\Control\\Video"})
                if(path==prefix || path.StartsWith(prefix+"\\",StringComparison.Ordinal)) allowed=true;
            if(!allowed) throw new ArgumentException("Bounded inventory path required");
            RegistryKey current=null;
            try {
                foreach(string part in path.Split('\\')) {
                    if(part.Length==0 || part=="." || part=="..") throw new ArgumentException("Invalid relative key");
                    // Only an existing, enumerated direct child in our private clone.
                    // No ACL edits, ownership changes, missing-key creation or link following.
                    RegistryKey parent=current==null ? root : current;
                    bool exists=false;
                    foreach(string child in parent.GetSubKeyNames())
                        if(String.Equals(child,part,StringComparison.OrdinalIgnoreCase)) { exists=true;break; }
                    if(!exists) { if(current!=null) current.Dispose();return null; }
                    IntPtr h;int disposition;
                    // RegOpenKeyEx does not apply SeBackup/SeRestorePrivilege to ACL checks.
                    // REG_OPTION_BACKUP_RESTORE honors the already enabled privileges;
                    // OPEN_LINK plus CheckLink retains explicit registry-link rejection.
                    int rc=RegCreateKeyEx(parent.Handle.DangerousGetHandle(),part,0,null,4|OpenLink,0,IntPtr.Zero,out h,out disposition);
                    if(rc!=0) throw new Win32Exception(rc,"Backup/restore open failed: "+path);
                    if(disposition!=2) { RegCloseKey(h);throw new InvalidOperationException("Expected existing private clone key: "+path); }
                    RegistryKey next;
                    try { CheckLink(h); next=RegistryKey.FromHandle(new SafeRegistryHandle(h,true),RegistryView.Registry64); }
                    catch { RegCloseKey(h); throw; }
                    if(current!=null) current.Dispose();current=next;
                }
                RegistryKey result=current;current=null;return result;
            } finally { if(current!=null) current.Dispose(); }
        }
        public sealed class RawValue {
            public bool Present; public uint Type; public int Bytes; public string RawHex;
        }
        public static RawValue ReadRawValue(RegistryKey key,string name,int maximumBytes) {
            if(maximumBytes<0 || maximumBytes>65536) throw new ArgumentOutOfRangeException("maximumBytes");
            uint type,bytes=0;int rc=RegQueryValueEx(key.Handle.DangerousGetHandle(),name,IntPtr.Zero,out type,null,ref bytes);
            if(rc==2)return new RawValue{Present=false};
            if(rc!=0)throw new Win32Exception(rc);
            if(type==6 || bytes>maximumBytes)throw new InvalidOperationException("Raw value type/size bound");
            uint initialType=type,initialBytes=bytes;byte[] data=new byte[bytes];
            rc=RegQueryValueEx(key.Handle.DangerousGetHandle(),name,IntPtr.Zero,out type,data,ref bytes);
            if(rc!=0)throw new Win32Exception(rc);
            if(type!=initialType || bytes!=initialBytes)throw new InvalidOperationException("Value changed during read");
            return new RawValue{Present=true,Type=type,Bytes=(int)bytes,RawHex=Hex(data)};
        }
        public static string ReadFixedRawValue(RegistryKey key,string name,uint expectedType,int expectedBytes) {
            RawValue value=ReadRawValue(key,name,65536);
            if(!value.Present || value.Type!=expectedType || value.Bytes!=expectedBytes)throw new InvalidOperationException("Fixed raw type/size mismatch");
            return value.RawHex;
        }
        static string Hex(byte[] data) { return BitConverter.ToString(data).Replace("-","").ToLowerInvariant(); }
        // Deliberately shallow: raw values of an explicitly opened related key.
        // Never enumerate child keys or traverse the full offline SYSTEM hive.
        public static string ValueFingerprint(RegistryKey key) {
            SortedDictionary<string,string> values=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            using(SHA256 hash=SHA256.Create()) {
                for(uint i=0;;++i) {
                    StringBuilder name=new StringBuilder(16384);uint chars=16384,type,bytes=4096;byte[] data=new byte[4096];
                    int rc=RegEnumValue(key.Handle.DangerousGetHandle(),i,name,ref chars,IntPtr.Zero,out type,data,ref bytes);
                    if(rc==259) break;
                    if(rc==234) {
                        if(bytes>64*1024*1024) throw new InvalidOperationException("Registry value limit");
                        data=new byte[bytes];chars=16384;name.Length=0;
                        rc=RegEnumValue(key.Handle.DangerousGetHandle(),i,name,ref chars,IntPtr.Zero,out type,data,ref bytes);
                    }
                    if(rc!=0) throw new Win32Exception(rc);
                    values.Add(name.ToString(),type+":"+bytes+":"+Hex(hash.ComputeHash(data,0,(int)bytes)));
                }
                foreach(KeyValuePair<string,string> item in values) {
                    byte[] b=Encoding.UTF8.GetBytes(item.Key.ToUpperInvariant()+"\0"+item.Value+"\n");
                    hash.TransformBlock(b,0,b.Length,b,0);
                }
                hash.TransformFinalBlock(new byte[0],0,0);return Hex(hash.Hash);
            }
        }
    }
}
