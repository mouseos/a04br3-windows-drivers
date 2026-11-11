# Pure fixed-record validator tests; no hive/native/device API is invoked.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/recovery/read-mt6630-checkpoint-loaded.ps1')
function Put32([byte[]]$b,[int]$offset,[uint32]$n){[Array]::Copy([BitConverter]::GetBytes($n),0,$b,$offset,4)}
function Fixture([uint32]$build=1031,[uint32]$channel=3,[uint32]$phase=2,[uint32]$kind=3,[uint32]$observed=1,[uint32]$detail=4,[uint32]$stage=128){
 $b=New-Object byte[] 1624
 Put32 $b 0 0x50433441;Put32 $b 4 1;Put32 $b 8 1624;Put32 $b 12 $channel
 [Array]::Copy([BitConverter]::GetBytes([uint64]7),0,$b,16,8)
 Put32 $b 48 1;Put32 $b 84 $build
 Put32 $b 88 $stage;Put32 $b 92 $phase;Put32 $b 96 $kind;Put32 $b 100 $observed;Put32 $b 104 0xffffffffL;Put32 $b 108 $detail
 return ,$b
}
$passed=0
foreach($detail in @(1,2,3,4,0x0304,0x1f04)){
 $b=Fixture -detail $detail
 if((Test-Mt6630CheckpointRecord $b 3) -ne 7){throw 'Valid fixed1031 RESET rejected'}
 $passed++
}
$negative=@(@{build=1029},@{build=1030},@{build=1032},@{channel=0},@{channel=1},@{channel=2},@{phase=1},@{phase=3},@{kind=0},@{kind=1},@{kind=2},@{kind=4},@{observed=0},@{detail=0},@{detail=5},@{detail=0x2004},@{detail=0x0404},@{stage=129})
foreach($v in $negative){
 $b=Fixture @v;$ch=[BitConverter]::ToUInt32($b,12);$rejected=$false
 try{$null=Test-Mt6630CheckpointRecord $b $ch}catch{$rejected=$true}
 if(!$rejected){throw ('Invalid RESET accepted: '+($v|ConvertTo-Json -Compress))}
 $passed++
}
# Legacy unrelated stage remains accepted with its original schema/build.
$b=Fixture -build 1023 -stage 88 -kind 4
if((Test-Mt6630CheckpointRecord $b 3) -ne 7){throw 'Legacy event regressed'}
$passed++
Write-Output ('FIXED_CHECKPOINT_VALIDATOR_PASS cases='+$passed+' hive_loads=0 device_operations=0')
