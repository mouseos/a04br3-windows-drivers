# Session0 jobからの固定rotation起動

run123のwrapperはprivilege enable/restore成功、launch集約rc5だけを残し、4modeの証拠が無い。旧SUPERVISOR_EXIT_RCはlaunch中のCreateProcessAsUser/Assign等の失敗も含むので、本物のchild exit5とは断定できない。

呼出元mt8167-peripheral-update.cはdiagnostic clientを個別のKILL_ON_JOB_CLOSE jobへAssignする。wrapperはSession0、user supervisorは実activeconsole SessionNだが、CreateProcessAsUserは既定でcallerのjobを継承する。既存Session0 jobへの所属とuser sessionは整合しない。単にCreateJobObjectをSession0で呼んだことが不具合なのではない。Windows8以降はjobを最初のmemberのsessionへ関連付けるため、新規のwrapper専用jobへuser childを最初にAssignすること自体は正当である。

## 二者での修正

1. 呼出元はtools/winpe/rotation-job-contract.hを使用する。既存署名/hash照合済みで、directory MT8167DisplayRotationProbeV1、exe mt8167-display-rotation-probe.exe、引数空文字が全一致するclientだけ、外側jobにJOB_OBJECT_LIMIT_BREAKAWAY_OKを追加する。他のdiagnostic jobは変更せず、SILENT_BREAKAWAYは使わない。
2. wrapperのCreateProcessAsUserはCREATE_BREAKAWAY_FROM_JOBを指定する。bInheritHandles FALSEのまま。新user supervisorはsuspendedで作り、既存の専用kill-on-close jobへAssign成功後だけResumeする。workerはそのuser session/jobを継承するので、同じjobをkill/drainしてから復元専用launchする既存契約を保持する。
3. 専用jobへのAssign後なら、callerが終了してwrapperをkillした場合も内側jobの最終handleが閉じ、supervisor/workerを停止する。CreateProcess成功からAssignまでの短い区間にwrapperが異常終了するとsuspended childが残る可能性があり、この区間の完全なprocess-tree停止保証はない。childはAssign成功後にしかResumeしないため、その場合mode変更コードは実行されない。outer wait260秒以上の契約も保持する。さらに上位にbreakaway不可のjobがあればなお拒否し得るため、これを無条件起動保証とはしない。

新PROBE_VERSION2はSUPERVISOR_CREATE/ASSIGN/RESUME_RC、BRIDGE_CONNECT/PID/WRITE_RC、SUPERVISOR_PROCESS_EXIT_RCを追加する。従来の最後のrc/privilege復元や4mode/元mode復元、固定pipe owner/PID、継承log/map限定は変えない。SID/ユーザー/鍵/画面内容をlogしない。run123で5を返した実APIは旧logだけでは一意に断定できない。source上のjob/session不整合を修正した候補であり、実4mode/表示/タッチの成功ではない。

## 必須同時採用

新wrapper3EXEだけを旧callerのbreakaway不可jobで実行しない。**このbranchのcaller差分（includeとjob flagsの2箇所）もMencius/mainの次profileへ統合して、そのnative service TUを再build/signする必要がある。** 現freezeのnative/mediaを変更しない。tools/ci/build-mt8167-display-rotation-probe.ps1は既存SDK closureで固定3roleをbuild/signする。callerは同branchのbuild-peripheral-next.ps1等で、次profileの新client hashesを含む新PackageMapを用いる。古い媒体/古いpolicyを新正本として再生成しない。

一次API契約:

- https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
- https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs
- https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject
- https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-createjobobjectw
- https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags

固定identity/空引数の境界predicateだけhost実行確認する。偽ユーザーtoken、Win32 API shim、架空のmode成功や実機追加bootは用いない。
