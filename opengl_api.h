#pragma once

#include <GL/glcorearb.h>
#include <GL/gl.h>

extern PFNGLCREATESHADERPROC  glCreateShader;
extern PFNGLSHADERSOURCEPROC  glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC   glGetShaderiv;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLATTACHSHADERPROC  glAttachShader;
extern PFNGLLINKPROGRAMPROC   glLinkProgram;
extern PFNGLGETPROGRAMIVPROC  glGetProgramiv;
extern PFNGLUSEPROGRAMPROC    glUseProgram;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM1IVPROC glUniform1iv;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLBINDTEXTUREUNITPROC glBindTextureUnit; // Extension
extern PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
extern PFNGLGETSTRINGIPROC glGetStringi;
extern PFNGLGENBUFFERSPROC  glGenBuffers;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;