/* Exact fixed-client policy predicate only; no Win32 API/user simulation. */
#include <assert.h>
#include <wchar.h>
#include <stdint.h>
#include <stdio.h>
typedef wchar_t WCHAR;typedef uint32_t DWORD;
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 0x2000u
#define JOB_OBJECT_LIMIT_BREAKAWAY_OK 0x0800u
#include "../tools/winpe/rotation-job-contract.h"
int main(void){
 const WCHAR *d=L"MT8167DisplayRotationProbeV1",*e=L"mt8167-display-rotation-probe.exe";
 assert(mt8167_rotation_job_flags(d,e,L"")==0x2800);
 assert(mt8167_rotation_job_flags(L"mt8167displayrotationprobev1",L"MT8167-DISPLAY-ROTATION-PROBE.EXE",L"")==0x2800);
 assert(mt8167_rotation_job_flags(d,e,L" ")==0x2000);
 assert(mt8167_rotation_job_flags(d,e,NULL)==0x2000);
 assert(mt8167_rotation_job_flags(d,L"mt8167-display-rotation-session.exe",L"")==0x2000);
 assert(mt8167_rotation_job_flags(L"Other",e,L"")==0x2000);
 assert(mt8167_rotation_job_flags(L"MT8167DisplayRotationProbeV1\\..",e,L"")==0x2000);
 puts("PASS only fixed signed-client identity/no arguments can request explicit breakaway; kill-on-close retained");
}
