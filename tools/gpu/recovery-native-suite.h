/* SPDX-License-Identifier: MIT
 * Four-frame ORIGINAL GLES workload; no direct packet or ioctl generation.
 * Same pbuffer/context: clear, warm clear, solid triangle, sampled texture.
 */
#ifndef A04_RECOVERY_NATIVE_SUITE_H
#define A04_RECOVERY_NATIVE_SUITE_H
/* Pixel-centre test for (-1,-1),(1,-1),(-1,1). For 800x600, the edge
 * numerator is odd and cannot equal 4800: no ambiguous on-edge samples. */
static int suite_inside(unsigned x,unsigned y)
{ return 3u*(2u*x+1u)+4u*(2u*y+1u)<4800u; }
static GLuint suite_shader(struct api *a,GLenum type,const char *source)
{
    GLuint shader=a->glCreateShader(type);GLint ok=0;char log[1024];
    if(!shader)return 0;
    a->glShaderSource(shader,1,&source,NULL);a->glCompileShader(shader);
    a->glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){a->glGetShaderInfoLog(shader,sizeof(log),NULL,log);fprintf(stderr,"SHADER_FAILED %s\n",log);a->glDeleteShader(shader);return 0;}
    return shader;
}
static int suite_draw(struct api *a,int textured,const unsigned char rgba[4])
{
    static const GLfloat vertices[]={-1,-1,1,-1,-1,1};
    const char *vs="attribute vec2 p;void main(){gl_Position=vec4(p,0.0,1.0);}";
    const char *fs=textured?"precision mediump float;uniform sampler2D t;void main(){gl_FragColor=texture2D(t,vec2(0.5));}":
        "precision mediump float;uniform vec4 c;void main(){gl_FragColor=c;}";
    GLuint v=suite_shader(a,GL_VERTEX_SHADER,vs),f=suite_shader(a,GL_FRAGMENT_SHADER,fs),program,texture=0;
    GLint ok=0,uniform;char log[1024];
    if(!v || !f)return 1;
    program=a->glCreateProgram();if(!program)return 1;
    a->glAttachShader(program,v);a->glAttachShader(program,f);a->glBindAttribLocation(program,0,"p");
    a->glLinkProgram(program);a->glGetProgramiv(program,GL_LINK_STATUS,&ok);
    a->glDeleteShader(v);a->glDeleteShader(f);
    if(!ok){a->glGetProgramInfoLog(program,sizeof(log),NULL,log);fprintf(stderr,"PROGRAM_FAILED %s\n",log);return 1;}
    a->glUseProgram(program);uniform=a->glGetUniformLocation(program,textured?"t":"c");
    if(uniform<0){fprintf(stderr,"UNIFORM_MISSING\n");return 1;}
    if(textured){
        a->glActiveTexture(GL_TEXTURE0);a->glGenTextures(1,&texture);a->glBindTexture(GL_TEXTURE_2D,texture);
        a->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        a->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        a->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        a->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        a->glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);a->glUniform1i(uniform,0);
    }else a->glUniform4f(uniform,rgba[0]/255.0f,rgba[1]/255.0f,rgba[2]/255.0f,1.0f);
    a->glViewport(0,0,WIDTH,HEIGHT);a->glDisable(GL_BLEND);a->glDisable(GL_DITHER);
    a->glDisable(GL_DEPTH_TEST);a->glDisable(GL_STENCIL_TEST);a->glDisable(GL_CULL_FACE);
    a->glClearColor(0,0,0,1);a->glClear(GL_COLOR_BUFFER_BIT);
    a->glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,vertices);a->glEnableVertexAttribArray(0);
    a->glDrawArrays(GL_TRIANGLES,0,3);return 0;
}
#endif
