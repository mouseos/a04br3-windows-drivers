param([Parameter(Mandatory=$true)][string]$Directory,[Parameter(Mandatory=$true)][string]$OutputFile)
$ErrorActionPreference='Stop'
if(Test-Path $OutputFile){throw 'output exists'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign=Join-Path $kit 'bin\x86\signtool.exe'
$root=(Resolve-Path $Directory).Path
$pairs=@(@('toprgu','mt8167-toprgu.cat','mt8167-toprgu.sys','mt8167-toprgu.inf'),@('parent','shared-parent.cat','mt8167-shared.sys','shared-parent.inf'),@('touch','bu21025.cat','bu21025.sys','bu21025.inf'))
$seen=@();$hashes=@{};$index=0
foreach($pair in $pairs){
 $dir=Join-Path $root $pair[0];$cat=Join-Path $dir $pair[1]
 foreach($name in @($pair[2],$pair[3])){
  $file=Join-Path $dir $name;$out=Join-Path $env:RUNNER_TEMP "a04-tx-sig-$index.out";$err=Join-Path $env:RUNNER_TEMP "a04-tx-sig-$index.err";$index++
  $proc=Start-Process -FilePath $sign -ArgumentList @('verify','/v','/pa','/c',"`"$cat`"","`"$file`"") -Wait -PassThru -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err
  $text=(Get-Content -Raw $out)+(Get-Content -Raw $err)
  if($text -notmatch 'File is signed in catalog:' -or ($proc.ExitCode -ne 0 -and $text -notmatch 'certificate which is not trusted by the trust provider')){throw "catalog membership not established: $($pair[0])/$name"}
  $seen+=@{package=$pair[0];name=$name;catalog=$pair[1];exit_code=$proc.ExitCode;untrusted_root=($proc.ExitCode -ne 0)}
 }
 foreach($name in @($pair[1],$pair[2],$pair[3])){$hashes[($pair[0]+'/'+$name)]=(Get-FileHash -Algorithm SHA256 (Join-Path $dir $name)).Hash.ToLowerInvariant()}
}
[ordered]@{version=1;hashes=$hashes;observations=$seen;scope='catalog membership only; trust failures and RT CI policy are not success'} | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $OutputFile
Write-Host 'TRANSACTION_CATALOG_MEMBERSHIP_RECORDED_RT_POLICY_UNPROVEN'
