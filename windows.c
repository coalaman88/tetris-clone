#define WIN32_LEAN_AND_MEAN
#define STB_IMAGE_IMPLEMENTATION

#include <windows.h>
#include <timeapi.h>
#include <assert.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>

#include "opengl_api.h"
#include "engine.h"
#include "game.h"

#include "wasapi.c"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

static b32 window_has_focus;

static void DrawBuffer(HWND window){
    HDC hdc = GetDC(window);
    SwapBuffers(hdc);
    ReleaseDC(window, hdc);
}

struct FileInfo{
    void *file;
    FILETIME last_write;
};

void *get_file_handle(void *file_info){
    struct FileInfo *info = file_info;
    return info->file;
}

// TODO use this instead of this method: https://learn.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
b32 update_file_info(void *info_handle){
    struct FileInfo *info = (struct FileInfo*)info_handle;
    FILETIME old_last_write = info->last_write;
    GetFileTime(info->file, NULL, NULL, &info->last_write);
    return (CompareFileTime(&old_last_write, &info->last_write) != 0);
}

void *create_file_info(void *file){
    struct FileInfo *info = os_memory_alloc(sizeof(struct FileInfo));
    info->file = file;
    GetFileTime(info->file, NULL, NULL, &info->last_write);
    return info;
}

static void *GetAnyGLFuncAddress(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if(p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)){
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void *)GetProcAddress(module, name);

        if(p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)){
            printf("%s\n", name);
            assert(false);
        }
    }
    return p;
}

static void LoadOpenGLFunctions(void){
    HWND dummy_window = CreateWindowExW(
        0, L"STATIC", L"Dummy Window",
        WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, NULL, NULL
    );
    assert(dummy_window);

    // Set Pixel Format for OpenGL
    HDC hdc = GetDC(dummy_window);
    PIXELFORMATDESCRIPTOR pixel_format = {0};
    pixel_format.nSize      = sizeof(pixel_format);
    pixel_format.nVersion   = 1;
    pixel_format.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;

    i32 result;
    i32 index = ChoosePixelFormat(hdc, &pixel_format);
    PIXELFORMATDESCRIPTOR suggested_pixel_format;
    DescribePixelFormat(hdc, index, sizeof(PIXELFORMATDESCRIPTOR), &suggested_pixel_format);
    result = SetPixelFormat(hdc, index, &suggested_pixel_format);
    assert(result != FALSE);

    HGLRC hrc = wglCreateContext(hdc);
    assert(hrc);
    result = wglMakeCurrent(hdc, hrc);
    assert(result);

    // Get OpenGL extesions (shader for example..)
    // wgl extension
    wglCreateContextAttribsARB = GetAnyGLFuncAddress("wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = GetAnyGLFuncAddress("wglChoosePixelFormatARB");

    // OPEN_GL_1_5
    glGenBuffers = GetAnyGLFuncAddress("glGenBuffers");

    // GL_VERSION_2_0
    glCreateShader    = GetAnyGLFuncAddress("glCreateShader");
    glShaderSource    = GetAnyGLFuncAddress("glShaderSource");
    glCompileShader = GetAnyGLFuncAddress("glCompileShader");
    glGetShaderiv     = GetAnyGLFuncAddress("glGetShaderiv");
    glCreateProgram = GetAnyGLFuncAddress("glCreateProgram");
    glAttachShader    = GetAnyGLFuncAddress("glAttachShader");
    glLinkProgram     = GetAnyGLFuncAddress("glLinkProgram");
    glGetProgramiv    = GetAnyGLFuncAddress("glGetProgramiv");
    glUseProgram        = GetAnyGLFuncAddress("glUseProgram");
    glDeleteProgram = GetAnyGLFuncAddress("glDeleteProgram");
    glGetProgramInfoLog = GetAnyGLFuncAddress("glGetProgramInfoLog");
    glGetShaderInfoLog = GetAnyGLFuncAddress("glGetShaderInfoLog");
    glEnableVertexAttribArray = GetAnyGLFuncAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = GetAnyGLFuncAddress("glVertexAttribPointer");
    glVertexAttribIPointer = GetAnyGLFuncAddress("glVertexAttribIPointer");
    glBindBuffer = GetAnyGLFuncAddress("glBindBuffer");
    glDisableVertexAttribArray = GetAnyGLFuncAddress("glDisableVertexAttribArray");
    glBufferData = GetAnyGLFuncAddress("glBufferData");
    glBufferSubData = GetAnyGLFuncAddress("glBufferSubData");
    glUniform3fv = GetAnyGLFuncAddress("glUniform3fv");
    glUniform2f = GetAnyGLFuncAddress("glUniform2f");
    glUniform1iv = GetAnyGLFuncAddress("glUniform1iv");
    glGetUniformLocation = GetAnyGLFuncAddress("glGetUniformLocation");
    glDeleteShader = GetAnyGLFuncAddress("glDeleteShader");
    glBindAttribLocation = GetAnyGLFuncAddress("glBindAttribLocation");
    glUniformMatrix4fv = GetAnyGLFuncAddress("glUniformMatrix4fv");
    glUniformMatrix3fv = GetAnyGLFuncAddress("glUniformMatrix3fv");
    glUniform1i = GetAnyGLFuncAddress("glUniform1i");
    glActiveTexture = GetAnyGLFuncAddress("glActiveTexture");
    glBlendEquationSeparate = GetAnyGLFuncAddress("glBlendEquationSeparate");
    glBlendFuncSeparate = GetAnyGLFuncAddress("glBlendFuncSeparate");
    glDrawArraysInstanced = GetAnyGLFuncAddress("glDrawArraysInstanced");
    glGetStringi = GetAnyGLFuncAddress("glGetStringi");
    glGetStringi = GetAnyGLFuncAddress("glGetStringi");
    //glBindTextureUnit = GetAnyGLFuncAddress("glBindTextureUnit");
    glGenVertexArrays = GetAnyGLFuncAddress("glGenVertexArrays");
    glBindVertexArray = GetAnyGLFuncAddress("glBindVertexArray");

    wglDeleteContext(hrc);
    ReleaseDC(dummy_window, hdc);
    DestroyWindow(dummy_window);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow){
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);

    //Create Console
    if(!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();

    // Make stdio stream functions work on new console on window apps!
    FILE *fpstdin = stdin, *fpstdout = stdout, *fpstderr = stderr;
    freopen_s(&fpstdin, "CONIN$", "r", stdin);
    freopen_s(&fpstdout, "CONOUT$", "w", stdout);
    freopen_s(&fpstderr, "CONOUT$", "w", stderr);

    LoadOpenGLFunctions();

    // Register the window class.

    WNDCLASS win_class = {0};
    win_class.lpfnWndProc   = WindowProc;
    win_class.hInstance     = hInstance;
    win_class.lpszClassName = "TetrisWindowClass";
    win_class.hCursor = LoadCursorA(NULL, IDC_ARROW);

    RegisterClass(&win_class);

    // Create the window.
    const i32 styles = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;

    RECT rect = {0, 0, WWIDTH, WHEIGHT};
    AdjustWindowRectEx(&rect, styles, false, 0);

    HWND window = CreateWindowEx(
        0,
        win_class.lpszClassName, // Window class
        "Tetris",  // Window text
        styles,    // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, // Size and position
        NULL,      // Parent window
        NULL,      // Menu
        hInstance, // Instance handle
        NULL       // Additional application data
    );

    HDC hdc = GetDC(window);

    int pixel_format_attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
    };

    i32 result;
    i32 pixel_format_wgl;
    u32 num_formats;
    wglChoosePixelFormatARB(hdc, pixel_format_attribs, 0, 1, &pixel_format_wgl, &num_formats);
    assert(num_formats);

    PIXELFORMATDESCRIPTOR suggested_pixel_format;
    DescribePixelFormat(hdc, pixel_format_wgl, sizeof(suggested_pixel_format), &suggested_pixel_format);
    result = SetPixelFormat(hdc, pixel_format_wgl, &suggested_pixel_format);
    assert(result);

    const i32 attrib_list[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0 // Only in version 3.2+, otherwise ignored!
    };
    // Add this if loading 3.2+ and using deprecated features: WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
    HGLRC hrc = wglCreateContextAttribsARB(hdc, NULL, attrib_list);
    assert(hrc);
    result = wglMakeCurrent(hdc, hrc);
    assert(result);

    ReleaseDC(window, hdc);
    ShowWindow(window, nCmdShow);

    // Sound
    WasapiAudio audio;
    init_wasapi(&audio);

    // Time things
    TIMECAPS time_caps;
    LARGE_INTEGER timer_start, timer_end, freq;
    timeGetDevCaps(&time_caps, sizeof(time_caps));
    QueryPerformanceFrequency(&freq);
    timeBeginPeriod(time_caps.wPeriodMin);

    EngineSetup();

    MSG msg = {0};
    timer_start.QuadPart = 0;
    while(GameRunning){
        LARGE_INTEGER old_start = timer_start;
        QueryPerformanceCounter(&timer_start);
        FramesPerSec = (u32)(1000 / (1000 * (timer_start.QuadPart - old_start.QuadPart) / freq.QuadPart));

        while(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)){
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if(!window_has_focus) EngineClearInput();

        EngineUpdate();
        update_sounds(&AudioState); // TODO move this to other thread?
        EngineProcessInput();
        DrawBuffer(window);

        QueryPerformanceCounter(&timer_end);
        i32 ms_elapsed = 1000 * (i32)((timer_end.QuadPart - timer_start.QuadPart) / freq.QuadPart);
        i32 time_left = TARGETFPS - ms_elapsed;

        if(time_left > 0){
            Sleep(time_left);
            ms_elapsed = TARGETFPS;
        }
        TimeElapsed = (f32)ms_elapsed / 1000.0f;
    }
    
    return 0;
}

static inline void KeyInput(WPARAM key_code, LPARAM lParam, b32 state){
    switch (key_code) {
        case VK_LEFT:
            Keyboard.left.state = state;
            break;
        case VK_RIGHT:
            Keyboard.right.state = state;
            break;
        case VK_UP:
            Keyboard.up.state = state;
            break;
        case VK_DOWN:
            Keyboard.down.state = state;
            break;
        case VK_SPACE:
            Keyboard.space_bar.state = state;
            break;
        case VK_BACK:
            Keyboard.back_space.state = state;
            break;
        case VK_SHIFT:
            Keyboard.shift.state = state;
            break;
        case VK_CONTROL:
            Keyboard.r_ctrl.state = state;
            break;
        case VK_RETURN:
            Keyboard.enter.state = state;
            break;
        case VK_OEM_PLUS:
            Keyboard.plus.state = state;
            break;
        case VK_OEM_MINUS:
            Keyboard.minus.state = state;
            break;
        case VK_MULTIPLY:
            Keyboard.asterisk.state = state;
            break;
        case VK_DIVIDE:
            Keyboard.r_slash.state = state;
            break;
        case VK_ESCAPE:
            Keyboard.esc.state = state;
            break;
        case VK_F4:
            if(lParam & 0x10000000){
                GameRunning = 0;
                PostQuitMessage(0);
            }
            break;
    }

    if(key_code >= 'A' && key_code <= 'Z'){
        i32 index = (i32)key_code - 'A';
        Key *k = &Keyboard.a + index;
        k->state = state;
        return;
    }

    if(key_code >= '0' && key_code <= '9'){
        i32 index = (i32)key_code - '0';
        Key *k = &Keyboard.n0 + index;
        k->state = state;
        return;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg){
        case WM_DESTROY:
            GameRunning = 0;
            PostQuitMessage(0);
            return 0;

        case WM_SETFOCUS:
            window_has_focus = true;
            return 0;

        case WM_KILLFOCUS:
            window_has_focus = false;
            return 0;

        case WM_KEYDOWN:
            KeyInput(wParam, lParam, true);
            return 0;

        case WM_KEYUP:
            KeyInput(wParam, lParam, false);
            return 0;

        case WM_LBUTTONDOWN:
            Mouse.left.state = true;
            return 0;

        case WM_LBUTTONUP:
            Mouse.left.state = false;
            return 0;

        case WM_RBUTTONDOWN:
            Mouse.right.state = true;
            return 0;

        case WM_RBUTTONUP:
            Mouse.right.state = false;
            return 0;

        case WM_MOUSEMOVE: {
            POINT mouse_pos;
            GetCursorPos(&mouse_pos);
            ScreenToClient(hwnd, &mouse_pos);
            Mouse.x = mouse_pos.x;
            Mouse.y = mouse_pos.y;
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void *os_open_file(const char *name){
    HANDLE file = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return file != INVALID_HANDLE_VALUE? file : NULL;
}

b32 os_close_file(void *file){
    return CloseHandle((HANDLE)file) != 0;
}

u8* os_read_whole_file_handle(void* file_handle, i32 *bytes){
    HANDLE file = (HANDLE)file_handle;
    *bytes = 0;

    u32 file_size = GetFileSize(file, NULL);
    u8 *buffer = os_memory_alloc(file_size);
    if(!buffer){
        return NULL;
    }

    DWORD read;
    b32 result = ReadFile(file, (void*)buffer, file_size, &read, NULL);
    if(!result || read != file_size){
        os_memory_free(buffer);
        return NULL;
    }

    SetFilePointer(file, 0, 0, FILE_BEGIN);
    *bytes = file_size;
    return buffer;
}

u8* os_read_whole_file(const char *name, i32 *bytes){
    *bytes = 0;
    HANDLE file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE)
        return NULL;

    u32 file_size = GetFileSize(file, NULL);
    u8 *buffer = os_memory_alloc(file_size);
    if(!buffer){
        CloseHandle(file);
        return NULL;
    }

    DWORD read;
    i32 result = ReadFile(file, (void*)buffer, file_size, &read, NULL);
    CloseHandle(file);

    if(!result || read != file_size)
        return NULL;

    *bytes = file_size;
    return buffer;
}

b32 os_write_to_file(void *data, i32 bytes, const char *name){
    DWORD written;
    HANDLE file = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL result = WriteFile(file, data, (DWORD)bytes, &written, NULL);
    CloseHandle(file);
    return (result != 0 && written == (DWORD)bytes);
}

b32 os_read_file(void *buffer, i32 bytes, const char *name){
    DWORD read;
    HANDLE file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL result = ReadFile(file, (void*)buffer, bytes, &read, NULL);
    CloseHandle(file);
    return (result != 0 && read == (DWORD)bytes);
}

char* os_font_path(char *buffer, u32 size, const char *append){
    buffer[0] = 0;
    ExpandEnvironmentStringsA("%windir%", buffer, size);
    assert(buffer[0]);
    i32 result = strcat_s(buffer, size, append) == 0;
    assert(result);
    return buffer;
}

Date os_get_local_time(void){
    SYSTEMTIME time;
    GetLocalTime(&time);
    Date date = {
        .day   = time.wDay,
        .month = time.wMonth,
        .year  = time.wYear
    };
    return date;
}

void *os_memory_alloc(size_t bytes){ // TODO reduce allocation calls?
    return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

b32 os_memory_free(void *address){
    return VirtualFree(address, 0, MEM_RELEASE) != 0;
}