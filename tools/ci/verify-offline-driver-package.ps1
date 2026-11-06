param([Parameter(Mandatory=$true)][string]$ToprguDirectory,
      [Parameter(Mandatory=$true)][string]$SharedDirectory,
      [Parameter(Mandatory=$true)][string]$OutputFile)
$ErrorActionPreference='Stop'
if(Test-Path $OutputFile){throw 'Output already exists'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$signtool=Join-Path $kit 'bin\x86\signtool.exe'
$top=(Resolve-Path $ToprguDirectory).Path;$shared=(Resolve-Path $SharedDirectory).Path
$certPaths=@((Join-Path $top 'mt8167-toprgu.cer'),(Join-Path $shared 'shared-touch-test.cer'))
$pairs=@(
 @($top,'mt8167-toprgu.cat','mt8167-toprgu.sys','mt8167-toprgu.inf'),
 @((Join-Path $shared 'parent'),'shared-parent.cat','mt8167-shared.sys','shared-parent.inf'),
 @((Join-Path $shared 'touch'),'bu21025.cat','bu21025.sys','bu21025.inf')
)
$hashes=@{};$observed=@();$index=0
foreach($p in $pairs){
 $cat=Join-Path $p[0] $p[1]
 foreach($name in @($p[2],$p[3])){
  $file=Join-Path $p[0] $name;$out=Join-Path $env:RUNNER_TEMP "a04-sign-out-$index.txt";$err=Join-Path $env:RUNNER_TEMP "a04-sign-err-$index.txt";$index++
  $proc=Start-Process -FilePath $signtool -ArgumentList @('verify','/v','/pa','/c',"`"$cat`"","`"$file`"") -Wait -PassThru -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err
  $text=(Get-Content -Raw $out)+(Get-Content -Raw $err)
  if($text -notmatch 'File is signed in catalog:' -or ($proc.ExitCode -ne 0 -and $text -notmatch 'certificate which is not trusted by the trust provider')){throw "Catalog membership rejected: $name"}
  $observed+=@{name=$name;exit_code=$proc.ExitCode;untrusted_root=($proc.ExitCode -ne 0)}
 }
 foreach($name in @($p[1],$p[2],$p[3])){$file=Join-Path $p[0] $name;$hashes[$name]=(Get-FileHash -Algorithm SHA256 $file).Hash.ToLowerInvariant()}
}
$certs=@();foreach($path in $certPaths){$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($path);$certs+=$cert;$hashes[(Split-Path $path -Leaf)]=(Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()}
$result=[ordered]@{version=1;hashes=$hashes;thumbprints=@($certs | ForEach-Object {$_.Thumbprint});catalog_membership_verified=$true;observations=$observed;scope='membership only; untrusted-root failures and RT policy are not success'}
$result | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $OutputFile
Write-Host 'OFFLINE_DRIVER_CATALOG_MEMBERSHIP_OBSERVED'
