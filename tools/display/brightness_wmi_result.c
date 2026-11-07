/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "brightness_wmi_result.h"
/* These are local contract errors, never reported as a provider method return. */
#define NO_RESULT ((int)0x8004100fu)
int a04_brightness_method_result(const struct a04_wmi_result_ops *o,unsigned *value)
{
 void *direct=0,*call=0,*result=0;int hr,operation=-1;
 *value=~0u;hr=o->exec(o->context,&direct,&call);
 o->trace(o->context,"wmi-dispatch",(direct?1u:0u)|(call?2u:0u),hr);
 if(hr==0 && call) {
  hr=o->secure(o->context,call);
  if(hr==0)hr=o->completion(o->context,call,&operation);
  o->trace(o->context,"wmi-completion-status",(unsigned)operation,hr);
  if(hr==0)hr=operation;
 }
 if(hr==0 && !o->output_required) {
  /* A declared void method has no ReturnValue. Completion is still mandatory;
   * no output object is fabricated and the caller must verify state readback. */
  if(!call)hr=NO_RESULT;
  o->trace(o->context,"wmi-declared-no-output",call?1u:0u,hr);
 } else if(hr==0 && direct)result=direct;
 else if(hr==0 && o->output_required && call) {
  hr=o->result(o->context,call,&result);
  o->trace(o->context,"wmi-output-object",result?1u:0u,hr);
 }
 if(hr==0 && o->output_required) {
  if(!result){hr=NO_RESULT;o->trace(o->context,"wmi-output-missing",0,hr);}
  else {hr=o->value(o->context,result,value);o->trace(o->context,"wmi-return-decode",*value,hr);}
 }
 if(result && result!=direct)o->release_object(o->context,result);
 if(direct)o->release_object(o->context,direct);
 if(call)o->release_call(o->context,call);
 return hr;
}
