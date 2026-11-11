// SPDX-License-Identifier: BSD-2-Clause-Patent
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;

namespace MT8167Usb1121OfflineHive {
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
    public sealed class RawValue {
        public uint type { get; set; }
        public int bytes { get; set; }
        public string raw_hex { get; set; }
    }
    public static class Native {
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
        public static int Load(string name,string file) { return RegLoadKey(HKLM,name,file); }
        public static int Unload(string name) { return RegUnLoadKey(HKLM,name); }
        static void CheckLink(IntPtr key) {
            uint type, bytes=0; int rc=RegQueryValueEx(key,"SymbolicLinkValue",IntPtr.Zero,out type,null,ref bytes);
            if ((rc==0 || rc==234) && type==6) throw new InvalidOperationException("Registry link in the modification path");
            if (rc!=0 && rc!=2 && rc!=234) throw new Win32Exception(rc);
        }
        public static RegistryKey OpenRoot(string name) {
            if (!name.StartsWith("MT8167OfflineSYSTEM_",StringComparison.Ordinal) || name.Length!="MT8167OfflineSYSTEM_".Length+32)
                throw new ArgumentException("Invalid private hive mount name");
            IntPtr h; int rc=RegOpenKeyEx(HKLM,name,OpenLink,Read|Write|0x100,out h);
            if(rc!=0) throw new Win32Exception(rc);
            try { CheckLink(h); RegistryKey result=RegistryKey.FromHandle(new SafeRegistryHandle(h,true),RegistryView.Registry64);PrivateRoots.Add(h);return result; }
            catch { RegCloseKey(h); throw; }
        }
        public static RegistryKey OpenPath(RegistryKey root,string path,bool writable) {
            if(!PrivateRoots.Contains(root.Handle.DangerousGetHandle())) throw new ArgumentException("Only our private clone root is allowed");
            if(path!="Select" && path!="ControlSet001\\Services" && !path.StartsWith("ControlSet001\\Services\\",StringComparison.Ordinal) && path!="ControlSet001\\Control\\GraphicsDrivers" && !path.StartsWith("ControlSet001\\Control\\GraphicsDrivers\\",StringComparison.Ordinal)) throw new ArgumentException("USB198 private path allowlist");
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
        static string Hex(byte[] data) { return BitConverter.ToString(data).Replace("-","").ToLowerInvariant(); }
        [DllImport("advapi32.dll")] static extern int RegGetKeySecurity(IntPtr key,uint information,byte[] descriptor,ref uint bytes);
        public static string SecurityFingerprint(RegistryKey key) {
            uint bytes=0;int rc=RegGetKeySecurity(key.Handle.DangerousGetHandle(),7,null,ref bytes);
            if(rc!=122 || bytes==0 || bytes>1024*1024) throw new Win32Exception(rc);
            byte[] data=new byte[bytes];rc=RegGetKeySecurity(key.Handle.DangerousGetHandle(),7,data,ref bytes);
            if(rc!=0) throw new Win32Exception(rc);
            using(SHA256 hash=SHA256.Create()) return Hex(hash.ComputeHash(data,0,(int)bytes));
        }
        public static SortedDictionary<string,RawValue> RawValues(RegistryKey key) {
            SortedDictionary<string,RawValue> values=new SortedDictionary<string,RawValue>(StringComparer.OrdinalIgnoreCase);
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
                byte[] raw=new byte[bytes];Array.Copy(data,raw,bytes);
                values.Add(name.ToString(),new RawValue { type=type,bytes=(int)bytes,raw_hex=Hex(raw) });
            }
            return values;
        }
        static void Expect(IDictionary<string,RawValue> values,string name,uint type,byte[] raw) {
            RawValue v;
            if(!values.TryGetValue(name,out v) || v.type!=type || v.bytes!=raw.Length || v.raw_hex!=Hex(raw))
                throw new InvalidOperationException("USB service raw identity mismatch: "+name);
        }
        public static void VerifyServiceIdentity(IDictionary<string,RawValue> values,int start) {
            Expect(values,"Type",4,BitConverter.GetBytes(16));
            Expect(values,"Start",4,BitConverter.GetBytes(start));
            Expect(values,"ErrorControl",4,BitConverter.GetBytes(1));
            Expect(values,"ImagePath",1,Encoding.Unicode.GetBytes("\"C:\\Windows\\System32\\MT8167Usb1121Install198.exe\"\0"));
            Expect(values,"ObjectName",1,Encoding.Unicode.GetBytes("LocalSystem\0"));
            Expect(values,"DisplayName",1,Encoding.Unicode.GetBytes("MediaTek MT8167 USB ABI3 Installation\0"));
        }
        // Startの除外はsnapshotの厳密なservice rootでだけ指定する。子キーは除外しない。
        public static string ValueFingerprint(RegistryKey key) { return FingerprintValues(RawValues(key),null); }
        public static string ValueFingerprintExceptStart(RegistryKey key) { return FingerprintValues(RawValues(key),"Start"); }
        public static string FingerprintValues(IDictionary<string,RawValue> raw,string excluded) {
            SortedDictionary<string,string> values=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            using(SHA256 hash=SHA256.Create()) {
                foreach(KeyValuePair<string,RawValue> item in raw) {
                    if(String.Equals(item.Key,excluded,StringComparison.OrdinalIgnoreCase)) continue;
                    RawValue v=item.Value;byte[] data=new byte[v.bytes];
                    if(v.raw_hex.Length!=v.bytes*2) throw new InvalidOperationException("Invalid raw length");
                    for(int i=0;i<data.Length;++i) data[i]=Convert.ToByte(v.raw_hex.Substring(i*2,2),16);
                    values.Add(item.Key,v.type+":"+v.bytes+":"+Hex(hash.ComputeHash(data)));
                }
                foreach(KeyValuePair<string,string> item in values) {
                    byte[] data=Encoding.UTF8.GetBytes(item.Key.ToUpperInvariant()+"\0"+item.Value+"\n");
                    hash.TransformBlock(data,0,data.Length,data,0);
                }
                hash.TransformFinalBlock(new byte[0],0,0);return Hex(hash.Hash);
            }
        }
    }
}
