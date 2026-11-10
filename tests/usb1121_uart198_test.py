#!/usr/bin/env python3
"""実formatter/停止observerをhost stubで検査。Windows service/driverはbuildしない。"""
import argparse,json,os,re,subprocess
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
def function(text,name):
 start=text.index(name+'(');start=text.rfind('\n',0,start)+1
 brace=text.index('{',start);end=brace+1;depth=1
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 return text[start:end]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
 c=(repo/'tools/usb/mt8167-usb1121-install-service.c').read_text();h=(repo/'tools/usb/usb1121-service-uart.h').read_text()
 # Compile exact production functions with an event/Win32 boundary stub.
 pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
typedef uint32_t DWORD;
#define ERROR_SUCCESS_REBOOT_REQUIRED 3010
#define ERROR_RETRY 1237
#define U198_MAX_PLANNED_REBOOTS 4u
#define U198_START 1
#define U198_RUN_RESULT 2
#define U198_REBOOT_REQUIRED 3
#define U198_SCM_STOP 4
#define U198_VOLUME_FLUSH 5
#define U198_TERMINAL 6
#define U198_REBOOT_REQUEST 7
static DWORD uart_run,uart_pid,stop_error,flush_error,reboot_error,budget_count;
static int n;static DWORD events[32],codes[32];
static DWORD next_reboot_count(DWORD count,DWORD *next);
static DWORD wait_for_stopped(void){return stop_error;}
static DWORD open_log(void){return 0;}
static DWORD disable(void){return 0;}
static DWORD uart_only(DWORD e,DWORD code){events[n]=e;codes[n++]=code;return 0;}
static void record(DWORD e,DWORD code){uart_only(e,code);}
static DWORD close_log_and_flush_volume(void){uart_only(U198_VOLUME_FLUSH,flush_error);return flush_error;}
static DWORD flush_fixed_system_volume(void){return flush_error;}
static DWORD reboot(void){return reboot_error;}
static DWORD reserve_reboot(void){DWORD next;return next_reboot_count(budget_count,&next);}
'''
 tail=r'''
static void check(int ok){if(!ok){fprintf(stderr,"UART198 contract failure\n");exit(1);}}
static void scenario(DWORD result,DWORD stop,DWORD flush,DWORD rb,int terminal,DWORD code){
 int i,got=0,stopAt=-1,flushAt=-1;n=0;stop_error=stop;flush_error=flush;reboot_error=rb;uart_only(U198_START,0);uart_only(U198_RUN_RESULT,result);if(result==3010)uart_only(U198_REBOOT_REQUIRED,3010);terminal_observer(result);
 for(i=0;i<n;i++){if(events[i]==U198_SCM_STOP&&!codes[i])stopAt=i;if(events[i]==U198_VOLUME_FLUSH&&!codes[i])flushAt=i;if(events[i]==U198_TERMINAL){got++;check(stopAt>=0&&flushAt>stopAt&&i>flushAt&&codes[i]==code);}}
 check(got==terminal);check(events[0]==U198_START&&events[1]==U198_RUN_RESULT&&codes[1]==result);
 if(result==3010&&terminal&&rb==0&&budget_count<4){check(events[n-2]==U198_REBOOT_REQUEST&&codes[n-2]==0&&codes[n-1]==3010);}
}
int main(void){char line[64];DWORD bytes;uart_run=0x12345678;uart_pid=0x87654321;bytes=event_line(line,6,3010);check(bytes==50);check(!memcmp(line,"U198 000000C6 12345678 87654321 00000006 00000BC2\n",50));
 scenario(0,0,0,0,1,0);scenario(1,0,0,0,1,1);scenario(3010,0,0,0,1,3010);scenario(0,1,0,0,0,0);scenario(0,0,1,0,0,0);scenario(3010,0,0,5,1,5);budget_count=4;scenario(3010,0,0,0,1,1237);puts("UART198_FORMAT_AND_SEVEN_TERMINAL_BOUNDARIES_OK");return 0;}
'''
 observer_exe=re.search(r'static const WCHAR exe\[\]=(L"(?:\\.|[^"])*")',c)[1]
 observer_cmd=re.search(r'lstrcpyW\(cmd,(L"(?:\\.|[^"])*")\)',c)[1]
 literals='static const wchar_t actual_exe[]='+observer_exe+';\nstatic const wchar_t actual_cmd[]='+observer_cmd+';\n'
 tail=tail.replace('int main(void){','int main(void){check(actual_exe[0]==67&&actual_exe[1]==58&&actual_exe[2]==92&&actual_exe[3]==87);check(actual_cmd[0]==34&&actual_cmd[1]==67&&actual_cmd[2]==58&&actual_cmd[3]==92&&actual_cmd[4]==87);')
 fixture=pre+literals+'\n'+function(h,'hex')+'\n'+function(h,'event_line')+'\n'+function(c,'next_reboot_count')+'\n'+function(c,'terminal_observer')+'\n'+tail
 (out/'uart198-host.c').write_text(fixture)
 env=os.environ.copy();env['TMPDIR']=str(out)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(out/'uart198-host.c'),'-o',str(out/'uart198-host')],check=True,env=env)
 subprocess.run([str(out/'uart198-host')],check=True,env=env)
 (out/'result.json').write_text(json.dumps(dict(actual_production_formatter=True,actual_stop_observer=True,actual_observer_C_path_quote_literals=True,stubbed_Windows_boundaries=True,scenarios=7,Windows_service_built=False,UART_IO_executed=False,hardware_verified=False),indent=2)+'\n')
if __name__=='__main__':main()
