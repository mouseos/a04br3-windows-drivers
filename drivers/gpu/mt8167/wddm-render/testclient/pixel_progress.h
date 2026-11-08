/* SPDX-License-Identifier: MIT. One durable row per completed pixel oracle.
 * GPR2 uses dedicated slots32..43 inside the precommitted GPR1 4096B file.
 * No handles, addresses or pixel payload. Old GPR1 slot format is unchanged. */
#ifndef GEO_PIXEL_PROGRESS_H
#define GEO_PIXEL_PROGRESS_H
static int geo_pixel_result(UINT kind,UINT pass,UINT match,UINT mismatch,UINT a,UINT b,UINT c,UINT hash,UINT passed) {
 WCHAR path[96];const WCHAR prefix[]=L"C:\\MT8167D3DClear\\clear-boundary-";
 DWORD n,written,row[16];UINT i,slot,crc=2166136261u;HANDLE h;LARGE_INTEGER offset;ULONGLONG now;
 if(kind==1&&pass<2)slot=32+pass;
 else if(kind==2&&!pass)slot=34;
 else if(kind==3&&!pass)slot=35;
 else if(kind==4&&pass<8)slot=36+pass;
 else return 0;
 n=GetEnvironmentVariableW(L"MT8167_D3D_PROGRESS_V1",path,96);if(!n||n>=96)return 0;
 for(i=0;prefix[i];i++)if(path[i]!=prefix[i])return 0;
 for(;i<n;i++)if(!((path[i]>=L'0'&&path[i]<=L'9')||(path[i]>=L'a'&&path[i]<=L'f')||path[i]==L'-'||path[i]==L'.'||path[i]==L'l'||path[i]==L'o'||path[i]==L'g'))return 0;
 h=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 if(GetFileSize(h,NULL)!=4096u){CloseHandle(h);return 0;}
 now=GetTickCount64();row[0]=0x32525047u;row[1]=2;row[2]=GetCurrentProcessId();row[3]=slot;
 row[4]=passed!=0;row[5]=kind;row[6]=pass;row[7]=(DWORD)now;row[8]=(DWORD)(now>>32);
 row[9]=match;row[10]=mismatch;row[11]=a;row[12]=b;row[13]=c;row[14]=hash;
 for(i=0;i<15;i++)crc=(crc^row[i])*16777619u;row[15]=crc;
 offset.QuadPart=(LONGLONG)slot*64;
 int good=SetFilePointerEx(h,offset,NULL,FILE_BEGIN)&&WriteFile(h,row,sizeof(row),&written,NULL)&&written==sizeof(row)&&FlushFileBuffers(h);
 CloseHandle(h);return good;
}
#endif
