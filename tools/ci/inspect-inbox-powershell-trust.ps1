# main offline Windows VM専用。ARM PSを起動せず、exact byteとFILE-choiceの実HRESULTだけ記録。
param([Parameter(Mandatory=$true)][string]$ActualPowerShell,[Parameter(Mandatory=$true)][string]$OfficialPowerShell,[Parameter(Mandatory=$true)][string]$ReportPath)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$pin='8266b956658681fe18f3851384ca8bb7f91a57a93dc29b91fd4519a6eea72052'
foreach($p in @($ActualPowerShell,$OfficialPowerShell)){if((Sha $p) -cne $pin -or (Get-Item -LiteralPath $p).Length -ne 458240){throw 'exact captured/official PS mismatch'}}
if(Test-Path $ReportPath){throw 'new diagnostic report required'}
$b=[IO.File]::ReadAllBytes($ActualPowerShell);$pe=[BitConverter]::ToInt32($b,60)
if([BitConverter]::ToUInt16($b,$pe+4) -ne 0x1c4 -or [BitConverter]::ToUInt16($b,$pe+24) -ne 0x10b -or [BitConverter]::ToUInt32($b,$pe+24+96+36) -ne 0){throw 'actual inbox ARMNT/PE32/no embedded certificate shape required'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace MT8167InboxPsDiagnostic {
 public static class Probe {
  [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct FileInfo {public uint cbStruct;public string path;public IntPtr file;public IntPtr subject;}
  [StructLayout(LayoutKind.Sequential)] struct TrustData {public uint cbStruct;public IntPtr policy,sip;public uint ui,revocation,choice;public IntPtr info;public uint action;public IntPtr state,url;public uint flags,context;public IntPtr settings;}
  [DllImport("wintrust.dll",ExactSpelling=true)] static extern int WinVerifyTrust(IntPtr hwnd,ref Guid action,ref TrustData data);
  public static uint[] FileChoice(string path) {
   FileInfo f=new FileInfo();f.cbStruct=(uint)Marshal.SizeOf(typeof(FileInfo));f.path=path;
   IntPtr ptr=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(FileInfo)));Marshal.StructureToPtr(f,ptr,false);
   TrustData d=new TrustData();d.cbStruct=(uint)Marshal.SizeOf(typeof(TrustData));d.ui=2;d.choice=1;d.info=ptr;d.action=1;d.flags=0x1000;
   Guid g=new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");int status=0,closed=0;
   try{status=WinVerifyTrust(IntPtr.Zero,ref g,ref d);}finally{d.action=2;closed=WinVerifyTrust(IntPtr.Zero,ref g,ref d);Marshal.DestroyStructure(ptr,typeof(FileInfo));Marshal.FreeHGlobal(ptr);}
   return new uint[]{unchecked((uint)status),unchecked((uint)closed)};
  }
 }
}
'@
$hr=[MT8167InboxPsDiagnostic.Probe]::FileChoice([IO.Path]::GetFullPath($ActualPowerShell))
$record=[ordered]@{schema='MT8167_INBOX_PS_FILE_TRUST_DIAGNOSTIC_V1';sha256=$pin;bytes=458240;machine='ARMNT';embedded_certificate_bytes=0;actual_official_bytes_equal=$true;FILE_choice_hresult=('{0:X8}' -f $hr[0]);FILE_choice_close_hresult=('{0:X8}' -f $hr[1]);FILE_choice_returned_zero=($hr[0] -eq 0);context='offline host Windows API on captured ARM file; not trial199 runtime HRESULT';FILE_choice_is_not_launch_authorization=$true;catalog_identity_resolved=$false;PS_launched=$false;PnP_invoked=$false;CI_policy_changed=$false}
[IO.File]::WriteAllText($ReportPath,($record|ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
$record|ConvertTo-Json -Depth 8
