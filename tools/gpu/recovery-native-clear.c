/* SPDX-License-Identifier: MIT
 * Explicit opt-in Android ARM64 pbuffer probe. This uses only original EGL/GL
 * APIs; no direct GPU ioctl or command packet is constructed. See README.
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WIDTH 800
#define HEIGHT 600
#define EGL_FUNCS(X) X(eglGetDisplay) X(eglInitialize) X(eglBindAPI) X(eglChooseConfig) \
 X(eglCreatePbufferSurface) X(eglCreateContext) X(eglMakeCurrent) X(eglGetError) \
 X(eglQueryString) X(eglDestroyContext) X(eglDestroySurface) X(eglTerminate)
#define GL_FUNCS(X) X(glGetString) X(glGetError) X(glDisable) X(glColorMask) \
 X(glClearColor) X(glClear) X(glFinish) X(glReadPixels)
#define DECL(n) __typeof__(&n) n;
struct api { EGL_FUNCS(DECL) GL_FUNCS(DECL) };
#undef DECL
static int symbol(void *lib,const char *name,void *dst,size_t bytes)
{
    void *p=dlsym(lib,name);
    if(!p || bytes!=sizeof(p)){fprintf(stderr,"MISSING_SYMBOL %s\n",name);return 1;}
    memcpy(dst,&p,bytes);return 0;
}
int main(int argc,char **argv)
{
    struct api a={0};void *el=0,*gl=0;int result=1,ready=0;
    EGLDisplay display=EGL_NO_DISPLAY;EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;EGLConfig config;EGLint major,minor,count;
    unsigned char *pixels=0;size_t i,mismatch=0,first=(size_t)-1;
    const char *renderer;
    const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_DEPTH_SIZE,0,EGL_STENCIL_SIZE,0,EGL_SAMPLES,0,EGL_NONE};
    const EGLint shape[]={EGL_WIDTH,WIDTH,EGL_HEIGHT,HEIGHT,EGL_NONE};
    const EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    if(argc!=4 || strcmp(argv[1],"--run")){
        fprintf(stderr,"usage: %s --run /absolute/libEGL.so /absolute/libGLESv2.so\n",argv[0]);
        return 2;
    }
    if(argv[2][0]!='/' || argv[3][0]!='/')return 2;
    /* No default execution: only the explicit --run path can load the driver. */
    setvbuf(stderr,0,_IONBF,0);alarm(30);
    el=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);
    if(!el){fprintf(stderr,"DLOPEN_FAILED EGL %s\n",dlerror());goto done;}
    gl=dlopen(argv[3],RTLD_NOW|RTLD_LOCAL);
    if(!gl){fprintf(stderr,"DLOPEN_FAILED GLES %s\n",dlerror());goto done;}
#define LOAD_E(n) if(symbol(el,#n,&a.n,sizeof(a.n)))goto done;
#define LOAD_G(n) if(symbol(gl,#n,&a.n,sizeof(a.n)))goto done;
    EGL_FUNCS(LOAD_E) GL_FUNCS(LOAD_G)
#undef LOAD_E
#undef LOAD_G
    fprintf(stderr,"PHASE egl-init\n");
    display=a.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY || !a.eglInitialize(display,&major,&minor))goto egl_fail;
    ready=1;
    fprintf(stderr,"EGL %d.%d vendor=%s version=%s\n",major,minor,
        a.eglQueryString(display,EGL_VENDOR),a.eglQueryString(display,EGL_VERSION));
    if(!a.eglBindAPI(EGL_OPENGL_ES_API) ||
       !a.eglChooseConfig(display,attrs,&config,1,&count) || count!=1)goto egl_fail;
    surface=a.eglCreatePbufferSurface(display,config,shape);
    context=a.eglCreateContext(display,config,EGL_NO_CONTEXT,version);
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT ||
       !a.eglMakeCurrent(display,surface,surface,context))goto egl_fail;
    renderer=(const char *)a.glGetString(GL_RENDERER);
    fprintf(stderr,"GL vendor=%s renderer=%s version=%s\n",a.glGetString(GL_VENDOR),
        renderer?renderer:"NULL",a.glGetString(GL_VERSION));
    if(!renderer || (!strstr(renderer,"PowerVR") && !strstr(renderer,"GE8300"))){
        fprintf(stderr,"REJECT_NON_POWERVR_RENDERER\n");goto done;
    }
    pixels=malloc((size_t)WIDTH*HEIGHT*4u);if(!pixels)goto done;
    a.glDisable(GL_SCISSOR_TEST);a.glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    a.glClearColor(17.0f/255.0f,34.0f/255.0f,51.0f/255.0f,1.0f);
    fprintf(stderr,"PHASE clear-begin\n");a.glClear(GL_COLOR_BUFFER_BIT);a.glFinish();
    if(a.glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL_CLEAR_FAILED\n");goto done;}
    fprintf(stderr,"PHASE clear-finished readback-begin\n");
    a.glReadPixels(0,0,WIDTH,HEIGHT,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    if(a.glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL_READBACK_FAILED\n");goto done;}
    for(i=0;i<(size_t)WIDTH*HEIGHT;i++){
        const unsigned char *p=pixels+i*4u;
        if(p[0]!=17 || p[1]!=34 || p[2]!=51 || p[3]!=255){
            if(!mismatch)first=i;
            mismatch++;
        }
    }
    fprintf(stderr,"READBACK pixels=%u mismatch=%zu first=%zu\n",WIDTH*HEIGHT,mismatch,first);
    result=mismatch?1:0;goto done;
egl_fail:
    fprintf(stderr,"EGL_FAILED error=0x%x\n",a.eglGetError());
done:
    if(ready){
        if(a.eglMakeCurrent)a.eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(context!=EGL_NO_CONTEXT && a.eglDestroyContext)a.eglDestroyContext(display,context);
        if(surface!=EGL_NO_SURFACE && a.eglDestroySurface)a.eglDestroySurface(display,surface);
        if(a.eglTerminate)a.eglTerminate(display);
    }
    free(pixels);if(gl)dlclose(gl);if(el)dlclose(el);alarm(0);
    return result;
}
