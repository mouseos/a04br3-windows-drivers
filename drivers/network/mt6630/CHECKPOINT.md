# Native1004の固定registry checkpoint

実機run33は配置/selected1003一致、service CODE21まで確認済みだが、exact CM/NDIS原因は
未回収。旧1001/14:44のSetupAPI Code31を今回の根拠に使わない。このcheckpointは次の
1004初期化を観測するためのもので、原因を推測してdriver動作を変更するものではない。
Ramanのservice LastDiagnosticとは別に、miniport自身の実呼出し結果を保持する。

## 固定保存先とscope

`HKLM\SYSTEM\CurrentControlSet\Services\Mt6630Runtime\Parameters\NativeCheckpoint`

- `Generation`: REG_QWORD。DriverEntry/Initialize開始ごとに既存値を読んで1増加。
- `DriverEntry`: REG_BINARY、固定1624byte/schema1、最大64イベント。
- `Initialize`: REG_BINARY、同じ固定schema。最新attemptだけを保持。

親serviceは既存keyをopenし、作成は自身のParametersとNativeCheckpointだけ。
任意path・名前・入力command・broker・ACL変更・保護key迂回は追加しない。
MAC/SSID/BSSID/校正/鍵/ポインタ/個体識別値は保存しない。生成counterの型/長さが
不正、overflow、registry操作拒否の場合は世代を捏造して成功記録を作らない。

DriverEntryとMiniportInitializeExの実PASSIVE経路だけがrecordを所有する。
Initializeのstack上制御と独立したPagedPool recordを用い、adapterには初期化中だけ
一時pointerを置く。成功/失敗ともcleanup前にpointerを外し、診断handle/recordを閉じる。
通常OID、timer/DPC、Send/Cancel、IRQ handler、shutdownからregistryは呼ばない。
全writer入口もPASSIVE_LEVELを検査する。logger用KMUTEXはIRQLを上げず、NDIS spinlockを
保持したregistry I/Oはない。親・I/O APIを呼ぶ間はlogger mutexを保持しない。
診断失敗は既存NDIS戻り値/既存cleanupの判断を変更しない。

## 何を記録するか

ABI/数値stage一覧の正本は `checkpoint_wire.h`。magic=A4CP、version=1、driver_build=1004。
生成counter、親DriverEntry世代、KeQueryInterruptTime/KeQuerySystemTimeも記録する。
RTCの2014年表示を新しいbootの証拠に代用しない。再実行前にnative replayしたSYSTEMの
Generationをbaselineとして保存し、回収値がそれを超えたか照合する。

各eventはStage、Phase、Kind、Observed、Raw、Detailの6 DWORD。

|Kind|意味|
|---|---|
|0 UNKNOWN|未観測。Rawが0でも成功ではない|
|1 NDIS_STATUS|実NDIS APIの戻り値、または既存Initializeが返す準備をした値|
|2 NTSTATUS|実NTSTATUS型API/helperの戻り値。NDIS/backend値をNTSTATUSに偽装しない|
|3 BACKEND_INT|既存SDIO/session/station等の実int戻り値|
|4 OBSERVATION|allocation pointer有無、resource検証、void cleanupが戻ったこと等。NTSTATUSではない|

BEGINは必ずObserved=0。対応RESULTがなければそのstageの結果は不明。
RXの64 slotはBEGIN一回と合計完了/失敗一回だけ記録する。失敗Detailは
`(slot_index << 8) | 1(MDL)/2(NBL)`。resource検証失敗Detailは
1=list/件数不整合、2=memory複数、3=memory無し。radio settingのDetailは実NIC power MIB。
CLEANUP/OBSERVATION1は関数が戻った証拠で、quarantined leaseの解放成功を主張しない。

記録点: Driver registration、Initialize entry/context、registration attrs、RX pool/slots、
NDIS configuration open、radio設定、親query、resources、session Acquire/map/IRQ/activate/
diagnostic、TC1、station、PS profile、general attrs、Native attrs allocation/登録、cleanup、
return-prepared。NdisMSetMiniportAttributesの実statusをregistration/general/Nativeで区別。
session旧entryはNULL observationで元の経路を使い、戻り値/lease/cleanup契約は維持する。

RETURN_PREPAREDは実戻り値を渡す直前の記録で、NDISがそれを受理したこと、WLAN登録・
interface列挙・scan・接続が成功したことを証明しない。post-Initialize OID失敗は今回の
記録scope外。checkpointに原因stageがなければ原statusを補完しない。

## boundedとflushの解釈

64event、最大10回ZwFlushKey/attempt、固定3値だけで履歴keyを増殖させない。
entry世代を一度flushし、register/parent-query/session/general/Native attrs/cleanupという
固定境界だけBEGINをflushする。細かい全eventでhive flushを繰り返さない。
最後はresult recordをflushし、その実NTSTATUSを保存して最後のZwFlushKeyを行う。
失敗時のfault保存/flushは一度だけ。stage数上限到達後はoverflow/errorを保持する。
Zw registry I/Oには取消/timeout APIがないため、これは回数と保存量の上限であり、
filesystemやhive I/O自体のwall-clock timeoutを保証するものではない。

実flush失敗とwrite失敗は最初のdiagnostic_errorとしてstickyにし、finishもその失敗を返す。
後の成功で上書きして全体成功にしない。未到達stageは0-successにしない。

最後のflush結果をregistryへ記録すると、さらにその記録をflushする操作が必要になる。
従って最終末尾flushの成功を事前に立てるflagは設けず、`tail_flush_known=0`を維持する。
`flush_observed/flush_status`は保存されたsnapshotより前に完了した実flushの結果であって、
末尾のflush完了証明ではない。最後のfault書込みまで保存できなかった場合も、回収recordを
durable-successと判定しない。これはcomponentの実return値まで未観測にするという意味では
ないが、raw status0から初期化全体/永続化全体/NDIS handoffの成功を推定してはならない。

## 主担当のnative replay後の回収例

既存native RegLoadKey/replayが完了した作業copyのmountを使用する。元のprivate SYSTEMや
保護ACLは変更しない。以下は読取り/固定recordのexportだけで、新しいload/replayではない。
`$mountedSystem`と`$privateOutput`は既存recipe内のmount名/作業ルート内新規出力先を使う。

```powershell
$select=[Microsoft.Win32.Registry]::LocalMachine.OpenSubKey("$mountedSystem\Select")
$current=[int]$select.GetValue('Current');$select.Close()
$key=[Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
  ('{0}\ControlSet{1:D3}\Services\Mt6630Runtime\Parameters\NativeCheckpoint' -f $mountedSystem,$current))
if(!$key){throw 'Checkpoint unobserved'}
try {
  $generation=$key.GetValue('Generation',$null)
  if($null -eq $generation){throw 'Generation unobserved'}
  foreach($name in @('DriverEntry','Initialize')){
    $bytes=$key.GetValue($name,$null)
    if($bytes -isnot [byte[]]){throw "$name unobserved"}
    [IO.File]::WriteAllBytes((Join-Path $privateOutput "$name.bin"),$bytes)
  }
} finally {$key.Close()}
```

```text
python tools/winpe/decode-mt6630-checkpoint.py <root内private出力>/Initialize.bin --after-generation <事前counter>
```

decoderはhive/端末/registryを操作しない。stage/type/raw値、未観測一覧、世代freshness、
flush診断をJSONへ表示し、durable_successとndis_handoff_observedをtrueにしない。
CLIの正常終了はschema decode成立だけを意味する。baseline不明はfreshness=unknown、
古いrecordはstale。Initialize recordのparent_generationとDriverEntry recordも照合する。
regf/transaction logのreplay自体は既存の主担当recipeで行う。

## host検証とbuild

単一writer fixtureで実Cの固定保存先/型、世代、原status、BEGIN未観測、高IRQLでregistry
操作0、flush失敗のsticky保持、capacity/flush上限、並行旧世代の上書き拒否を確認する。
decoderも原status/未観測/flush失敗/stale baselineを限定確認。WDK8.1 link/署名と実SYSTEMへの
永続化は未実施。必要なZw registry API/KeMutexは保存済みRT8.1 kernel export metadataに存在。

Native build helperはcheckpoint.cを含め、INF1004/既存scalar flags/kernel runtimeを維持する。
PSP/RF休止・scan復帰のsource実装はb470f3cから保持し、実測未確認事項はNATIVE-1004.md参照。
未実装cap広告、未知FW opcode、rootcause不明の推測修正は追加しない。

## run42を受けた1006観測候補

1004記録/selected INF1005の確定失敗範囲、追加RESULT stage45..55と新SYS1006の
build手順は[RUN42-DIAGNOSTIC-1006.md](RUN42-DIAGNOSTIC-1006.md)を参照。
旧recordのbuild1004を1006として読まない。schema/容量/末尾flushの解釈は維持する。
