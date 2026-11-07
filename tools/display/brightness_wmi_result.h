/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BRIGHTNESS_WMI_RESULT_H
#define A04_BRIGHTNESS_WMI_RESULT_H
/* Internal fixed-brightness transport adapter, no external query/command API. */
struct a04_wmi_result_ops {
 void *context;
 int (*exec)(void *,void **,void **);
 int (*secure)(void *,void *);
 /* GetCallStatus transport HRESULT and original operation HRESULT are
  * distinct. Both must succeed before asking for output parameters. */
 int (*completion)(void *,void *,int *);
 int (*result)(void *,void *,void **);
 int (*value)(void *,void *,unsigned *);
 void (*release_object)(void *,void *);
 void (*release_call)(void *,void *);
 void (*trace)(void *,const char *,unsigned,int);
 /* Set only from successful GetMethod signature introspection. */
 unsigned output_required;
};
int a04_brightness_method_result(const struct a04_wmi_result_ops *,unsigned *);
#endif
