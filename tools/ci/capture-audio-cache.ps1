# Hash every real compiler-reported include before reusing an ARM audio object.
function Get-CaptureDependencyKey([string]$Compiler,[string[]]$Arguments,[string]$Source,[string]$TracePath){
 $prior=$ErrorActionPreference
 try{$ErrorActionPreference='Continue';$trace=@(& $Compiler @Arguments -H -fsyntax-only $Source 2>&1);$rc=$LASTEXITCODE}finally{$ErrorActionPreference=$prior}
 $trace|ForEach-Object{[string]$_}|Set-Content -Encoding UTF8 $TracePath
 if($rc){throw "Audio dependency/syntax check failed: $Source"}
 $paths=@($Source)
 foreach($line in $trace){if([string]$line -match '^\.+ (.+)$'){if(Test-Path -LiteralPath $Matches[1]){$paths+=[IO.Path]::GetFullPath($Matches[1])}}}
 if($paths.Count -lt 2){throw 'Compiler include trace missing; cannot certify reuse'}
 $deps=@($paths|Sort-Object -Unique|ForEach-Object{@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash}})
 $keyText=($Arguments -join "`n")+"`n"+($deps|ForEach-Object{$_.path+'='+$_.sha256}|Out-String)
 $hash=[Security.Cryptography.SHA256]::Create()
 try{$key=[BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($keyText))).Replace('-','')}finally{$hash.Dispose()}
 return @{key=$key;dependencies=$deps}
}
function Build-CaptureObject([string]$Compiler,[string[]]$Arguments,[string]$Source,[string]$Object){
 $dep=Get-CaptureDependencyKey $Compiler $Arguments $Source ($Object+'.includes.txt')
 $name=[IO.Path]::GetFileName($Object);$prior=$null
 if($script:AudioObjectCache -and $script:AudioObjectCache.compiler -eq $script:AudioCompilerIdentity){
  $prior=$script:AudioObjectCache.units|Where-Object{$_.name -eq $name -and $_.dependency_key -eq $dep.key}|Select-Object -First 1
 }
 $reused=$false
 if($prior -and (Test-Path -LiteralPath $prior.object) -and (Get-FileHash -LiteralPath $prior.object -Algorithm SHA256).Hash -eq $prior.sha256){
  Copy-Item -LiteralPath $prior.object -Destination $Object;$reused=$true
 }else{
  & $Compiler @Arguments -c $Source -o $Object
  if($LASTEXITCODE){throw "Audio object compilation failed: $Source"}
 }
 $script:AudioObjectResults+=@{name=$name;source=$Source;dependency_key=$dep.key;dependencies=$dep.dependencies;reused=$reused;sha256=(Get-FileHash $Object -Algorithm SHA256).Hash}
}
