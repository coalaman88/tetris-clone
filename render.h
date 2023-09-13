#pragma once

#include <GL/glcorearb.h>
#include <GL/gl.h>

//#include <GL/glext.h>

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
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLBINDTEXTUREUNITPROC glBindTextureUnit; // Extension
extern PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
extern PFNGLGETSTRINGIPROC glGetStringi;
//extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLGENBUFFERSPROC  glGenBuffers;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

typedef struct{
    HANDLE file;
    FILETIME last_write;
}FileInfo;

typedef struct S_ShaderContext {
    u32 program_id;
    u32 vertex_array_id;

    struct{
        FileInfo vert_file_info, frag_file_info;
        b32 program_is_ready;
        void (*setup_shader)(struct S_ShaderContext*);
        const char **bind_attributes;
        i32 bind_attributes_count;
    }debug_info;

}ShaderContext;

extern struct S_ShaderContext TextureShader;
extern struct S_ShaderContext PrimitiveShader;

void init_render(void);
void push_render_quad_command(ShaderContext *context, u32 tex_id, Quad *data);
void draw_quads(void);
