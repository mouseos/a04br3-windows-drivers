$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$t='C:\mt8167-console-launch363-r2';New-Item -ItemType Directory $t|Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory('C:\mt8167-console-launch363-r2-input.zip',$t)
$j=Get-Content C:\mt8167-console-launch363-r2-input-pins.json -Raw|ConvertFrom-Json
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Sha "$t\dxgi-console-launch363.c") -cne $j.source_sha256 -or (Sha "$t\memory.obj") -cne $j.memory_object_sha256){throw 'Exact source/object pins'}
$llvm='C:\Program Files\LLVM\bin';$kit='C:\mt8167-minimal-service-host\cache\kit81\Include';$libs='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm';$link='C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.29.30133\bin\Hostx64\arm\link.exe'
$f=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',"-I$kit\shared","-I$kit\um",'-Werror=incompatible-pointer-types','-Werror=implicit-function-declaration')
# Redirect native stderr inside Python so harmless SDK warnings do not become
# a PowerShell terminating NativeCommandError.
$env:A04_CONSOLE363_TASK=$t
@'
import os,subprocess,json
from pathlib import Path
t=Path(os.environ['A04_CONSOLE363_TASK']);f=json.loads((t/'flags.json').read_text(encoding='utf-8-sig'))
p=subprocess.run(['C:/Program Files/LLVM/bin/clang.exe',*f,'-c',str(t/'dxgi-console-launch363.c'),'-o',str(t/'launcher.obj')],capture_output=True,text=True);(t/'compile.log').write_text(p.stdout+p.stderr)
if p.returncode:print(p.stderr[-2500:]);raise SystemExit(p.returncode)
'@|Set-Content -Encoding UTF8 "$t\compile.py"
$f|ConvertTo-Json|Set-Content -Encoding UTF8 "$t\flags.json"
& python "$t\compile.py";if($LASTEXITCODE){throw 'Console-launch356 compile'}
& $link /nologo /machine:arm /subsystem:console,6.03 /entry:mainCRTStartup /nodefaultlib /largeaddressaware /tsaware:no "$t\launcher.obj" "$t\memory.obj" "$libs\kernel32.lib" "$libs\advapi32.lib" "$libs\bcrypt.lib" "/out:$t\mt8167-dxgi-console-launch363.exe"
if($LASTEXITCODE){throw 'Console-launch356 link'}
$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 BCBF6C456C6F0DD0818434763B189488FBF13570 "$t\mt8167-dxgi-console-launch363.exe";if($LASTEXITCODE){throw 'Console-launch356 sign'}
& $sign verify /pa "$t\mt8167-dxgi-console-launch363.exe";if($LASTEXITCODE){throw 'Console-launch356 trust'}
$a=Get-AuthenticodeSignature "$t\mt8167-dxgi-console-launch363.exe"
@{source_commit=$j.source_commit;source_sha256=$j.source_sha256;sha256=(Sha "$t\mt8167-dxgi-console-launch363.exe");signer=$a.SignerCertificate.Thumbprint;signature_status=[string]$a.Status;compiled_TUs=1;memory_object_retained=$true;memory_object_sha256=$j.memory_object_sha256;evaluator_sha256=$j.evaluator_sha256;GPU_calls_in_launcher=0;user_token_substitution=$false;console_system_token_duplicate=$true;hardware_verified=$false}|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$t\signed-manifest.json"
Compress-Archive "$t\*" C:\mt8167-console-launch363-r2-output.zip
'CONSOLE_LAUNCH363_SIGNED_HARDWARE_UNVERIFIED'
