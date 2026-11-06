/* SPDX-License-Identifier: MIT
 * Primary-operated Recovery launcher. No GPU open here. Mounts are private;
 * only new files below the supplied /tmp stage are created. No block writes.
 */
#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/stat.h>
#include <sched.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static void fail(const char *what)
{ fprintf(stderr,"NATIVE_STAGE_FAILED %s errno=%d\n",what,errno);exit(2); }
static void path(char *out,const char *base,const char *suffix)
{ if(snprintf(out,PATH_MAX,"%s/%s",base,suffix)>=PATH_MAX){errno=ENAMETOOLONG;fail("path");} }
static void directory(const char *p)
{ if(mkdir(p,0700))fail(p); }
static void bind_dir(const char *source,const char *target)
{ if(mount(source,target,0,MS_BIND|MS_REC,0))fail(source); }
static int exists_dir(const char *p)
{ struct stat s;return !stat(p,&s) && S_ISDIR(s.st_mode); }
int main(int argc,char **argv)
{
    char *base,root[PATH_MAX],target[PATH_MAX],source[PATH_MAX],link[PATH_MAX];
    char fw[PATH_MAX]={0};DIR *system;struct dirent *entry;FILE *f;
    static const char *const dirs[]={"system-origin","system","vendor","stage","deps",
        "capture","dev","proc","sys","apex","linkerconfig","lib","lib/firmware","sbin","bin","lib64","etc"};
    static const char *const inherited[]={"dev","proc","sys","apex","linkerconfig","sbin","bin","lib64","etc"};
    unsigned i;int inspect;
    if(argc!=3 || (strcmp(argv[1],"--run") && strcmp(argv[1],"--inspect"))){
        fprintf(stderr,"usage: recovery-native-stage --run|--inspect /tmp/fresh-stage\n");return 2;
    }
    inspect=!strcmp(argv[1],"--inspect");
    base=realpath(argv[2],0);
    if(!base || strncmp(base,"/tmp/",5) || geteuid()!=0){errno=EINVAL;fail("stage/root requirement");}
    umask(077);alarm(15);
    /* Source firmware_class.c has this path before the /lib/firmware defaults. */
    f=fopen("/sys/module/firmware_class/parameters/path","r");
    if(f){if(!fgets(fw,sizeof(fw),f))fw[0]=0;fclose(f);fw[strcspn(fw,"\r\n")]=0;}
    if(fw[0] && strcmp(fw,"/vendor/firmware") && strcmp(fw,"/system/vendor/firmware") &&
       strcmp(fw,"/lib/firmware")){errno=EINVAL;fail("unsupported firmware search path");}
    /* A failure here occurs before any driver load, mount or GPU descriptor. */
    if(unshare(CLONE_NEWNS))fail("unshare mount namespace");
    if(mount(0,"/",0,MS_REC|MS_PRIVATE,0))fail("private mount propagation");
    if(inspect){
        char data[4096];size_t n;
        path(target,base,"debugfs");directory(target);
        if(mount("debugfs",target,"debugfs",MS_RDONLY|MS_NOSUID|MS_NODEV|MS_NOEXEC,0))fail("read-only debugfs");
        path(source,target,"pvr/version");f=fopen(source,"r");if(!f)fail("pvr version");
        while((n=fread(data,1,sizeof(data),f)))fwrite(data,1,n,stdout);
        if(ferror(f))fail("read pvr version");
        fclose(f);return 0;
    }
    path(root,base,"root");directory(root);
    for(i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++){path(target,root,dirs[i]);directory(target);}
    path(target,root,"system-origin");bind_dir("/system",target);
    if(mount(0,target,0,MS_BIND|MS_REMOUNT|MS_RDONLY,0))fail("read-only system view");
    /* Preserve the real Recovery Bionic/linker and its absolute APEX symlinks. */
    system=opendir("/system");if(!system)fail("opendir system");
    while((entry=readdir(system))){
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..") || !strcmp(entry->d_name,"vendor"))continue;
        if(snprintf(target,sizeof(target),"%s/system/%s",root,entry->d_name)>=(int)sizeof(target) ||
           snprintf(link,sizeof(link),"/system-origin/%s",entry->d_name)>=(int)sizeof(link))fail("system link path");
        if(symlink(link,target))fail("system entry symlink");
    }
    closedir(system);path(target,root,"system/vendor");if(symlink("/vendor",target))fail("vendor alias");
    for(i=0;i<sizeof(inherited)/sizeof(inherited[0]);i++){
        if(snprintf(source,sizeof(source),"/%s",inherited[i])>=(int)sizeof(source))fail("inherited path");
        if(exists_dir(source)){path(target,root,inherited[i]);bind_dir(source,target);}
    }
    path(source,base,"original-vendor");path(target,root,"vendor");bind_dir(source,target);
    path(source,base,"bin");path(target,root,"stage");bind_dir(source,target);
    path(source,base,"platform-candidates");path(target,root,"deps");bind_dir(source,target);
    path(source,base,"runtime");directory(source);path(target,root,"capture");bind_dir(source,target);
    path(source,base,"original-vendor/firmware");path(target,root,"lib/firmware");bind_dir(source,target);
    if(chroot(root) || chdir("/"))fail("chroot");
    if(setenv("A04_M111_CAPTURE","/capture/capture-01.bin",1) ||
       setenv("LD_PRELOAD","/stage/recovery-native-bridge-capture.so",1) ||
       setenv("LD_LIBRARY_PATH","/deps:/vendor/lib64:/vendor/lib64/egl:/system/lib64",1))fail("environment");
    fprintf(stderr,"NATIVE_STAGE_PRIVATE_ROOT fw_path=%s default=/lib/firmware fixed_egl=/system/vendor/lib64/egl/libGLESv2_mtk.so\n",fw[0]?fw:"(default)");
    {
        char *const command[]={"/stage/recovery-native-clear","--run",
            "/vendor/lib64/egl/libEGL_mtk.so","/vendor/lib64/egl/libGLESv2_mtk.so",0};
        execv(command[0],command);fail("exec clear harness");
    }
}
