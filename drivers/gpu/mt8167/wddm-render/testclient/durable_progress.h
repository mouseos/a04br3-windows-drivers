/* SPDX-License-Identifier: MIT. Existing committed file slots; never pixel logs.
 * Optional process-local diagnostics. No handle/pointer/GPU address is stored. */
#ifndef GEO_DURABLE_PROGRESS_H
#define GEO_DURABLE_PROGRESS_H
#define GEO_PROGRESS_ENV L"MT8167_D3D_PROGRESS_V1"
#define GEO_PROGRESS_BYTES 4096u
static int geo_progress_write(UINT stage,UINT status,UINT detail) {
 WCHAR path[96];DWORD n,written;HANDLE h;LARGE_INTEGER offset;DWORD row[10];ULONGLONG now;UINT i,hash=2166136261u;
 n=GetEnvironmentVariableW(GEO_PROGRESS_ENV,path,96);if(!n)return 1;
 if(n>=96||stage>=64)return 0;
 /* Set by the dedicated client in this process. Restrict all characters and
  * directory so a malformed environment cannot redirect diagnostics. */
 const WCHAR prefix[]=L"C:\\MT8167D3DClear\\clear-boundary-";
 for(i=0;prefix[i];i++)if(path[i]!=prefix[i])return 0;
 for(;i<n;i++)if(!((path[i]>=L'0'&&path[i]<=L'9')||(path[i]>=L'a'&&path[i]<=L'f')||path[i]==L'-'||path[i]==L'.'||path[i]==L'l'||path[i]==L'o'||path[i]==L'g'))return 0;
 h=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(h==INVALID_HANDLE_VALUE)return 0;
 if(GetFileSize(h,NULL)!=GEO_PROGRESS_BYTES){CloseHandle(h);return 0;}
 now=GetTickCount64();row[0]=0x31525047u;row[1]=1;row[2]=GetCurrentProcessId();row[3]=stage;row[4]=status;row[5]=detail;row[6]=(DWORD)now;row[7]=(DWORD)(now>>32);row[8]=0;
 for(i=0;i<9;i++)hash=(hash^row[i])*16777619u;row[9]=hash;
 offset.QuadPart=(LONGLONG)stage*64;
 int good=SetFilePointerEx(h,offset,NULL,FILE_BEGIN)&&WriteFile(h,row,sizeof(row),&written,NULL)&&written==sizeof(row)&&FlushFileBuffers(h);
 CloseHandle(h);return good;
}
#endif
