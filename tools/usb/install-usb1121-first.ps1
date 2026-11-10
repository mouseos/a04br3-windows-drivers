# 実行はmain所有。GPU用run197 profileとは独立したUSB専用transaction。
param(
 [Parameter(Mandatory=$true)][ValidateSet('Validate','Apply','Resume','Rollback')][string]$Mode,
 [Parameter(Mandatory=$true)][string]$BundleDirectory,
 [Parameter(Mandatory=$true)][string]$ExpectedPlanSha256,
 [Parameter(Mandatory=$true)][string]$ToolManifest,
 [Parameter(Mandatory=$true)][string]$ExpectedToolManifestSha256,
 [Parameter(Mandatory=$true)][string]$TransactionDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Require($ok,$message){if(!$ok){throw $message}}
function CheckHash($p,$h){Require ($h -cmatch '^[0-9a-f]{64}$' -and (Sha $p) -ceq $h) "Hash不一致: $p"}
function ChildPath($base,$relative){
 $p=[IO.Path]::GetFullPath((Join-Path $base $relative));Require ($p.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) 'bundle外参照'
 Require (!((Get-Item -LiteralPath $p).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'reparse pointは使用しない';return $p
}
$bundle=[IO.Path]::GetFullPath($BundleDirectory).TrimEnd('\')
$transaction=[IO.Path]::GetFullPath($TransactionDirectory).TrimEnd('\')
CheckHash (Join-Path $bundle 'plan.json') $ExpectedPlanSha256
$plan=Get-Content (Join-Path $bundle 'plan.json') -Raw|ConvertFrom-Json
Require ($plan.schema -ceq 'MT8167_USB_ABI3_FIRST_INSTALL_V1' -and $plan.owner_ABI -eq 3 -and
 !$plan.root_PDO_creation -and !$plan.force_host -and !$plan.source_ON) '専用USB ABI3契約が必要'
foreach($f in $plan.files.PSObject.Properties){CheckHash (ChildPath $bundle $f.Name) $f.Value}
CheckHash $PSCommandPath $plan.files.'recipe/install-usb1121-first.ps1'
CheckHash $ToolManifest $ExpectedToolManifestSha256
$tool=Get-Content $ToolManifest -Raw|ConvertFrom-Json
Require ($tool.schema -ceq 'MT8167_USB_ABI3_PNP_HELPER_V1' -and $tool.source_sha256 -ceq $plan.files.'recipe/UsbAbi3Pnp.cs' -and $tool.platform -ceq 'ARM') 'helper source/ARM不一致'
$dll=Join-Path (Split-Path $ToolManifest) 'MT8167UsbAbi3Pnp.dll';CheckHash $dll $tool.dll_sha256
$signature=Get-AuthenticodeSignature -LiteralPath $dll
Require ($signature.Status -eq 'Valid' -and $signature.SignerCertificate.Thumbprint -ceq $plan.tool_signer) '署名helper不一致'
Require ([IntPtr]::Size -eq 4 -and $env:PROCESSOR_ARCHITECTURE -ceq 'ARM' -and $env:windir -and
 [Environment]::OSVersion.Version.Major -eq 6 -and [Environment]::OSVersion.Version.Minor -eq 3) '実Windows RT8.1 ARM32でのみ実行'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
Require ((New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) '管理者tokenが必要'
Add-Type -Path $dll
[MT8167UsbDeployment.Pnp]::AssertArmAbi()
function FindParent { $n=[MT8167UsbDeployment.Pnp]::Find('ACPI\BEN0001',0);Require ($null -ne $n) '実parent欠落';return $n }
function FindLegacy($p){return [MT8167UsbDeployment.Pnp]::Find('MT8167\USB20',$p.DevInst)}
function FindHcd($p){return [MT8167UsbDeployment.Pnp]::Find('MT8167\USBPORT20',$p.DevInst)}
function CheckNode($n,$e){Require ($null -ne $n -and $n.Service -ieq $e.service -and $n.ClassGuid -ieq $e.class_guid -and $n.Version -ceq $e.version) '選択HWID/service/class/version不一致'}
function InstalledSys($e){return Join-Path $env:windir ('System32\drivers\'+[IO.Path]::GetFileName($e.names.sys))}
function CheckInstalled($e){CheckHash (InstalledSys $e) $plan.files.($e.names.sys)}
$journal=Join-Path $transaction 'journal.json'
$lock=$null
function SaveState($phase){
 $script:j.phase=$phase;$script:j.updated_utc=[DateTime]::UtcNow.ToString('o')
 $text=$script:j|ConvertTo-Json -Depth 20;$bytes=[Text.Encoding]::UTF8.GetBytes($text)
 $tmp=$journal+'.next';$stream=New-Object IO.FileStream($tmp,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::None)
 try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
 if(Test-Path $journal){[IO.File]::Replace($tmp,$journal,$null)}else{[IO.File]::Move($tmp,$journal)}
}
function Done($name){return @($script:j.completed) -ccontains $name}
function Complete($name){$script:j.completed=@($script:j.completed)+$name;SaveState ($name+':done')}
function StagePackage($entry){
 $inf=ChildPath $bundle $entry.names.inf;$published=[MT8167UsbDeployment.Pnp]::Stage($inf)
 CheckHash $published $plan.files.($entry.names.inf);return $published
}
function ResumeNeedsReboot($currentBoot){
 if($script:j.pending_reboot_boot){
  if($script:j.pending_reboot_boot -ceq $currentBoot){return $true}
  $script:j.pending_reboot_boot='';$script:j.reboot_required=$false
 }
 Require (!$script:j.rollback_started -or $Mode -ceq 'Rollback') '復旧開始済み。Rollbackでのみ再開する'
 return $false
}
function RequestReboot($status){
 $script:j.reboot_required=$true;$script:j.pending_reboot_boot=$boot
 Finish $status
}
function Finish($status){SaveState $status;$script:j|ConvertTo-Json -Depth 20}
# WMIのboot時刻は同一boot内で不変。取得不能なら変更前に停止する。
$os=@(Get-WmiObject -Class Win32_OperatingSystem)
Require ($os.Count -eq 1 -and $os[0].LastBootUpTime) 'boot識別情報を取得できない'
$boot=[string]$os[0].LastBootUpTime
try{
 if($Mode -ceq 'Validate' -or $Mode -ceq 'Apply'){
  $parent=FindParent;$legacy=FindLegacy $parent
  CheckNode $parent $plan.baseline.parent;CheckNode $legacy $plan.baseline.legacy
  Require ($parent.Instance -ieq ('ACPI\BEN0001\'+$plan.baseline.parent.instance) -and
   @($legacy.LowerFilters).Count -eq 1 -and $legacy.LowerFilters[0] -ceq $plan.old_filter) 'actual parent/旧filter不一致'
  Require ($null -eq (FindHcd $parent)) '想定外の既存HCD PDOがある'
  CheckInstalled $plan.rollback.parent;CheckInstalled $plan.rollback.legacy
  if($Mode -ceq 'Validate'){@{status='baseline-validated-no-mutation';parent=$parent;legacy=$legacy}|ConvertTo-Json -Depth 10;return}
  Require (!(Test-Path $transaction)) '既存transactionはResume/Rollbackで扱う'
  New-Item -ItemType Directory $transaction|Out-Null
  # 全rollback/candidate leafを固定コピーしてから、最初のstageを行う。
  $frozen=Join-Path $transaction 'bundle';Copy-Item -LiteralPath $bundle -Destination $frozen -Recurse
  $bundle=$frozen
  foreach($f in $plan.files.PSObject.Properties){CheckHash (ChildPath $bundle $f.Name) $f.Value}
  $script:j=[ordered]@{schema=1;plan_sha256=$ExpectedPlanSha256;tool_manifest_sha256=$ExpectedToolManifestSha256;
   phase='prepared';completed=@();parent_before=$parent;legacy_before=$legacy;published_parent='';published_hcd='';observed_hcd=$null;reboot_required=$false;pending_reboot_boot='';rollback_started=$false;hardware_verified=$false;updated_utc=''}
 }else{
  Require (Test-Path $journal) 'transaction記録欠落'
  $script:j=Get-Content $journal -Raw|ConvertFrom-Json
  Require ($script:j.plan_sha256 -ceq $ExpectedPlanSha256 -and $script:j.tool_manifest_sha256 -ceq $ExpectedToolManifestSha256) '別transactionの再開は禁止'
  $bundle=Join-Path $transaction 'bundle'
  CheckHash (Join-Path $bundle 'plan.json') $ExpectedPlanSha256
  foreach($f in $plan.files.PSObject.Properties){CheckHash (ChildPath $bundle $f.Name) $f.Value}
 }
 $lock=New-Object IO.FileStream((Join-Path $transaction 'transaction.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 if(ResumeNeedsReboot $boot){Finish 'await-required-reboot';return}
 SaveState ('mode:'+ $Mode)
 if($Mode -ceq 'Rollback'){
  $script:j.rollback_started=$true;SaveState 'rollback-started'
  $parent=FindParent;$hcd=FindHcd $parent
  if($hcd){CheckNode $hcd $plan.candidate.hcd;SaveState 'rollback-disable-hcd:intent';$need=[MT8167UsbDeployment.Pnp]::Change($hcd,2)
   $hcd=FindHcd $parent;if($need){RequestReboot 'rollback-reboot-required';return}
   Require (!$hcd -or !$hcd.Started) 'HCDが停止していない'}
  SaveState 'rollback-stage:intent';$null=StagePackage $plan.rollback.parent;$null=StagePackage $plan.rollback.legacy
  $legacy=FindLegacy $parent
  if($parent.Version -cne $plan.baseline.parent.version -or !$legacy){
   if($parent.Version -cne $plan.baseline.parent.version){
    SaveState 'rollback-parent:intent';$need=[MT8167UsbDeployment.Pnp]::Bind($parent,(ChildPath $bundle $plan.rollback.parent.names.inf),[MT8167UsbDeployment.Pnp]::Version($plan.rollback.parent.version),$true)
    if($need){RequestReboot 'rollback-reboot-required';return}
   }
   $need=[MT8167UsbDeployment.Pnp]::Change($parent,3)
   if($need){RequestReboot 'rollback-reboot-required';return}
  }
  $parent=FindParent;[MT8167UsbDeployment.Pnp]::Rescan($parent);$legacy=FindLegacy $parent
  Require ($null -ne $legacy) 'rollback旧PDO未列挙。復旧原本を保持し再開する'
  CheckNode $legacy $plan.baseline.legacy;$need=[MT8167UsbDeployment.Pnp]::Change($legacy,1)
  if($need){RequestReboot 'rollback-reboot-required';return}
  $legacy=FindLegacy (FindParent);CheckInstalled $plan.rollback.parent;CheckInstalled $plan.rollback.legacy
  CheckNode (FindParent) $plan.baseline.parent;Require ($legacy.Started) '旧keyboard開始未確認'
  Finish 'rollback-selected-tuple-restored';return
 }
 if(!(Done 'stage-both')){
  SaveState 'stage-both:intent';$script:j.published_hcd=StagePackage $plan.candidate.hcd
  $script:j.published_parent=StagePackage $plan.candidate.parent;Complete 'stage-both'
 }
 $parent=FindParent
 if(!(Done 'legacy-stopped')){
  $legacy=FindLegacy $parent
  if($legacy){CheckNode $legacy $plan.baseline.legacy;SaveState 'legacy-disable:intent'
   $need=[MT8167UsbDeployment.Pnp]::Change($legacy,2);$legacy=FindLegacy $parent
   if($need){RequestReboot 'legacy-stop-reboot-required';return}
   Require (!$legacy -or !$legacy.Started) '旧keyboard provider停止が未確認'}
  Complete 'legacy-stopped'
 }
 # SCMのmshidkmdfは他のHIDと共用。対象PDOだけをPnP停止し、global serviceは触らない。
 if(!(Done 'parent-bound')){
  SaveState 'parent-DIF:intent';$need=[MT8167UsbDeployment.Pnp]::Bind($parent,(ChildPath $bundle $plan.candidate.parent.names.inf),[MT8167UsbDeployment.Pnp]::Version($plan.candidate.parent.version),$true)
  Complete 'parent-bound';if($need){RequestReboot 'parent-reboot-required';return}
 }
 if(!(Done 'parent-restart-requested')){
  SaveState 'parent-restart:intent';$need=[MT8167UsbDeployment.Pnp]::Change((FindParent),3);Complete 'parent-restart-requested'
  if($need){RequestReboot 'parent-reboot-required';return}
 }
 $parent=FindParent;CheckNode $parent $plan.candidate.parent;CheckInstalled $plan.candidate.parent
 SaveState 'parent-reenumerate:intent';[MT8167UsbDeployment.Pnp]::Rescan($parent)
 $legacy=FindLegacy $parent;Require (!$legacy -or !$legacy.Started) '新旧providerが同時に開始している'
 $hcd=FindHcd $parent
 if(!$hcd){Finish ([MT8167UsbDeployment.Pnp]::DeploymentState($legacy,$null));return}
 # 親が作った実PDOだけを観測する。root devnode作成や強制role変更は行わない。
 if($hcd.Service -ine $plan.candidate.hcd.service -or $hcd.Version -cne $plan.candidate.hcd.version){
  $need=[MT8167UsbDeployment.Pnp]::Bind($hcd,(ChildPath $bundle $plan.candidate.hcd.names.inf),[MT8167UsbDeployment.Pnp]::Version($plan.candidate.hcd.version),$false)
  if($need){RequestReboot 'hcd-reboot-required';return}
 }
 $hcd=FindHcd (FindParent);CheckNode $hcd $plan.candidate.hcd;CheckInstalled $plan.candidate.hcd
 Require (@($hcd.LowerFilters).Count -eq 0) '新HCDへ旧keyboard filterを継承している'
 $script:j.observed_hcd=$hcd
 Finish ([MT8167UsbDeployment.Pnp]::DeploymentState((FindLegacy (FindParent)),$hcd))
}catch{
 if($script:j -and $lock){$script:j.phase='failure-rollback-required';try{SaveState $script:j.phase}catch{}}
 throw
}finally{if($lock){$lock.Dispose()}}
