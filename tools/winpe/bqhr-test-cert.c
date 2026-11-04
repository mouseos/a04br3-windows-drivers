/* SPDX-License-Identifier: MIT
 * WinPE RAM registry only. Trust the bundled development certificate for the
 * BQHR test package; never operate on an installed Windows registry. */
#include <windows.h>
#include <wincrypt.h>
static void say(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL);}
void mainCRTStartup(void)
{
    WCHAR drive[16]; HKEY pekey=NULL; HANDLE file=INVALID_HANDLE_VALUE;
    HCERTSTORE root=NULL,publisher=NULL; BYTE *data=NULL; DWORD size=0,read=0; int ok=0;
    if(GetEnvironmentVariableW(L"SystemDrive",drive,16)!=2 ||
       (drive[0]!=L'X'&&drive[0]!=L'x') || drive[1]!=L':' ||
       RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinPE",0,KEY_READ,&pekey)!=ERROR_SUCCESS)
        goto done;
    file=CreateFileW(L"X:\\a04br3\\lc709203f\\lc709203f-test.cer",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)goto done;
    size=GetFileSize(file,NULL);if(!size||size>65536u||size==INVALID_FILE_SIZE)goto done;
    data=VirtualAlloc(NULL,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!data||!ReadFile(file,data,size,&read,NULL)||read!=size)goto done;
    root=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_LOCAL_MACHINE,L"ROOT");
    publisher=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_LOCAL_MACHINE,L"TrustedPublisher");
    if(!root||!publisher)goto done;
    if(!CertAddEncodedCertificateToStore(root,X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,data,size,CERT_STORE_ADD_USE_EXISTING,NULL)||
       !CertAddEncodedCertificateToStore(publisher,X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,data,size,CERT_STORE_ADD_USE_EXISTING,NULL))goto done;
    ok=1;
done:
    if(publisher)CertCloseStore(publisher,0);if(root)CertCloseStore(root,0);
    if(data)VirtualFree(data,0,MEM_RELEASE);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(pekey)RegCloseKey(pekey);
    if(ok)say("BQHR_WINPE_TEST_CERT_READY\r\n");else say("BQHR_WINPE_TEST_CERT_FAILED\r\n");
    ExitProcess(ok?0:1);
}
