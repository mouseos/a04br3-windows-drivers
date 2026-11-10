/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * 固定Type9でglobal診断ringを1回だけsnapshotする。driverprivate/DRTを呼ばない。
 */
#include <Windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include "dxg-global-diag-format.h"
C_ASSERT(sizeof(void *)==4);
C_ASSERT(sizeof(ULONG)==4);
C_ASSERT(D3DKMT_ESCAPE_DIAGNOSTICS==9);
C_ASSERT(D3DKMT_ESCAPE_DRT_TEST==8);
C_ASSERT(sizeof(D3DKMT_ESCAPE)==28);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,hAdapter)==0);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,hDevice)==4);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,Type)==8);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,Flags)==12);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,pPrivateDriverData)==16);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,PrivateDriverDataSize)==20);
C_ASSERT(FIELD_OFFSET(D3DKMT_ESCAPE,hContext)==24);
C_ASSERT(sizeof(D3DKMT_ADAPTERINFO)==20);
C_ASSERT(sizeof(D3DKMT_ENUMADAPTERS)==324);
struct global_packet {ULONG length;unsigned char raw[DXG_DIAG_CAPACITY];};
C_ASSERT(sizeof(struct global_packet)==DXG_DIAG_CAPACITY+4);
C_ASSERT(FIELD_OFFSET(struct global_packet,raw)==4);
static struct global_packet packet;
static WCHAR prefix[MAX_PATH],path[MAX_PATH];
static D3DKMT_ENUMADAPTERS adapters;
static HANDLE summary=INVALID_HANDLE_VALUE,response=INVALID_HANDLE_VALUE,rawfile=INVALID_HANDLE_VALUE;
static int io_ok=1;
/* freestanding、CRTを追加しない。volatileにしてcompilerの自己再呼出しを防ぐ。 */
void * __cdecl memset(void *p,int value,size_t n){volatile unsigned char *d=p;while(n--)*d++=(unsigned char)value;return p;}
void * __cdecl memcpy(void *p,const void *q,size_t n){volatile unsigned char *d=p;const volatile unsigned char *s=q;while(n--)*d++=*s++;return p;}
static int write_all(HANDLE file,const void *data,DWORD bytes){DWORD wrote=0;
 if(!WriteFile(file,data,bytes,&wrote,NULL)||wrote!=bytes||!FlushFileBuffers(file)){io_ok=0;return 0;}return 1;
}
static void line(const char *key,ULONG value){char b[128];UINT i=0,j;static const char hex[]="0123456789abcdef";
 while(*key&&i<110)b[i++]=*key++;b[i++]='=';b[i++]='0';b[i++]='x';
 for(j=0;j<8;j++)b[i++]=hex[(value>>(28-j*4))&15];b[i++]='\r';b[i++]='\n';write_all(summary,b,i);
}
/* Windowsのpathで使えないquoteをescapeする必要はない。単一の絶対prefixだけを受ける。 */
static int argument(void){const WCHAR *p=GetCommandLineW();UINT n=0;int quoted=0;
 if(*p=='"'){++p;while(*p&&*p!='"')++p;if(*p!='"')return 0;++p;}else while(*p&&*p!=' '&&*p!='\t')++p;
 while(*p==' '||*p=='\t')++p;if(*p=='"'){quoted=1;++p;}
 while(*p&&!(quoted?*p=='"':(*p==' '||*p=='\t'))){if(n>=MAX_PATH-24)return 0;prefix[n++]=*p++;}
 if(quoted){if(*p!='"')return 0;++p;}while(*p==' '||*p=='\t')++p;
 if(*p||n<4||prefix[1]!=':'||prefix[2]!='\\'||!((prefix[0]>='A'&&prefix[0]<='Z')||(prefix[0]>='a'&&prefix[0]<='z')))return 0;
 prefix[n]=0;return 1;
}
static HANDLE output(const WCHAR *suffix){UINT n=0,j=0;while(prefix[n]){path[n]=prefix[n];++n;}
 while(suffix[j]&&n<MAX_PATH-1)path[n++]=suffix[j++];path[n]=0;
 if(suffix[j])return INVALID_HANDLE_VALUE;
 return CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
}
static void emit(void *unused,uint32_t offset,const unsigned char *p){(void)unused;
 line("TAG23_OFFSET",offset);line("TAG23_TICK_TIME_LOW",dxg_diag_u32(p+8));line("TAG23_TICK_TIME_HIGH",dxg_diag_u32(p+12));
 line("TAG23_THREAD_LOW",dxg_diag_u32(p+0x20));line("TAG23_THREAD_HIGH",dxg_diag_u32(p+0x24));
 line("TAG23_SEQUENCE",dxg_diag_u32(p+0x28));line("TAG23_WD_EVENT_ORDER",dxg_diag_u32(p+0x2c));
 line("TAG23_PACKED_ID",dxg_diag_u32(p+0x34));line("TAG23_STATUS",dxg_diag_u32(p+0x3c));
}
void mainCRTStartup(void){HMODULE gdi=NULL;PFND3DKMT_ENUMADAPTERS enumerate=NULL;PFND3DKMT_CLOSEADAPTER close_adapter=NULL;PFND3DKMT_ESCAPE escape=NULL;
 NTSTATUS status;D3DKMT_ESCAPE request;D3DKMT_CLOSEADAPTER close_request;D3DKMT_HANDLE chosen=0;
 struct dxg_diag_result parsed={0};DWORD session=0;FILETIME now;UINT i;DWORD result=1;int close_ok=1;
 if(!argument())goto done;
 summary=output(L"-records.txt");if(summary==INVALID_HANDLE_VALUE)goto done;
 response=output(L"-response.bin");if(response==INVALID_HANDLE_VALUE){line("OUTPUT_ERROR",GetLastError());goto done;}
 rawfile=output(L"-raw.bin");if(rawfile==INVALID_HANDLE_VALUE){line("OUTPUT_ERROR",GetLastError());goto done;}
 line("SCHEMA",1);line("API_TYPE",D3DKMT_ESCAPE_DIAGNOSTICS);line("RAW_CAPACITY",DXG_DIAG_CAPACITY);
 line("GPU_SUCCESS_INFERRED",0);line("PID",GetCurrentProcessId());line("UPTIME_MS",GetTickCount());
 if(ProcessIdToSessionId(GetCurrentProcessId(),&session))line("SESSION_ID",session);else line("SESSION_QUERY_ERROR",GetLastError());
 GetSystemTimeAsFileTime(&now);line("FILETIME_LOW",now.dwLowDateTime);line("FILETIME_HIGH",now.dwHighDateTime);
 gdi=LoadLibraryW(L"gdi32.dll");if(!gdi){line("LOAD_GDI_ERROR",GetLastError());goto done;}
 enumerate=(PFND3DKMT_ENUMADAPTERS)GetProcAddress(gdi,"D3DKMTEnumAdapters");
 close_adapter=(PFND3DKMT_CLOSEADAPTER)GetProcAddress(gdi,"D3DKMTCloseAdapter");
 escape=(PFND3DKMT_ESCAPE)GetProcAddress(gdi,"D3DKMTEscape");
 if(!enumerate||!close_adapter||!escape){line("EXPORTS_MISSING",1);goto done;}
 if(!io_ok)goto done;
 status=enumerate(&adapters);line("ENUM_STATUS",(ULONG)status);line("ENUM_COUNT",adapters.NumAdapters);
 if(status!=0){result=2;goto done;}
 if(!adapters.NumAdapters||adapters.NumAdapters>MAX_ENUM_ADAPTERS){result=2;goto close_all;}
 for(i=0;i<adapters.NumAdapters;i++)if(adapters.Adapters[i].hAdapter){chosen=adapters.Adapters[i].hAdapter;line("SELECTED_ENUM_INDEX",i);
  line("SELECTED_LUID_LOW",adapters.Adapters[i].AdapterLuid.LowPart);line("SELECTED_LUID_HIGH",(ULONG)adapters.Adapters[i].AdapterLuid.HighPart);break;}
 if(!chosen){result=2;goto close_all;}
 memset(&request,0,sizeof(request));packet.length=DXG_DIAG_CAPACITY;
 request.hAdapter=chosen;request.Type=D3DKMT_ESCAPE_DIAGNOSTICS;request.pPrivateDriverData=&packet;
 request.PrivateDriverDataSize=sizeof(packet); /* Flags=0、hDevice=0、hContext=0。 */
 line("ESCAPE_CALL_COUNT",1);if(!io_ok)goto close_all;
 status=escape(&request);line("ESCAPE_STATUS",(ULONG)status);line("RETURNED_RAW_BYTES",packet.length);
 /* API failureを含む全responseを、解析より先にそのまま保持する。 */
 if(!write_all(response,&packet,sizeof(packet))){result=1;goto close_all;}
 if(status!=0){result=dxg_diag_snapshot_exit((uint32_t)status,packet.length,NULL);goto close_all;}
 if(packet.length>DXG_DIAG_CAPACITY){line("INVALID_RETURN_LENGTH",1);result=5;goto close_all;}
 if(!write_all(rawfile,packet.raw,packet.length)){result=1;goto close_all;}
 parsed=dxg_diag_parse(packet.raw,packet.length,emit,NULL);
 line("RECORDS",parsed.records);line("TAG23_COUNT",parsed.tag23);line("NEGATIVE_TAG23_COUNT",parsed.negative_tag23);
 if(parsed.negative_tag23){line("FIRST_NEGATIVE_TAG23_STATUS",parsed.first_negative_status);line("FIRST_NEGATIVE_TAG23_OFFSET",parsed.first_negative_offset);}
 line("PARSED_BYTES",parsed.parsed_bytes);line("MALFORMED_OR_PARTIAL",parsed.malformed);line("CAPACITY_HIT",parsed.capacity_hit);
 result=io_ok?dxg_diag_snapshot_exit((uint32_t)status,packet.length,&parsed):1;
close_all:
 for(i=0;i<adapters.NumAdapters&&i<MAX_ENUM_ADAPTERS;i++)if(adapters.Adapters[i].hAdapter){
  close_request.hAdapter=adapters.Adapters[i].hAdapter;status=close_adapter(&close_request);line("CLOSE_ENUM_INDEX",i);line("CLOSE_STATUS",(ULONG)status);
  if(status!=0)close_ok=0;}
 if(!close_ok&&result==0)result=6;
done:
 if(gdi&&!FreeLibrary(gdi))io_ok=0;
 if(response!=INVALID_HANDLE_VALUE&&!CloseHandle(response))io_ok=0;
 if(rawfile!=INVALID_HANDLE_VALUE&&!CloseHandle(rawfile))io_ok=0;
 if(summary!=INVALID_HANDLE_VALUE){line("SNAPSHOT_PARSE_COMPLETE",result==0&&io_ok);line("EXIT_CODE",io_ok?result:1);if(!CloseHandle(summary))io_ok=0;}
 ExitProcess(io_ok?result:1);
}
