/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_ROTATION_JOB_CONTRACT_H
#define MT8167_ROTATION_JOB_CONTRACT_H
/* Hash/trust-pinned, argument-free session bridges need explicit breakaway.
 * Ordinary diagnostics retain their existing job limits. */
static int mt8167_rotation_job_equal(const WCHAR *a,const WCHAR *b){
 if(!a||!b)return 0;while(*a&&*b){WCHAR x=*a++,y=*b++;if(x>=L'A'&&x<=L'Z')x+=L'a'-L'A';if(y>=L'A'&&y<=L'Z')y+=L'a'-L'A';if(x!=y)return 0;}return !*a&&!*b;
}
static DWORD mt8167_rotation_job_flags(const WCHAR *dir,const WCHAR *exe,const WCHAR *arguments){
 DWORD flags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(arguments&&!*arguments&&
    ((mt8167_rotation_job_equal(dir,L"MT8167DisplayRotationProbeV1")&&
      mt8167_rotation_job_equal(exe,L"mt8167-display-rotation-probe.exe"))||
     (mt8167_rotation_job_equal(dir,L".")&&
      mt8167_rotation_job_equal(exe,L"mt8167-dxgi-console-launch363.exe"))))
    flags|=JOB_OBJECT_LIMIT_BREAKAWAY_OK;
 return flags;
}
#endif
