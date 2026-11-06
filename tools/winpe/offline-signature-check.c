/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Observational WinPE trust check; this is not the installed RT CI policy. */
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
static void say(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD v){static const char d[]="0123456789ABCDEF";char b[9];unsigned i;for(i=0;i<8;i++)b[i]=d[(v>>(28-4*i))&15];b[8]=0;say(b);}
static int check(const WCHAR *path,const char *label){WINTRUST_FILE_INFO file;WINTRUST_DATA data;GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG result;BYTE *p;unsigned i;
 p=(BYTE*)&file;for(i=0;i<sizeof(file);i++)p[i]=0;file.cbStruct=sizeof(file);file.pcwszFilePath=path;
 p=(BYTE*)&data;for(i=0;i<sizeof(data);i++)p[i]=0;data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
 result=WinVerifyTrust(INVALID_HANDLE_VALUE,&policy,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(INVALID_HANDLE_VALUE,&policy,&data);
 say(label);say(" WINPE_GENERIC_TRUST_STATUS=");hex((DWORD)result);say("\r\n");return result==ERROR_SUCCESS;}
void mainCRTStartup(void){int top=check(L"X:\\a04br3\\update\\mt8167-toprgu.sys","SIGN_TOPRGU");int touch=check(L"X:\\a04br3\\update\\bu21025.sys","SIGN_TOUCH");int parent=check(L"X:\\a04br3\\update\\mt8167-shared.sys","SIGN_PARENT");
 if(!top||!touch||!parent){say("SIGNATURE_PRECHECK_REJECTED_NO_UPDATE\r\n");ExitProcess(2);}
 say("SIGNATURE_WINPE_GENERIC_OK_RT_CI_POLICY_UNPROVEN\r\n");ExitProcess(0);}
