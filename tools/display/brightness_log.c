/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "brightness_log.h"
int a04_brightness_log_write(struct a04_brightness_log *log,const char *data,unsigned n)
{
 if(!log || !data || !log->root || !log->stdout_write)return -1;
 /* A tee failure never skips an otherwise valid root write. A root failure
  * never hides the same record from the inherited capture handle. */
 if(n>A04_BRIGHTNESS_LOG_FRAGMENT_LIMIT){log->failed|=3u;return -1;}
 if(log->root(log->context,data,n))log->failed|=1u;
 if(log->stdout_bytes>A04_BRIGHTNESS_STDOUT_LIMIT || n>A04_BRIGHTNESS_STDOUT_LIMIT-log->stdout_bytes)log->failed|=2u;
 else {log->stdout_bytes+=n;if(log->stdout_write(log->context,data,n))log->failed|=2u;}
 return log->failed?-1:0;
}
