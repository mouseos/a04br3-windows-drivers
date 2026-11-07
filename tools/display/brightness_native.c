/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed local Monitor.sys WMI experiment. No arguments, shell, DDC or MMIO.
 * A fresh process calls A04RunBrightnessNativeProbe once. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <wbemidl.h>
#include "brightness_sequence.h"
#include "brightness_native.h"
#include "brightness_wmi_result.h"
#include "brightness_log.h"
#define WAIT_MS 3000
/* Freestanding ARM32 entry: do not depend on an MSVC CRT installation. */
void *memset(void *dst,int value,size_t length)
{volatile unsigned char *p=dst;while(length--)*p++=(unsigned char)value;return dst;}
void *memcpy(void *dst,const void *src,size_t length)
{volatile unsigned char *p=dst;const volatile unsigned char *q=src;while(length--)*p++=*q++;return dst;}
#define LOG_PATH L"C:\\Windows\\System32\\MT8167EngineProbe\\logs\\brightness-native.log"
struct probe {
 HANDLE file,stdout_handle;struct a04_brightness_log log;DWORD root_error,stdout_error;IWbemServices *services;IWbemClassObject *input;
 BSTR name,monitor_path,method_path,method_name;int log_failed;
};
static HRESULT exact(HRESULT hr) {return hr==S_OK?S_OK:(FAILED(hr)?hr:HRESULT_FROM_WIN32(ERROR_TIMEOUT));}
static HRESULT secure(IUnknown *p)
{return CoSetProxyBlanket(p,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,NULL,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE);}
static int root_bytes(void *context,const char *s,unsigned n)
{
 struct probe *p=context;DWORD written=0;
 if(!WriteFile(p->file,s,n,&written,NULL)){p->root_error=GetLastError();return -1;}
 if(written!=n){p->root_error=ERROR_WRITE_FAULT;return -1;}
 if(!FlushFileBuffers(p->file)){p->root_error=GetLastError();return -1;}return 0;
}
static int stdout_bytes(void *context,const char *s,unsigned n)
{
 struct probe *p=context;DWORD written=0;
 if(!p->stdout_handle || p->stdout_handle==INVALID_HANDLE_VALUE){p->stdout_error=ERROR_INVALID_HANDLE;return -1;}
 if(!WriteFile(p->stdout_handle,s,n,&written,NULL)){p->stdout_error=GetLastError();return -1;}
 if(written!=n){p->stdout_error=ERROR_WRITE_FAULT;return -1;}return 0;
}
static int bytes(struct probe *p,const char *s,DWORD n)
{
 int result=a04_brightness_log_write(&p->log,s,n);
 if(result)p->log_failed=1;return result;
}
static char *text(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *hex(char *p,unsigned v){unsigned i;for(i=0;i<8;i++)*p++="0123456789abcdef"[(v>>(28-4*i))&15];return p;}
static int record(void *context,const char *phase,unsigned requested,unsigned observed,int status)
{
 struct probe *p=context;char line[192],*s=line;
 s=text(s,"tick=");s=hex(s,GetTickCount());s=text(s," phase=");s=text(s,phase);
 s=text(s," requested_hex=");s=hex(s,requested);s=text(s," observed_hex=");s=hex(s,observed);
 s=text(s," status_hex=");s=hex(s,(unsigned)status);s=text(s,"\r\n");
 return bytes(p,line,(DWORD)(s-line));
}
static HRESULT number(IWbemClassObject *obj,const WCHAR *key,CIMTYPE type,unsigned *out)
{
 VARIANT v;CIMTYPE actual=0;HRESULT hr;VariantInit(&v);
 hr=IWbemClassObject_Get(obj,key,0,&v,&actual,NULL);
 if(SUCCEEDED(hr)) {
  if(actual!=type || (type==CIM_UINT8?V_VT(&v)!=VT_UI1:V_VT(&v)!=VT_I4))hr=WBEM_E_TYPE_MISMATCH;
  else *out=type==CIM_UINT8?V_UI1(&v):(unsigned)V_I4(&v);
 }
 VariantClear(&v);return hr;
}
static HRESULT signature_type(IWbemClassObject *obj,const WCHAR *key,CIMTYPE expected)
{
 VARIANT v;CIMTYPE actual=0;HRESULT hr;
 if(!obj)return WBEM_E_INVALID_OBJECT;
 VariantInit(&v);hr=IWbemClassObject_Get(obj,key,0,&v,&actual,NULL);
 if(hr==S_OK && actual!=expected)hr=WBEM_E_TYPE_MISMATCH;
 VariantClear(&v);return hr;
}
static HRESULT string_value(IWbemClassObject *obj,const WCHAR *key,BSTR *out)
{
 VARIANT v;HRESULT hr;VariantInit(&v);hr=IWbemClassObject_Get(obj,key,0,&v,NULL,NULL);
 if(SUCCEEDED(hr)) {
  if(V_VT(&v)!=VT_BSTR || !V_BSTR(&v) || !SysStringLen(V_BSTR(&v)) || SysStringLen(V_BSTR(&v))>1024)hr=WBEM_E_TYPE_MISMATCH;
  else {*out=SysAllocStringLen(V_BSTR(&v),SysStringLen(V_BSTR(&v)));if(!*out)hr=E_OUTOFMEMORY;}
 }
 VariantClear(&v);return hr;
}
static int same(BSTR a,BSTR b)
{return a && b && CompareStringOrdinal(a,(int)SysStringLen(a),b,(int)SysStringLen(b),TRUE)==CSTR_EQUAL;}
static HRESULT active(IWbemClassObject *obj)
{
 VARIANT v;HRESULT hr;VariantInit(&v);hr=IWbemClassObject_Get(obj,L"Active",0,&v,NULL,NULL);
 if(SUCCEEDED(hr) && (V_VT(&v)!=VT_BOOL || V_BOOL(&v)!=VARIANT_TRUE))hr=WBEM_E_INVALID_OBJECT;
 VariantClear(&v);return hr;
}
static HRESULT query(struct probe *p,const WCHAR *fixed,IEnumWbemClassObject **out)
{
 BSTR lang=SysAllocString(L"WQL"),q=SysAllocString(fixed);HRESULT hr=E_OUTOFMEMORY;
 if(lang && q)hr=IWbemServices_ExecQuery(p->services,lang,q,WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY,NULL,out);
 if(SUCCEEDED(hr) && !*out)hr=E_UNEXPECTED;
 if(SUCCEEDED(hr))hr=secure((IUnknown *)*out);
 SysFreeString(lang);SysFreeString(q);return hr;
}
static HRESULT object(struct probe *p,BSTR path,IWbemClassObject **out)
{
 IWbemCallResult *call=NULL;HRESULT hr;
 hr=IWbemServices_GetObject(p->services,path,WBEM_FLAG_RETURN_IMMEDIATELY,NULL,NULL,&call);
 if(hr==S_OK && !call)hr=E_UNEXPECTED;
 if(hr==S_OK)hr=secure((IUnknown *)call);
 if(hr==S_OK)hr=exact(IWbemCallResult_GetResultObject(call,WAIT_MS,out));
 if(hr==S_OK && !*out)hr=E_UNEXPECTED;
 if(call)IWbemCallResult_Release(call);return hr;
}
static HRESULT levels(IWbemClassObject *obj,unsigned original)
{
 VARIANT v;CIMTYPE type=0;LONG low,high,i;unsigned count=0;int found=0;HRESULT hr;
 VariantInit(&v);hr=number(obj,L"Levels",CIM_UINT32,&count);
 if(SUCCEEDED(hr))hr=IWbemClassObject_Get(obj,L"Level",0,&v,&type,NULL);
 if(SUCCEEDED(hr) && (type!=(CIM_UINT8|CIM_FLAG_ARRAY) || V_VT(&v)!=(VT_ARRAY|VT_UI1) || !V_ARRAY(&v) || SafeArrayGetDim(V_ARRAY(&v))!=1))hr=WBEM_E_TYPE_MISMATCH;
 if(SUCCEEDED(hr))hr=SafeArrayGetLBound(V_ARRAY(&v),1,&low);
 if(SUCCEEDED(hr))hr=SafeArrayGetUBound(V_ARRAY(&v),1,&high);
 if(SUCCEEDED(hr) && (count<1 || count>101 || high<low || (ULONGLONG)((LONGLONG)high-low+1)!=count))hr=WBEM_E_INVALID_OBJECT;
 if(SUCCEEDED(hr))for(i=0;i<(LONG)count;i++) {
  BYTE value=0;LONG at=low+i;hr=SafeArrayGetElement(V_ARRAY(&v),&at,&value);if(FAILED(hr))break;
  if(value==original)found|=1;if(value==30)found|=2;if(value==70)found|=4;
 }
 if(SUCCEEDED(hr) && found!=7)hr=WBEM_E_NOT_SUPPORTED;
 VariantClear(&v);return hr;
}
static HRESULT identity_log(struct probe *p)
{
 char utf8[1024];int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,p->name,(int)SysStringLen(p->name),utf8,sizeof(utf8),NULL,NULL);
 if(!n)return HRESULT_FROM_WIN32(GetLastError());
 if(bytes(p,"instance_utf8=",14) || bytes(p,utf8,(DWORD)n) || bytes(p,"\r\n",2))return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
 return S_OK;
}
static int snapshot(void *context,unsigned *original)
{
 struct probe *p=context;IEnumWbemClassObject *en=NULL;IWbemClassObject *obj=NULL,*other=NULL,*klass=NULL,*definition=NULL,*output_definition=NULL;
 ULONG got=0;BSTR name=NULL,klass_name=NULL;HRESULT hr;unsigned i,matches=0;
 hr=query(p,L"SELECT * FROM WmiMonitorBrightness WHERE Active=TRUE",&en);if(FAILED(hr))goto done;
 hr=IEnumWbemClassObject_Next(en,WAIT_MS,1,&obj,&got);
 if(hr==WBEM_S_FALSE && !got)hr=WBEM_E_NOT_FOUND;else hr=exact(hr);
 if(FAILED(hr) || got!=1){if(SUCCEEDED(hr))hr=WBEM_E_NOT_FOUND;goto done;}
 /* A second active panel is ambiguity, even if it has the same name. */
 hr=IEnumWbemClassObject_Next(en,WAIT_MS,1,&other,&got);
 if(hr!=WBEM_S_FALSE || got!=0){hr=FAILED(hr)?hr:WBEM_E_INVALID_OBJECT;goto done;}
 hr=active(obj);if(SUCCEEDED(hr))hr=number(obj,L"CurrentBrightness",CIM_UINT8,original);
 if(SUCCEEDED(hr) && (*original<1 || *original>100))hr=WBEM_E_INVALID_OBJECT;
 if(SUCCEEDED(hr))hr=levels(obj,*original);
 if(SUCCEEDED(hr))hr=string_value(obj,L"InstanceName",&p->name);
 if(SUCCEEDED(hr))hr=string_value(obj,L"__RELPATH",&p->monitor_path);
 if(FAILED(hr))goto done;
 IWbemClassObject_Release(obj);obj=NULL;IEnumWbemClassObject_Release(en);en=NULL;
 hr=query(p,L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE",&en);if(FAILED(hr))goto done;
 for(i=0;i<16;i++) {
  got=0;hr=IEnumWbemClassObject_Next(en,WAIT_MS,1,&obj,&got);
  if(hr==WBEM_S_FALSE && !got){hr=matches==1?S_OK:WBEM_E_NOT_FOUND;break;}
  hr=exact(hr);if(FAILED(hr) || got!=1){if(SUCCEEDED(hr))hr=E_UNEXPECTED;break;}
  hr=active(obj);if(SUCCEEDED(hr))hr=string_value(obj,L"InstanceName",&name);
  if(SUCCEEDED(hr) && same(name,p->name)) {
   if(++matches!=1)hr=WBEM_E_INVALID_OBJECT;
   else hr=string_value(obj,L"__RELPATH",&p->method_path);
  }
  SysFreeString(name);name=NULL;IWbemClassObject_Release(obj);obj=NULL;
  if(FAILED(hr))break;
 }
 if(i==16)hr=WBEM_E_INVALID_OBJECT;
 if(FAILED(hr))goto done;
 /* Obtain the official input signature; no hand-built WMI parameter class. */
 klass_name=SysAllocString(L"WmiMonitorBrightnessMethods");p->method_name=SysAllocString(L"WmiSetBrightness");
 if(!klass_name || !p->method_name){hr=E_OUTOFMEMORY;goto done;}
 hr=object(p,klass_name,&klass);
 if(SUCCEEDED(hr))hr=IWbemClassObject_GetMethod(klass,p->method_name,0,&definition,&output_definition);
 if(SUCCEEDED(hr))hr=signature_type(definition,L"Timeout",CIM_UINT32);
 if(SUCCEEDED(hr))hr=signature_type(definition,L"Brightness",CIM_UINT8);
 if(SUCCEEDED(hr))hr=signature_type(output_definition,L"ReturnValue",CIM_UINT32);
 (void)record(p,"wmi-method-signature",CIM_UINT32,CIM_UINT8,(int)hr);
 if(SUCCEEDED(hr))hr=IWbemClassObject_SpawnInstance(definition,0,&p->input);
 if(SUCCEEDED(hr))hr=identity_log(p);
done:
 if(output_definition)IWbemClassObject_Release(output_definition);
 if(definition)IWbemClassObject_Release(definition);if(klass)IWbemClassObject_Release(klass);
 if(obj)IWbemClassObject_Release(obj);if(other)IWbemClassObject_Release(other);if(en)IEnumWbemClassObject_Release(en);
 SysFreeString(name);SysFreeString(klass_name);return (int)hr;
}
static int method_exec(void *context,void **direct,void **call)
{
 struct probe *p=context;
 return (int)IWbemServices_ExecMethod(p->services,p->method_path,p->method_name,
   WBEM_FLAG_RETURN_IMMEDIATELY,NULL,p->input,(IWbemClassObject **)direct,(IWbemCallResult **)call);
}
static int method_secure(void *context,void *call)
{(void)context;return (int)secure((IUnknown *)call);}
static int method_completion(void *context,void *call,int *operation)
{
 LONG status=(LONG)WBEM_E_FAILED;HRESULT hr;
 (void)context;
 hr=IWbemCallResult_GetCallStatus((IWbemCallResult *)call,WAIT_MS,&status);
 /* On transport timeout/error, status is unspecified and must not authorize
  * output decode or a retry. Preserve each result independently in stdout. */
 if(hr==S_OK)*operation=(int)status;
 return (int)exact(hr);
}
static int method_result(void *context,void *call,void **out)
{(void)context;return (int)exact(IWbemCallResult_GetResultObject((IWbemCallResult *)call,WAIT_MS,(IWbemClassObject **)out));}
static int method_value(void *context,void *reply,unsigned *value)
{(void)context;return (int)number((IWbemClassObject *)reply,L"ReturnValue",CIM_UINT32,value);}
static void method_release_object(void *context,void *reply)
{(void)context;IWbemClassObject_Release((IWbemClassObject *)reply);}
static void method_release_call(void *context,void *call)
{(void)context;IWbemCallResult_Release((IWbemCallResult *)call);}
static void method_trace(void *context,const char *phase,unsigned value,int hr)
{(void)record(context,phase,0,value,hr);}
static int set(void *context,unsigned level)
{
 struct probe *p=context;VARIANT v;unsigned result=~0u;HRESULT hr;
 struct a04_wmi_result_ops ops;
 VariantInit(&v);V_VT(&v)=VT_I4;V_I4(&v)=0;
 hr=IWbemClassObject_Put(p->input,L"Timeout",0,&v,0);
 V_VT(&v)=VT_UI1;V_UI1(&v)=(BYTE)level;
 if(SUCCEEDED(hr))hr=IWbemClassObject_Put(p->input,L"Brightness",0,&v,0);
 if(record(p,"wmi-arguments",level,0,(int)hr))p->log_failed=1;
 ops.context=p;ops.exec=method_exec;ops.secure=method_secure;ops.completion=method_completion;ops.result=method_result;
 ops.value=method_value;ops.release_object=method_release_object;ops.release_call=method_release_call;ops.trace=method_trace;
 if(hr==S_OK)hr=(HRESULT)a04_brightness_method_result(&ops,&result);
 if(record(p,"wmi-method-output",level,result,(int)hr))p->log_failed=1;
 if(hr==S_OK && result)hr=WBEM_E_FAILED;
 return (int)hr;
}
static int read_level(void *context,unsigned *value)
{
 struct probe *p=context;IWbemClassObject *obj=NULL;BSTR name=NULL;HRESULT hr=object(p,p->monitor_path,&obj);
 if(SUCCEEDED(hr))hr=active(obj);
 if(SUCCEEDED(hr))hr=string_value(obj,L"InstanceName",&name);
 if(SUCCEEDED(hr) && !same(name,p->name))hr=WBEM_E_INVALID_OBJECT;
 if(SUCCEEDED(hr))hr=number(obj,L"CurrentBrightness",CIM_UINT8,value);
 SysFreeString(name);if(obj)IWbemClassObject_Release(obj);return (int)hr;
}
static void pause_read(void *context){(void)context;Sleep(250);}
DWORD A04RunBrightnessNativeProbe(void)
{
 struct probe p={0};struct a04_brightness_ops ops;IWbemLocator *locator=NULL;BSTR ns=NULL;HRESULT hr;unsigned result=8;BOOL initialized=FALSE;const char *com_phase="co-initialize";
 /* Engine owns provisioning/archive of the entire state generation. Never
  * create directories, erase an old log, or consult the legacy root guard. */
 p.stdout_handle=GetStdHandle(STD_OUTPUT_HANDLE);
 p.log.context=&p;p.log.root=root_bytes;p.log.stdout_write=stdout_bytes;
 /* Existing log means this one-shot already ran; never truncate its evidence. */
 p.file=CreateFileW(LOG_PATH,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
 if(p.file==INVALID_HANDLE_VALUE){
  DWORD error=GetLastError();char line[160],*end=line;
  end=text(end,"phase=log-create-failed status_hex=");end=hex(end,(unsigned)HRESULT_FROM_WIN32(error));end=text(end,"\r\n");
  (void)stdout_bytes(&p,line,(unsigned)(end-line));return 16;
 }
 {static const char header[]="schema=1 api=local-Monitor.sys-WMI raw_pwm_observed=0 restore_scope=standard-percent\r\n";
  if(bytes(&p,header,sizeof(header)-1)){result|=4;goto done;}}
 hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);if(FAILED(hr))goto failed;initialized=TRUE;
 com_phase="co-security";hr=CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);if(FAILED(hr))goto failed;
 com_phase="co-create-locator";hr=CoCreateInstance(&CLSID_WbemLocator,NULL,CLSCTX_INPROC_SERVER,&IID_IWbemLocator,(void **)&locator);if(FAILED(hr))goto failed;
 com_phase="wmi-namespace";ns=SysAllocString(L"ROOT\\WMI");if(!ns){hr=E_OUTOFMEMORY;goto failed;}
 com_phase="wmi-connect";hr=IWbemLocator_ConnectServer(locator,ns,NULL,NULL,NULL,WBEM_FLAG_CONNECT_USE_MAX_WAIT,NULL,NULL,&p.services);if(FAILED(hr))goto failed;
 com_phase="wmi-proxy";hr=secure((IUnknown *)p.services);if(FAILED(hr))goto failed;
 ops.context=&p;ops.snapshot=snapshot;ops.set=set;ops.read=read_level;ops.record=record;ops.pause=pause_read;
 result=a04_brightness_once(&ops);goto done;
failed:
 if(record(&p,com_phase,0,0,(int)hr))result|=4;
 if(record(&p,"com-preflight-failed",0,0,(int)hr))result|=4;
done:
 if(p.input)IWbemClassObject_Release(p.input);if(p.services)IWbemServices_Release(p.services);if(locator)IWbemLocator_Release(locator);
 SysFreeString(ns);SysFreeString(p.name);SysFreeString(p.monitor_path);SysFreeString(p.method_path);SysFreeString(p.method_name);
 if(initialized)CoUninitialize();if(p.log_failed)result|=4;CloseHandle(p.file);return result;
}
#ifndef A04_BRIGHTNESS_NATIVE_NO_ENTRY
void WinMainCRTStartup(void){ExitProcess(A04RunBrightnessNativeProbe());}
#endif
