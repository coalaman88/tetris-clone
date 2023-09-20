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
PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
PFNGLBINDTEXTUREUNITPROC glBindTextureUnit;
PFNGLGETSTRINGIPROC glGetStringi;

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
    i32 sample_tex;
    float uv_matrix[3][3];
    float ident_matrix[4][4];
    float translation_matrix[4][4];
}Uniforms;

typedef struct{
    i32 type;
    u32 tex_id;
    i32 vertices_count;
    ShaderContext *shader_context;
    Uniforms uniforms;
}DrawCommand;

static struct{
    DrawCommand batchs[124];
    DrawCommand *current;
    i32 count;
}BatchList;

typedef struct {
    b32 drawing;

    // Draw State
    // - per vertex effect
    Vec2 texture_coord;
    Vec4 color;

    // - global effect
    Uniforms uniforms;

    DrawCommand command;
}T_ImmediateDrawContext;

T_ImmediateDrawContext ImmediateDrawContext = {0};

static GLuint create_program(HANDLE vert_file, HANDLE frag_file);
static b32 compile_shader(GLuint shader);

static void init_batchs(i32 vertices_left){
    BatchList.count = 0;
    BatchList.current = NULL;
    vertex_count = vertices_left;
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
        }
    }

    glUseProgram(context->program_id);
}

u32 get_gl_mode(u32 renderer_type){
    renderer_type -= 1;
    assert(renderer_type >= 0);
    u32 lookup_table[] = {GL_TRIANGLES};
    assert(renderer_type < array_size(lookup_table));
    return lookup_table[renderer_type];
}

static void set_default_uniforms(Uniforms *uniforms){
    float a = 2.0f / WWIDTH;
    float b = 2.0f / WHEIGHT;

    const GLfloat translation_matrix[4][4] = {
        {   a,  0.0,  0.0, -1.0},
        { 0.0,   -b,  0.0,  1.0},
        { 0.0,  0.0,  1.0,  0.0},
        { 0.0,  0.0,  0.0,  1.0}
    };

    const GLfloat ident_matrix[4][4] = {
        { 1.0,  0.0,  0.0,  0.0},
        { 0.0,  1.0,  0.0,  0.0},
        { 0.0,  0.0,  1.0,  0.0},
        { 0.0,  0.0,  0.0,  1.0}
    };

    const GLfloat uv_matrix[3][3] = {
        { 1.0,  0.0,  0.0},
        { 0.0,  1.0,  0.0},
        { 0.0,  0.0,  1.0},
    };

    uniforms->sample_tex = 0;
    memcpy(uniforms->uv_matrix, uv_matrix, sizeof(uv_matrix));
    memcpy(uniforms->ident_matrix, ident_matrix, sizeof(ident_matrix));
    memcpy(uniforms->translation_matrix, translation_matrix, sizeof(translation_matrix));
}

void update_shader_uniforms(u32 program, const Uniforms *uniforms){
    i32 location;
    // TODO save the location insted. Save in the shader_context?
    location = glGetUniformLocation(program, "ident_matrix");
    glUniformMatrix4fv(location, 1, GL_TRUE, (float*)uniforms->ident_matrix);

    location = glGetUniformLocation(program, "trans_matrix");
    glUniformMatrix4fv(location, 1, GL_TRUE, (float*)uniforms->translation_matrix);

    location = glGetUniformLocation(program, "texture_trans_matrix");
    glUniformMatrix3fv(location, 1, GL_TRUE, (float*)uniforms->uv_matrix);

    location = glGetUniformLocation(program, "sample_tex");
    glUniform1i(location, uniforms->sample_tex);
}

void execute_draw_commands(void){
    i32 vertices_start = 0;
    i32 vertices_end   = vertex_count;
    i32 vertices_left  = 0;
    
    if(ImmediateDrawContext.drawing){
        vertices_end -= ImmediateDrawContext.command.vertices_count;
        vertices_left = ImmediateDrawContext.command.vertices_count;
    }
    glBufferSubData(GL_ARRAY_BUFFER, vertices_start, sizeof(Vertex) * vertices_end, vertex_buffer);

    for(i32 i = 0; i < BatchList.count; i++){
        DrawCommand *batch = &BatchList.batchs[i];
        assert(batch->type && batch->shader_context);
        use_shader_context(batch->shader_context);
        update_shader_uniforms(batch->shader_context->program_id, &batch->uniforms);
        glBindTexture(GL_TEXTURE_2D, batch->tex_id);
        u32 gl_mode = get_gl_mode(batch->type);
        glDrawArrays(gl_mode, vertices_start, batch->vertices_count);
        vertices_start += batch->vertices_count;
    }

    if(vertices_left){
        assert(false);
        i32 size  = sizeof(Vertex) * vertices_left;
        memmove(vertex_buffer, vertex_buffer + vertices_end, size);
    }
    init_batchs(vertices_left);
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

static GLuint create_program(HANDLE vert_file, HANDLE frag_file){
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

static b32 compile_shader(GLuint shader){
    char error_buffer[400];
    i32 error_string_size, result;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    glGetShaderInfoLog(shader, sizeof(error_buffer), &error_string_size, error_buffer);
    printf("Shader Compilation:\n%s", error_buffer);
    return result;
}

static b32 create_shader_context(ShaderContext *context, const char *vert_shader, const char *frag_shader){
    HANDLE vert_file = CreateFileA(vert_shader, GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE frag_file = CreateFileA(frag_shader, GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    GLuint program = create_program(vert_file, frag_file);
    if(!program)
        return false;

    context->program_id = program;
    // Debug
    context->debug_info.vert_file_info = create_file_info(vert_file);
    context->debug_info.frag_file_info = create_file_info(frag_file);

    return true;
}

static void print_all_gl_extensions(void){
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

    b32 result0 = create_shader_context(&TextureShader, "shaders/simple.vert", "shaders/simple.frag");
    b32 result1 = create_shader_context(&PrimitiveShader, "shaders/primitive.vert", "shaders/primitive.frag");
    assert(result0 && result1);

    glViewport(0, 0, WWIDTH, WHEIGHT);
    glActiveTexture(GL_TEXTURE0);
    glDepthRange(0, 1);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    set_default_uniforms(&ImmediateDrawContext.uniforms);

    GLuint error;
    while(error = glGetError(), error)
        printf("Error:%x\n", error);

    init_batchs(0);
    stbi_set_flip_vertically_on_load(true);
}

void set_simple_quad(f32 x, f32 y, f32 w, f32 h){
    set_texture_coord(Vec2(0.0f, 1.0f));
    set_vertex(Vec2(x, y));

    set_texture_coord(Vec2(1.0f, 1.0f));
    set_vertex(Vec2(x + w, y));

    set_texture_coord(Vec2(0.0f, 0.0f));
    set_vertex(Vec2(x, y + h));

    set_texture_coord(Vec2(0.0f, 0.0f));
    set_vertex(Vec2(x, y + h));

    set_texture_coord(Vec2(1.0f, 1.0f));
    set_vertex(Vec2(x + w, y));

    set_texture_coord(Vec2(1.0f, 0.0f));
    set_vertex(Vec2(x + w, y + h));
}

void immediate_draw_rect(f32 x, f32 y, f32 w, f32 h, Vec4 color){
    immediate_begin(DRAW_TRIANGLE);
    set_shader(&PrimitiveShader);
    set_color(color);
    set_simple_quad(x, y, w, h);
    immediate_end();
}

static inline b32 is_same_draw_command(const DrawCommand *a, const DrawCommand *b){
    if(!a || !b) return false;
    if(a->type != b->type) return false;
    if(a->tex_id != b->tex_id) return false;
    if(a->shader_context != b->shader_context) return false;
    
    const Uniforms *ua = &a->uniforms;
    const Uniforms *ub = &b->uniforms;
    if(ua->sample_tex != ub->sample_tex) return false;
    if(memcmp(ua->uv_matrix, ub->uv_matrix, sizeof(ua->uv_matrix)) != 0) return false;
    if(memcmp(ua->translation_matrix, ub->translation_matrix, sizeof(ua->translation_matrix)) != 0) return false;
    return true;
}

static void enqueue_render_command(void){
    const T_ImmediateDrawContext *context = &ImmediateDrawContext;
    if(BatchList.count >= array_size(BatchList.batchs))
        execute_draw_commands();

    if(!is_same_draw_command(BatchList.current, &context->command)){
        BatchList.current  = BatchList.batchs + BatchList.count++;
        *BatchList.current = context->command;
    } else{
        BatchList.current->vertices_count += context->command.vertices_count;
    }
}

void immediate_begin(i32 primitive){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(!context->drawing);
    context->drawing = true;
    context->texture_coord = Vec2(0, 0);
    context->color = Vec4(0, 0, 0, 0);
    context->command.vertices_count = 0;
    context->command.type = primitive;
    context->command.tex_id = 0;
    context->command.shader_context = &PrimitiveShader;
}

void immediate_end(void){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    assert(context->command.vertices_count % 3 == 0);
    context->command.uniforms = context->uniforms;
    enqueue_render_command();
    context->drawing = false;
}

void set_color(Vec4 color){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    context->color = color;
}

void set_texture_coord(Vec2 coord){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    context->texture_coord = coord;
}

void set_texture(u32 texture){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    context->command.tex_id = texture;
}

void set_shader(ShaderContext *shader){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    context->command.shader_context = shader;
}

void set_vertex(Vec2 pos){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    assert(context->drawing);
    
    if(vertex_count > array_size(vertex_buffer)){
        execute_draw_commands();
        assert(vertex_count < array_size(vertex_buffer)); // trying to draw something really big or forggot to call immediate_end
    }

    context->command.vertices_count++;
    Vertex *current = &vertex_buffer[vertex_count++];
    current->position[0] = pos.x;
    current->position[1] = pos.y;
    current->texture_coord[0] = context->texture_coord.x;
    current->texture_coord[1] = context->texture_coord.y;
    current->color[0] = context->color.x;
    current->color[1] = context->color.y;
    current->color[2] = context->color.z;
    current->color[3] = context->color.w;
}

void immediate_draw_texture(float x, float y, f32 scale, TextureInfo tex){
    f32 w = roundf(tex.width  * scale);
    f32 h = roundf(tex.height * scale);
    
    immediate_begin(DRAW_TRIANGLE);
    set_texture(tex.id);
    set_shader(&TextureShader);
    set_color(White_v4);
    set_simple_quad(x, y, w, h);
    immediate_end();
}

void immediate_draw_sprite(float x0, float y0, f32 scale, Vec4 color, Sprite sprite){
    f32 y1 = y0 + roundf(sprite.w * scale);
    f32 x1 = x0 + roundf(sprite.h * scale);
    
    f32 tex_x0 = (f32)sprite.x / sprite.atlas.width;
    f32 tex_x1 = (f32)(sprite.x + sprite.w) / sprite.atlas.width;
    f32 tex_y0 = 1.0f - (f32)(sprite.y + sprite.h) / sprite.atlas.height;
    f32 tex_y1 = 1.0f - (f32)sprite.y / sprite.atlas.height;

    immediate_begin(DRAW_TRIANGLE);
    set_shader(&TextureShader);
    set_color(color);
    set_texture(sprite.atlas.id);
    
    set_texture_coord(Vec2(tex_x0, tex_y1));
    set_vertex(Vec2(x0, y0));

    set_texture_coord(Vec2(tex_x0, tex_y0));
    set_vertex(Vec2(x0, y1));

    set_texture_coord(Vec2(tex_x1, tex_y0));
    set_vertex(Vec2(x1, y1));

    set_texture_coord(Vec2(tex_x1, tex_y0));
    set_vertex(Vec2(x1, y1));

    set_texture_coord(Vec2(tex_x1, tex_y1));
    set_vertex(Vec2(x1, y0));

    set_texture_coord(Vec2(tex_x0, tex_y1));
    set_vertex(Vec2(x0, y0));

    immediate_end();
}

void set_uv_matrix(const float *matrix3x3){
    T_ImmediateDrawContext *context = &ImmediateDrawContext;
    memcpy(context->uniforms.uv_matrix, matrix3x3, sizeof(context->uniforms.uv_matrix));
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

