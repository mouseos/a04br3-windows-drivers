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
 X(eglGetConfigAttrib) X(eglCreatePbufferSurface) X(eglCreateContext) X(eglMakeCurrent) X(eglGetError) \
 X(eglQueryString) X(eglDestroyContext) X(eglDestroySurface) X(eglTerminate)
#define GL_FUNCS(X) X(glGetString) X(glGetError) X(glDisable) X(glColorMask) \
 X(glClearColor) X(glClear) X(glFinish) X(glReadPixels)
#define GL_SUITE_FUNCS(X) X(glViewport) X(glCreateShader) X(glShaderSource) X(glCompileShader) \
 X(glGetShaderiv) X(glGetShaderInfoLog) X(glCreateProgram) X(glAttachShader) X(glBindAttribLocation) \
 X(glLinkProgram) X(glGetProgramiv) X(glGetProgramInfoLog) X(glDeleteShader) X(glUseProgram) \
 X(glGetUniformLocation) X(glUniform4f) X(glUniform1i) X(glVertexAttribPointer) \
 X(glEnableVertexAttribArray) X(glDrawArrays) X(glGenTextures) X(glBindTexture) \
 X(glTexParameteri) X(glTexImage2D) X(glActiveTexture)
#define DECL(n) __typeof__(&n) n;
struct api { EGL_FUNCS(DECL) GL_FUNCS(DECL) GL_SUITE_FUNCS(DECL) };
#undef DECL
static int symbol(void *lib,const char *name,void *dst,size_t bytes)
{
    void *p=dlsym(lib,name);
    if(!p || bytes!=sizeof(p)){fprintf(stderr,"MISSING_SYMBOL %s\n",name);return 1;}
    memcpy(dst,&p,bytes);return 0;
}
#include "recovery-native-suite.h"
int main(int argc,char **argv)
{
    struct api a={0};void *el=0,*gl=0;int result=1,ready=0,suite=0;
    EGLDisplay display=EGL_NO_DISPLAY;EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;EGLConfig config;EGLint major,minor,count;
    unsigned char *pixels=0;size_t i,mismatch=0,first=(size_t)-1;
    const char *renderer;
    const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_DEPTH_SIZE,0,EGL_STENCIL_SIZE,0,EGL_SAMPLES,0,EGL_NONE};
    const EGLint shape[]={EGL_WIDTH,WIDTH,EGL_HEIGHT,HEIGHT,EGL_NONE};
    const EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    if((argc!=4 && argc!=5) || strcmp(argv[1],"--run") || (argc==5 && strcmp(argv[4],"--suite"))){
        fprintf(stderr,"usage: %s --run /absolute/libEGL.so /absolute/libGLESv2.so [--suite]\n",argv[0]);
        return 2;
    }
    suite=argc==5;
    if(argv[2][0]!='/' || argv[3][0]!='/')return 2;
    /* No default execution: only the explicit --run path can load the driver. */
    setvbuf(stderr,0,_IONBF,0);alarm(30);
    if(getenv("A04_M111_CAPTURE")){
        int (*capture_ready)(void)=0;
        if(symbol(RTLD_DEFAULT,"a04_m111_capture_ready",&capture_ready,sizeof(capture_ready)) ||
           !capture_ready()){
            fprintf(stderr,"CAPTURE_NOT_READY\n");return 1;
        }
    }
    el=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);
    if(!el){fprintf(stderr,"DLOPEN_FAILED EGL %s\n",dlerror());goto done;}
    gl=dlopen(argv[3],RTLD_NOW|RTLD_LOCAL);
    if(!gl){fprintf(stderr,"DLOPEN_FAILED GLES %s\n",dlerror());goto done;}
#define LOAD_E(n) if(symbol(el,#n,&a.n,sizeof(a.n)))goto done;
#define LOAD_G(n) if(symbol(gl,#n,&a.n,sizeof(a.n)))goto done;
    EGL_FUNCS(LOAD_E) GL_FUNCS(LOAD_G)
    if(suite){GL_SUITE_FUNCS(LOAD_G)}
#undef LOAD_E
#undef LOAD_G
    fprintf(stderr,"PHASE egl-init\n");
    fprintf(stderr,"PHASE egl-get-display\n");
    display=a.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY)goto egl_fail;
    fprintf(stderr,"PHASE egl-initialize\n");
    if(!a.eglInitialize(display,&major,&minor))goto egl_fail;
    ready=1;
    fprintf(stderr,"EGL %d.%d vendor=%s version=%s\n",major,minor,
        a.eglQueryString(display,EGL_VENDOR),a.eglQueryString(display,EGL_VERSION));
    if(!a.eglBindAPI(EGL_OPENGL_ES_API) ||
       !a.eglChooseConfig(display,attrs,&config,1,&count) || count!=1)goto egl_fail;
    /* eglChooseConfig attributes are minima, not exact constraints. */
    {
        const EGLint keys[]={EGL_RED_SIZE,EGL_GREEN_SIZE,EGL_BLUE_SIZE,EGL_ALPHA_SIZE,
            EGL_DEPTH_SIZE,EGL_STENCIL_SIZE,EGL_SAMPLES,EGL_SAMPLE_BUFFERS};
        const EGLint expected[]={8,8,8,8,0,0,0,0};
        EGLint value;
        for(i=0;i<sizeof(keys)/sizeof(keys[0]);i++){
            if(!a.eglGetConfigAttrib(display,config,keys[i],&value))goto egl_fail;
            if(value!=expected[i]){
                fprintf(stderr,"REJECT_CONFIG attribute=0x%x actual=%d expected=%d\n",
                    keys[i],value,expected[i]);goto done;
            }
        }
        fprintf(stderr,"CONFIG rgba=8,8,8,8 depth=0 stencil=0 samples=0 buffers=0\n");
    }
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
    for(unsigned frame=0;frame<(suite?4u:1u);frame++){
        static const unsigned char colours[4][4]={{17,34,51,255},{68,85,102,255},{204,51,102,255},{51,153,204,255}};
        const unsigned char *rgba=colours[frame];mismatch=0;first=(size_t)-1;
        fprintf(stderr,"FRAME_BEGIN index=%u role=%s\n",frame,frame<2?"clear":frame==2?"solid-triangle":"texture-triangle");
        if(frame<2){
            a.glClearColor(rgba[0]/255.0f,rgba[1]/255.0f,rgba[2]/255.0f,1.0f);
            a.glClear(GL_COLOR_BUFFER_BIT);
        }else if(suite_draw(&a,frame==3,rgba))goto done;
        a.glFinish();
        if(a.glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL_FRAME_FAILED index=%u\n",frame);goto done;}
        a.glReadPixels(0,0,WIDTH,HEIGHT,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        if(a.glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL_READBACK_FAILED index=%u\n",frame);goto done;}
        for(i=0;i<(size_t)WIDTH*HEIGHT;i++){
            const unsigned char *px=pixels+i*4u;
            unsigned x=(unsigned)(i%WIDTH),y=(unsigned)(i/WIDTH);
            int inside=frame<2 || suite_inside(x,y);
            if(px[0]!=(inside?rgba[0]:0) || px[1]!=(inside?rgba[1]:0) ||
               px[2]!=(inside?rgba[2]:0) || px[3]!=255){if(!mismatch)first=i;mismatch++;}
        }
        fprintf(stderr,"READBACK frame=%u pixels=%u mismatch=%zu first=%zu\n",frame,WIDTH*HEIGHT,mismatch,first);
        if(mismatch)goto done;
        fprintf(stderr,"FRAME_END index=%u verified=1\n",frame);
    }
    result=0;goto done;
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
