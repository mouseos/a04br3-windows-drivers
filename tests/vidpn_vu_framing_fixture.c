/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Reuse the fixed8715 synthetic registry fixture AND its real C formatter.
 * No Windows calls; source/SYS words are synthetic, never deployable evidence.
 */
#define main existing_snapshot_fixture_main
#include "vidpn_uart_snapshot_test.c"
#undef main
static int dump_send(void *context,const char *line,unsigned bytes) {
 (void)context;return fwrite(line,1,bytes,stdout)!=bytes;
}
int main(int argc,char **argv) {
 struct a04_vut_uart uart={dump_send,NULL,0,0,0};unsigned i,e;
 reset();
 if(argc==1)for(i=0;i<A04_VUT_VALUES;i++)value(i);
 else if(argc!=2||strcmp(argv[1],"--missing"))return 2;
 e=a04_vut_collect(read_mock,NULL,&binding,a04_vut_uart_emit,&uart,&first,&second);
 return e==(argc==1?A04_VUT_OK:A04_VUT_MISSING)?0:1;
}
