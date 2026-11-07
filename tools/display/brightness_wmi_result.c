/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "brightness_wmi_result.h"
#define NO_RESULT ((int)0x8004100fu) /* WBEM_E_INVALID_OBJECT, not synthetic E_UNEXPECTED. */
int a04_brightness_method_result(const struct a04_wmi_result_ops *o,unsigned *value)
{
 void *direct=0,*call=0,*result=0;int hr;
 *value=~0u;hr=o->exec(o->context,&direct,&call);
 o->trace(o->context,"wmi-dispatch",(direct?1u:0u)|(call?2u:0u),hr);
 if(hr==0 && direct) {
  /* ppOutParams already contains the completed method output. */
  result=direct;
 } else if(hr==0 && call) {
  hr=o->secure(o->context,call);
  if(hr==0)hr=o->result(o->context,call,&result);
  o->trace(o->context,"wmi-pending-result",result?1u:0u,hr);
 }
 if(hr==0) {
  if(!result)hr=NO_RESULT;
  else hr=o->value(o->context,result,value);
  o->trace(o->context,"wmi-return-decode",*value,hr);
 }
 if(result && result!=direct)o->release_object(o->context,result);
 if(direct)o->release_object(o->context,direct);
 if(call)o->release_call(o->context,call);
 return hr;
}
