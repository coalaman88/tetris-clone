#include <GL/glcorearb.h>
#include "engine.h"
#include "game.h"
#include "renderer.h"
#include "opengl_api.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Opengl reminders:
// It is advised however, that you stick to powers-of-two for
// texture sizes, unless you have a significant need to use arbitrary sizes.

PFNGLCREATESHADERPROC  glCreateShader;
PFNGLSHADERSOURCEPROC  glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC   glGetShaderiv;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLLINKPROGRAMPROC   glLinkProgram;
PFNGLGETPROGRAMIVPROC  glGetProgramiv;
PFNGLUSEPROGRAMPROC    glUseProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLATTACHSHADERPROC  glAttachShader;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGENBUFFERSPROC    glGenBuffers;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLUNIFORM3FVPROC glUniform3fv;
PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORM1IVPROC glUniform1iv;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
PFNGLBINDTEXTUREUNITPROC glBindTextureUnit;
PFNGLGETSTRINGIPROC glGetStringi;
//PFNGLDRAWELEMENTSPROC glDrawElements;

PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

const Vec4 White_v4  = {1.0f, 1.0f, 1.0f, 1.0f};
const Vec4 Black_v4  = {0.0f, 0.0f, 0.0f, 1.0f};
const Vec4 Red_v4    = {1.0f, 0.0f, 0.0f, 1.0f};
const Vec4 Green_v4  = {0.0f, 1.0f, 0.0f, 1.0f};
const Vec4 Blue_v4   = {0.0f, 0.0f, 1.0f, 1.0f};
const Vec4 Yellow_v4 = {1.0f, 1.0f, 0.0f, 1.0f};

struct S_ShaderContext PrimitiveShader;
struct S_ShaderContext TextureShader;

static GLuint vertex_array_obj, vertex_buffer_obj;

static Vertex vertex_buffer[60000];
static i32 vertex_count = 0;

typedef struct{
    i32 type;
    u32 tex_id;
    i32 count_vertex;
    ShaderContext *shader_context;
}Batch;

static struct{
    Batch batchs[124];
    Batch *current;
    i32 count;
}BatchList;

GLuint create_program(HANDLE vert_file, HANDLE frag_file);
b32 compile_shader(GLuint shader);

static void init_batchs(void){
    BatchList.count = 0;
    BatchList.current = NULL;
    vertex_count = 0;
}

void push_render_quad_command(ShaderContext *context, u32 tex_id, Quad *data){
    if(array_size(vertex_buffer) - vertex_count <= 6 || BatchList.count >= array_size(BatchList.batchs))
        draw_quads();

    if(BatchList.current == NULL || tex_id != BatchList.current->tex_id || BatchList.current->shader_context != context){
        BatchList.current = BatchList.batchs + BatchList.count++;
        BatchList.current->tex_id = tex_id;
        BatchList.current->count_vertex = 6;
        BatchList.current->shader_context = context;
        BatchList.current->type = GL_TRIANGLES;
    } else{
        BatchList.current->count_vertex += 6;
    }

    memcpy(vertex_buffer + vertex_count, data, sizeof(Quad));
    vertex_count += 6;
}

static inline void use_shader_context(ShaderContext *context){
    void *vert_info = context->debug_info.vert_file_info;
    void *frag_info = context->debug_info.frag_file_info;

    if(update_file_info(vert_info) | update_file_info(frag_info)){
        printf("updating\n");
        HANDLE vert_file = get_file_handle(vert_info);
        HANDLE frag_file = get_file_handle(frag_info);
        b32 new_program = create_program(vert_file, frag_file);

        if(new_program){
            glDeleteProgram(context->program_id);
            context->program_id = new_program;
            context->debug_info.program_is_ready = false;
        }
    }

    glUseProgram(context->program_id);
    if(!context->debug_info.program_is_ready)
        context->debug_info.setup_shader(context);
}

void draw_quads(void){
    i32 vertex_start = 0;
    glBufferSubData(GL_ARRAY_BUFFER, vertex_start, sizeof(Vertex) * vertex_count, vertex_buffer);

    for(i32 i = 0; i < BatchList.count; i++){
        Batch *batch = &BatchList.batchs[i];

        assert(batch->type && batch->shader_context);
        use_shader_context(batch->shader_context);
        glBindTexture(GL_TEXTURE_2D, batch->tex_id);
        glDrawArrays(batch->type, vertex_start, batch->count_vertex);
        vertex_start += batch->count_vertex;
    }

    init_batchs();
}

u32 create_texture_from_bitmap(u8 *data, i32 width, i32 height){
    u32 id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return id;
}

TextureInfo load_texture(const char *file_name){
    TextureInfo tex;
    u8 *texture_data = stbi_load(file_name, &tex.width, &tex.height, NULL, 4);
    assert(texture_data);
    tex.id = create_texture_from_bitmap(texture_data, tex.width, tex.height);
    stbi_image_free(texture_data);
    return tex;
}

GLuint create_program(HANDLE vert_file, HANDLE frag_file){
    i32 result;
    char error_buffer[200];
    i32 error_string_size;

    i32 vert_size, frag_size;
    const char *vert_data = read_whole_file(vert_file, &vert_size);
    const char *frag_data = read_whole_file(frag_file, &frag_size);

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(frag, 1, &frag_data, &frag_size);
    glShaderSource(vert, 1, &vert_data, &vert_size);

    VirtualFree((void*)vert_data, 0, MEM_RELEASE);
    VirtualFree((void*)frag_data, 0, MEM_RELEASE);

    if(!compile_shader(frag) || !compile_shader(vert))
         return 0;

    GLuint program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &result);
    assert(result == 2);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    glGetProgramInfoLog(program, sizeof(error_buffer), &error_string_size, error_buffer);
    printf("Link:\n%s\n", error_buffer);
    if(!result) return 0;

    glDeleteShader(frag);
    glDeleteShader(vert);

    return program;
}

b32 compile_shader(GLuint shader){
    char error_buffer[400];
    i32 error_string_size, result;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    glGetShaderInfoLog(shader, sizeof(error_buffer), &error_string_size, error_buffer);
    printf("Shader Compilation:\n%s", error_buffer);
    return result;
}

void set_commun_uniforms(ShaderContext *context){
    const GLfloat ident_matrix[4][4] = {
        { 1.0,  0.0,  0.0,  0.0},
        { 0.0,  1.0,  0.0,  0.0},
        { 0.0,  0.0,  1.0,  0.0},
        { 0.0,  0.0,  0.0,  1.0}
    };

    float a = 2.0f / WWIDTH;
    float b = 2.0f / WHEIGHT;

    const GLfloat translation_matrix[4][4] = {
        {   a,  0.0,  0.0, -1.0},
        { 0.0,   -b,  0.0,  1.0},
        { 0.0,  0.0,  1.0,  0.0},
        { 0.0,  0.0,  0.0,  1.0}
    };

    GLint location;
    location = glGetUniformLocation(context->program_id, "ident_matrix");
    glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)ident_matrix);

    location = glGetUniformLocation(context->program_id, "trans_matrix");
    glUniformMatrix4fv(location, 1, GL_TRUE, (GLfloat*)translation_matrix);

    GLint text_location = glGetUniformLocation(context->program_id, "sample_tex");
    glUniform1i(text_location, 0);
}

b32 create_shader_context(ShaderContext *context, const char *vert_shader, const char *frag_shader, void (*setup_shader)(ShaderContext*)){
    HANDLE vert_file = CreateFileA(vert_shader, GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE frag_file = CreateFileA(frag_shader, GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    GLuint program = create_program(vert_file, frag_file);
    if(!program)
        return false;

    context->program_id = program;
    // Debug
    context->debug_info.setup_shader = setup_shader;
    context->debug_info.vert_file_info = create_file_info(vert_file);
    context->debug_info.frag_file_info = create_file_info(frag_file);

    glUseProgram(context->program_id);
    setup_shader(context);
    glUseProgram(0);

    return true;
}

void setup_texture_shader(ShaderContext *context){
    set_commun_uniforms(context);
    context->debug_info.program_is_ready = true;
}

void setup_primitive_shader(ShaderContext *context){
    set_commun_uniforms(context);
    context->debug_info.program_is_ready = true;
}

void print_all_gl_extensions(void){
    i32 num;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &num);
    printf("max texture size:%dx%d\n", num / 2, num / 2);
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    printf("extension number:%d\n", num);
    for(i32 i = 0; i < num; i++){
        printf(" %s\n", glGetStringi(GL_EXTENSIONS, i));
    }
}

void init_renderer(void){
    const unsigned char *version = glGetString(GL_VERSION);
    printf("opengl version:%s\n", version);
    //print_all_gl_extensions();

    /* Allocate and assign a Vertex Array Object to our handle */
    glGenVertexArrays(1, &vertex_array_obj);

    /* Bind our Vertex Array Object as the current used object */
    glBindVertexArray(vertex_array_obj);

    glGenBuffers(1, &vertex_buffer_obj);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_obj);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer), NULL, GL_STREAM_DRAW); // Copy buffer

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));      // This only tell opengl what is what in
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texture_coord)); // the buffer!
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));    

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    b32 result0 = create_shader_context(&TextureShader, "shaders/simple.vert", "shaders/simple.frag", setup_texture_shader);
    b32 result1 = create_shader_context(&PrimitiveShader, "shaders/primitive.vert", "shaders/primitive.frag", setup_primitive_shader);
    assert(result0 && result1);

    glViewport(0, 0, WWIDTH, WHEIGHT);
    glActiveTexture(GL_TEXTURE0);
    glDepthRange(0, 1);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    GLuint error;
    while(error = glGetError(), error)
        printf("Error:%x\n", error);

    init_batchs();
    stbi_set_flip_vertically_on_load(true);
}


void immediate_draw_rect(f32 x1, f32 y1, f32 w, f32 h, Vec4 color){

    f32 x2 = x1 + w;
    f32 y2 = y1 + h;

    Quad new_quad = {
        {{ x1, y1 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
        {{ x1, y2 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
        {{ x2, y2 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
        {{ x2, y2 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
        {{ x2, y1 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
        {{ x1, y1 }, { 0.0f, 0.0f }, { color.x, color.y, color.z, color.w }},
    };

    push_render_quad_command(&PrimitiveShader, 0, &new_quad);
}

void immediate_draw_texture(float x1, float y1, f32 scale, TextureInfo tex){

    f32 x2 = roundf(tex.width    * scale);
    f32 y2 = roundf(tex.height * scale);

    Quad new_quad = {
        {{ x1 +  0, y1 + y2 }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
        {{ x1 +  0, y1 +  0 }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
        {{ x1 + x2, y1 +  0 }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
        {{ x1 + x2, y1 +  0 }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
        {{ x1 + x2, y1 + y2 }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
        {{ x1 +  0, y1 + y2 }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }},
    };

    push_render_quad_command(&TextureShader, tex.id, &new_quad);
}

void clear_screen(Vec4 color){
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

Vec4 brightness(Vec4 color, f32 scaler){
    scaler   = clampf(scaler, 0.0f, 1.0f);
    color.x *= scaler;
    color.y *= scaler;
    color.z *= scaler;
    return color;
}

