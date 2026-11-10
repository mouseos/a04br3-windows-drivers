# Linux PowerShellでのソースcompile/構文/pure判定のみ。PnP APIは呼ばない。
param([Parameter(Mandatory=$true)][string]$Repository)
$ErrorActionPreference='Stop'
$source=Join-Path $Repository 'tools/usb/UsbAbi3Pnp.cs'
Add-Type -TypeDefinition ([IO.File]::ReadAllText($source)) -IgnoreWarnings
if([MT8167UsbDeployment.Pnp]::Version('2026.11.10.1121') -ne [UInt64]::Parse('570268350061085793')){throw 'version変換不一致'}
if([MT8167UsbDeployment.Pnp]::DeploymentState($null,$null) -cne 'await-host-role-or-parent-restart-not-hardware-success'){throw 'PDO欠落を成功扱いした'}
$hcd=New-Object MT8167UsbDeployment.Node
$hcd.Service='MT8167MUSBHcd';$hcd.ClassGuid='{36fc9e60-c465-11cf-8056-444553540000}';$hcd.Version='2026.11.10.1019';$hcd.LowerFilters=[string[]]@()
if([MT8167UsbDeployment.Pnp]::DeploymentState($null,$hcd) -cne 'hcd-selected-start-unverified'){throw '未開始を成功扱いした'}
$hcd.Status=8
if([MT8167UsbDeployment.Pnp]::DeploymentState($null,$hcd) -cne 'HCD-selected-and-started-power-delivery-unverified'){throw '開始結果の分類不一致'}
$hcd.Problem=10
if([MT8167UsbDeployment.Pnp]::DeploymentState($null,$hcd) -cne 'hcd-selected-start-unverified'){throw 'problem codeを無視した'}
$legacy=New-Object MT8167UsbDeployment.Node;$legacy.Status=8;$rejected=$false
try{$null=[MT8167UsbDeployment.Pnp]::DeploymentState($legacy,$hcd)}catch{$rejected=$true}
if(!$rejected){throw '新旧providerの同時稼働を許容した'}
$hcd.Problem=0;$hcd.LowerFilters=[string[]]@('A04Usb20Keyboard');$rejected=$false
try{$null=[MT8167UsbDeployment.Pnp]::DeploymentState($null,$hcd)}catch{$rejected=$true}
if(!$rejected){throw '旧filter継承を許容した'}
foreach($relative in @('tools/usb/install-usb1121-first.ps1','tools/ci/build-usb1121-pnp-helper.ps1')){
 $tokens=$null;$errors=$null;$null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $Repository $relative),[ref]$tokens,[ref]$errors)
 if($errors.Count){$errors|Format-List;throw 'PowerShell構文エラー'}
 Write-Output ('PARSE_OK '+$relative)
}
# 実recipeから再開gateだけを取り出し、PnP/WMIを呼ばずjournal遷移を検証する。
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $Repository 'tools/usb/install-usb1121-first.ps1'),[ref]$tokens,[ref]$errors)
foreach($name in @('Require','ResumeNeedsReboot')){
 $f=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true)
 if(!$f){throw '再開gate関数欠落'}
 . ([scriptblock]::Create($f.Extent.Text))
}
$Mode='Resume'
$script:j=[pscustomobject]@{pending_reboot_boot='boot-A';reboot_required=$true;rollback_started=$false}
if(!(ResumeNeedsReboot 'boot-A') -or !$script:j.reboot_required){throw '同一bootで再開を許可した'}
if((ResumeNeedsReboot 'boot-B') -or $script:j.reboot_required -or $script:j.pending_reboot_boot){throw '実再起動後のgate解除不一致'}
$script:j.rollback_started=$true;$rejected=$false
try{$null=ResumeNeedsReboot 'boot-B'}catch{$rejected=$true}
if(!$rejected){throw '復旧開始後の候補再開を許可した'}
$Mode='Rollback'
if(ResumeNeedsReboot 'boot-B'){throw '復旧の再開を拒否した'}
Write-Output 'CS_HOST_COMPILE_AND_PURE_ADMISSION_PASS_NO_PNP_EXECUTION'
