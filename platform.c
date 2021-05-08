#include "platform.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_KEYS 256
#define BYTES_PER_PIXEL 4

typedef enum
{
    Action_up     = 0x57,
    Action_down   = 0x53,
    Action_left   = 0x41,
    Action_right  = 0x44
}Keycodes;

typedef struct
{
    bool down;
    bool pressed;
    bool released;
}DigitalButton;

// TODO(shvayko): struct for those guys 
global DigitalButton g_keys[MAX_KEYS];
global DigitalButton g_left_mouse;
global DigitalButton g_right_mouse;
global s32           g_mouse_delta_x;
global s32           g_mouse_delta_y;
global s32           g_mouse_pos_x;
global s32           g_mouse_pos_y;
global bool g_app_is_running;
global bool g_raw_input_registered;

typedef struct 
{
    BITMAPINFO bitmap_info;
    void *memory;
    s32 width;
    s32 height;
    s32 stride;
}Backbuffer;

global Backbuffer g_backbuffer;

typedef struct
{
    s32 width;
    s32 height;
}WindowDim;

typedef struct 
{
    void *memory;
    u32 file_size;
}FileContent;

FileContent
win_read_file(char *filename)
{
    FileContent result = {0};
    HANDLE file = CreateFileA(filename,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
    if(file != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_read;
        LARGE_INTEGER file_size_large;
        
        GetFileSizeEx(file,&file_size_large);
        result.file_size = file_size_large.QuadPart;
        
        result.memory =
            VirtualAlloc(0, result.file_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if(result.memory)
        {
            bool success = ReadFile(file,result.memory,result.file_size,&bytes_read,0);
            if(!(bytes_read == result.file_size))
            {
                VirtualFree(result.memory,0,MEM_RELEASE);
            }
        }
    }
    CloseHandle(file);
    return result;
}

void
win_write_file(char *filename, void *data, u32 bytes_to_write)
{
    HANDLE file = CreateFileA(filename, GENERIC_WRITE, 0,0,CREATE_ALWAYS,0,0);
    if(file != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written = 0;
        bool success = WriteFile(file,data,bytes_to_write,&bytes_written,0);
        if(success && (bytes_written == bytes_to_write))
        {
            
        }
    }
}

void
win_free_file(FileContent file_content)
{
    if(file_content.memory)
    {
        VirtualFree(file_content.memory,0,MEM_RELEASE);
    }
}

internal WindowDim
get_window_dim(HWND window)
{
    WindowDim result = {0};
    RECT rect;
    GetClientRect(window,&rect);
    
    result.width  = rect.right  - rect.left;
    result.height = rect.bottom - rect.top;
    
    return result;
}

LARGE_INTEGER
get_clock_value()
{
    LARGE_INTEGER result;
    
    QueryPerformanceCounter(&result);
    
    return result;
}

f32
get_clock_dif(LARGE_INTEGER last_time, LARGE_INTEGER first_time)
{
    f32 result = 0;
    
    result = (f32)(last_time.QuadPart - first_time.QuadPart);
    
    return result;
}



void
create_backbuffer(Backbuffer *backbuffer, s32 width, s32 height)
{
    if(backbuffer->memory)
    {
        VirtualFree(backbuffer->memory,0,MEM_RELEASE);
    }
    
    backbuffer->width  = width;
    backbuffer->height = height;
    
    backbuffer->bitmap_info.bmiHeader;
    
    backbuffer->bitmap_info.bmiHeader.biSize     = sizeof(backbuffer->bitmap_info.bmiHeader);
    backbuffer->bitmap_info.bmiHeader.biWidth    = backbuffer->width;
    backbuffer->bitmap_info.bmiHeader.biHeight   = -backbuffer->height;
    backbuffer->bitmap_info.bmiHeader.biPlanes   = 1;
    backbuffer->bitmap_info.bmiHeader.biBitCount = BYTES_PER_PIXEL * 8;
    backbuffer->bitmap_info.bmiHeader.biCompression = BI_RGB;
    
    s32 backbufferSize = backbuffer->width * backbuffer-> height * BYTES_PER_PIXEL;
    backbuffer->stride = backbuffer->width * BYTES_PER_PIXEL;  
    
    backbuffer->memory = VirtualAlloc(0, (size_t)backbufferSize,MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);  
}

void 
blit_to_screen(HDC device_context, Backbuffer *backbuffer,s32 window_width, s32 window_height)
{
    StretchDIBits(device_context,
                  0,0, window_width, window_height,
                  0,0, window_width, window_height,
                  backbuffer->memory,
                  &backbuffer->bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

void
pull_mouse_pos(HANDLE window)
{
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    ScreenToClient(window,&cursor_pos);
    
    g_mouse_pos_x = cursor_pos.x;
    g_mouse_pos_y = cursor_pos.y;
    
}

void
reset_digital_button(DigitalButton *key)
{
    key->pressed  = false;
    key->released = false;
}

void 
update_digital_button(DigitalButton *key,bool is_down)
{
    bool was_down = key->down;
    key->pressed =  !was_down && is_down;
    key->released = was_down && !is_down;
    key->down = is_down;
}

void 
pull_keyboard(void)
{
    BYTE kb_states[MAX_KEYS];
    if(!GetKeyboardState(kb_states))
    {
        return;
    }
    
    for(s32 key_index = 0;
        key_index < MAX_KEYS;
        key_index++)
    {
        BYTE key_state = kb_states[key_index];
        update_digital_button(g_keys + key_index, kb_states[key_index] >> 7);
    }
}

LRESULT CALLBACK 
window_proc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    LRESULT result = 0;
    
    switch(message)
    {
        case WM_INPUT:
        {
            UINT size;
            GetRawInputData((HRAWINPUT)lParam,RID_INPUT,0,&size,sizeof(RAWINPUTHEADER));
            BYTE *buf = malloc(size);
            if(GetRawInputData((HRAWINPUT)lParam,RID_INPUT, buf,&size,sizeof(RAWINPUTHEADER)) == size)
            {
                RAWINPUT *raw_input = (RAWINPUT*)buf;
                free(buf);
                if(raw_input->header.dwType == RIM_TYPEMOUSE)
                {
                    g_mouse_delta_x += raw_input->data.mouse.lLastX;
                    g_mouse_delta_y += raw_input->data.mouse.lLastY;
#if 0
                    char message_buffer[256];
                    snprintf(message_buffer, 256,"Mouse dX pos:[%d], mouse dY pos:[%d]\n",g_mouse_last_x, g_mouse_last_y);
                    OutputDebugStringA(message_buffer);
#endif
                    USHORT button_flags = raw_input->data.mouse.usButtonFlags;
                    bool left_mouse_button_down  = g_left_mouse.down;
                    bool right_mouse_button_down = g_right_mouse.down;
                    if(button_flags & RI_MOUSE_LEFT_BUTTON_DOWN)
                    {
                        left_mouse_button_down = true;
                    }
                    if(button_flags & RI_MOUSE_LEFT_BUTTON_UP)
                    {
                        left_mouse_button_down = false;
                    }
                    update_digital_button(&g_left_mouse,left_mouse_button_down);
                    
                    if(button_flags & RI_MOUSE_RIGHT_BUTTON_DOWN)
                    {
                        right_mouse_button_down = true;
                    }
                    if(button_flags & RI_MOUSE_RIGHT_BUTTON_UP)
                    {
                        right_mouse_button_down = false;
                    }
                    update_digital_button(&g_right_mouse,right_mouse_button_down);
                }
            }
        }break;
        case WM_CREATE:
        {
            OutputDebugStringA("WM_CREATE\n");
        }break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(window, &ps);
            WindowDim dim = get_window_dim(window);
            create_backbuffer(&g_backbuffer,dim.width,dim.height);
            EndPaint(window, &ps);
            OutputDebugStringA("WM_PAINT\n");
        }break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            OutputDebugStringA("WM_DESTROY\n");
            g_app_is_running = false;
        }break;
        case WM_QUIT:
        {
            g_app_is_running = false;
        }break;
        default:
        {
            return DefWindowProc(window,message,wParam,lParam);
        }break;
    };
    
    return result;
}

//NOTE(shvayko): if create_and_init_window fails, it returns INVALID_HANDLE_VALUE
HANDLE
create_and_init_window(s32 window_width,s32 window_height,char *window_name,HINSTANCE window_instance)
{
    HANDLE window = INVALID_HANDLE_VALUE;
    WNDCLASSA window_class = {0};
    window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = window_instance;
    window_class.lpszClassName = "MyWindowClassName";
    window_class.hCursor       = LoadCursorA(window_instance,IDC_ARROW);
    
    if(RegisterClassA(&window_class))
    {
        
        window = CreateWindowExA(0,window_class.lpszClassName,window_name,
                                 WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                                 window_width, window_height,0,0,window_instance,0);
    }
    return window;
}

void
register_raw_input_devices(HANDLE window)
{
    
    if(!g_raw_input_registered)
    {
        RAWINPUTDEVICE rid[2];
        
        rid[0].usUsagePage = 0x01;          
        rid[0].usUsage = 0x02;              
        rid[0].dwFlags = 0;    
        rid[0].hwndTarget = window;
        
        rid[1].usUsagePage = 0x01;          
        rid[1].usUsage = 0x06;              
        rid[1].dwFlags = 0;    
        rid[1].hwndTarget = window;
        
        if (RegisterRawInputDevices(rid, 2, sizeof(rid[0])) == FALSE)
        {
            ASSERT(!"Registration failed!")
        }
        
        g_raw_input_registered = true;
    }
    
}

int 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCmd)
{
    LARGE_INTEGER perf_freq;
    QueryPerformanceFrequency(&perf_freq);
    s64 perf_freq_value = perf_freq.QuadPart;
    
    char *window_name = "3D Rasterizer..";
    s32 init_window_width  = 800;
    s32 init_window_height = 600;
    g_app_is_running  = true;
    
    create_backbuffer(&g_backbuffer, init_window_width, init_window_height);
    
    HANDLE window = create_and_init_window(init_window_width,init_window_height,window_name, Instance);
    ShowWindow(window,ShowCmd);
    HDC device_context = GetDC(window);
    
    register_raw_input_devices(window);
    
    FileContent test_read_file = win_read_file(__FILE__);
    if(test_read_file.memory)
    {
        win_free_file(test_read_file);
    }
    
    char *text = malloc(256);
    memset(text,0, 256);
    text[0] = 'H';
    text[1] = 'E';
    text[2] = 'L';
    text[3] = 'L';
    text[4] = 'O';
    win_write_file("test.txt",text,256);
    free(text);
    
    if(window != INVALID_HANDLE_VALUE)
    {
        AppMemory memory = {0};
        memory.permanent_storage_size = MEGABYTES(256);
        memory.transient_storage_size = MEGABYTES(64);
        
        memory.permanent_storage = VirtualAlloc(0, memory.permanent_storage_size,MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        memory.transient_storage = VirtualAlloc(0, memory.transient_storage_size,MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if(!memory.permanent_storage && !memory.transient_storage)
        {
            ASSERT(!"Memory alloc for application has failed");
        }
        
        LARGE_INTEGER first_time = get_clock_value();
        s64 first_cpu_cycles = __rdtsc();
        while(g_app_is_running)
        {
            MSG message;
            g_mouse_delta_x = 0;
            g_mouse_delta_y = 0;
            reset_digital_button(&g_left_mouse);
            reset_digital_button(&g_right_mouse);
            
            pull_mouse_pos(window);
            
            while(PeekMessageA(&message,window,0,0,PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            
            pull_keyboard();
            
            if(g_keys[Action_up].pressed)
            {
                OutputDebugStringA("Button 'W' was pressed\n");
            }
            if(g_keys[Action_down].released)
            {
                OutputDebugStringA("Button 'S' is released!\n");
            }
            if(g_keys[Action_left].pressed)
            {
                OutputDebugStringA("Button 'A' is pressed!\n");
            }
            if(g_keys[Action_right].released)
            {
                OutputDebugStringA("Button 'D' is released!\n");
            }
            
            if(g_left_mouse.pressed)
            {
                OutputDebugStringA("Left mouse button is pressed!\n");
            }
            if(g_left_mouse.released)
            {
                OutputDebugStringA("Left mouse button is released!\n");
            }
            
            if(g_right_mouse.pressed)
            {
                OutputDebugStringA("Right mouse button is pressed!\n");
            }
            if(g_right_mouse.released)
            {
                OutputDebugStringA("Right mouse button is released!\n");
            }
            
            WindowDim dim = get_window_dim(window);
            
            AppBackbuffer app_backbuffer = {0};
            app_backbuffer.width  = g_backbuffer.width;
            app_backbuffer.height = g_backbuffer.height;
            app_backbuffer.stride = g_backbuffer.stride;
            app_backbuffer.memory = g_backbuffer.memory;
            //update_and_render(&app_backbuffer, &memory);
            
            blit_to_screen(device_context,&g_backbuffer,dim.width,dim.height);
            
            s64 last_cpu_cycles  = __rdtsc();
            s64 delta_cpu_cycles = last_cpu_cycles - first_cpu_cycles;
            
            LARGE_INTEGER last_time = get_clock_value();
            f32 clock_dif = get_clock_dif(last_time, first_time);
            f32 delta_time_sec = clock_dif / (f32)perf_freq_value;
            
            f32 time_ms = (f32)(delta_time_sec * 1000.0f);
            f32 FPS     = (f32)(1.0f / delta_time_sec);
            
#if 0
            char message_buffer[256];
            snprintf(message_buffer, 256,"Mouse X:[%d], mouse Y:[%d]\n",g_mouse_pos_x, g_mouse_pos_x);
            OutputDebugStringA(message_buffer);
#endif
#if 0
            char message_buffer[256];
            snprintf(message_buffer, 256,"FPS:[%f], MS:[%f], Seconds:[%f], cycles:[%lld]\n",FPS,time_ms,delta_time_sec, delta_cpu_cycles);
            OutputDebugStringA(message_buffer);
#endif
            first_time       = last_time;
            first_cpu_cycles = last_cpu_cycles;
        }
    }
    return 0;
}