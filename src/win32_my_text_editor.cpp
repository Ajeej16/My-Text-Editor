#include "windows.h"
#include <intrin.h>

// NOTE(ajeej): 
/*

Still need fixing:
- cursor movement up and down has some bugs
- need to add text wrapping
- need to add selection
- add continuous cursor movement with key arrows
- add mouse for cursor movement
- properly synchronize render threading
*/


/*
Instruction:
-Move Cursor around with key arrows
-The initial text shows the controls
-The test Document is in the test file
  -In the test file is a simple main console program made using the text editor
-Use Shift+O to move around to open files
-Since there isn't any text wrapping, most files will overflow that screen
-Can search for specific letters or words using SHIFT+F
-Save any changes by using SHIFT+S
-Create new file using Shift+N and typing in a new file name that isn't present
*/

#include "my_text_editor_utils.h"
#include "my_text_editor_math.h"

#include "my_text_editor_input.h"

global_var char character_map[CHAR_MAP_SIZE];
#include "my_text_editor_input.cpp"

#include "win32_my_text_editor.h"
#include "win32_my_text_editor_threading.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct bitmap_t
{
    v2 alignment;
    f32 dim_ratio;
    u32 width;
    u32 height;
    void *data;
};

struct win32_framebuffer_t
{
    union
    {
        struct
        {
            v2 alignment;
            f32 dim_ratio;
            u32 width;
            u32 height;
            void *data;
        };
        
        bitmap_t bitmap;
    };
    
    BITMAPINFO info;
};

global_var b32 global_running;
global_var win32_framebuffer_t global_framebuffer;

// NOTE(ajeej): Memory is managed through a memory arena
//              Contains a pointer the the memory block initialized
//              at the beginning of the program. Increases the used
//              index for every push made by the arena
internal void
init_memory_arena(memory_arena_t *arena, u32 size, void *memory)
{
    arena->base_ptr = (u8 *)memory;
    arena->size = size;
    arena->used = 0;
}

// NOTE(ajeej): Quick definitions for specific use cases
#define PushStruct(arena, type) (type *)_push_size(arena, sizeof(type))
#define PushArray(arena, type, count) (type *)_push_size(arena, sizeof(type)*count)
#define PushSize(arena, size) _push_size(arena, size)

internal void *
_push_size(memory_arena_t *arena, u32 size)
{
    // TODO(ajeej): Assert that size is not overflowed
    void *memory = arena->base_ptr + arena->used;
    arena->used += size;
    
    return memory;
}

inline LARGE_INTEGER
win32_get_counter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter;
}

inline f32
win32_seconds_elapsed(win32_timer *timer,
                      LARGE_INTEGER start,
                      LARGE_INTEGER end)
{
    f32 seconds_elapsed = ((f32)(end.QuadPart-start.QuadPart)) /
        (f32)timer->performance_freq;
    
    return seconds_elapsed;
}

internal b32
win32_timer_init(win32_timer *timer)
{
    b32 success = 0;
    
    LARGE_INTEGER frequency;
    if(QueryPerformanceFrequency(&frequency))
    {
        timer->performance_freq = frequency.QuadPart;
        success = 1;
    }
    
    timer->sleep_is_granular = (timeBeginPeriod(1) == TIMERR_NOERROR);
    
    timer->target_sec_per_frame = 1/60.0f;
    
    return success;
}

internal void
win32_timer_begin_frame(win32_timer *timer)
{
    timer->begin_frame = win32_get_counter();
}

// NOTE(ajeej): Completely broken
internal void
win32_timer_wait(win32_timer *timer)
{
    LARGE_INTEGER work_frame = win32_get_counter();
    f32 sec_elapsed = win32_seconds_elapsed(timer, timer->begin_frame,
                                            work_frame);
    f32 target_sec_per_frame = timer->target_sec_per_frame;
    
    if(sec_elapsed < target_sec_per_frame)
    {
        if(timer->sleep_is_granular)
        {
            DWORD ms_to_sleep = (DWORD)(1000.0f *
                                        (target_sec_per_frame - sec_elapsed));
            // TODO(ajeej): fix this mess
            if(ms_to_sleep > 0)
            {
                Sleep(ms_to_sleep);
            }
        }
        
        while(sec_elapsed < target_sec_per_frame)
        {
            sec_elapsed = win32_seconds_elapsed(timer,
                                                timer->begin_frame,
                                                win32_get_counter());
        }
    }
}

internal void
win32_timer_end_frame(win32_timer *timer)
{
    LARGE_INTEGER end_frame = win32_get_counter();
    f32 sec_per_frame = win32_seconds_elapsed(timer,
                                              timer->begin_frame,
                                              end_frame);
    timer->target_sec_per_frame = sec_per_frame;
    
    timer->begin_frame = end_frame;
}

internal void
win32_reset_framebuffer(win32_framebuffer_t* buffer,
                        u32 width, u32 height)
{
    if(buffer->data)
        VirtualFree(buffer->data, 0, MEM_RELEASE);
    
    buffer->width = width;
    buffer->height = height;
    
    buffer->alignment = {0.0f, 0.0f};
    buffer->dim_ratio = width/(f32)height;
    
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = -height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;
    
    buffer->data = VirtualAlloc(0, width*height*4, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

// NOTE(ajeej): Creates bitmap from loaded data
//              Used to load in fonts from the stb_truetype library
internal void
create_bitmap(bitmap_t *bitmap, memory_arena_t *arena, u32 width, u32 height,
              v2 alignment, void *src_data)
{
    bitmap->data = PushSize(arena, width*height*4);
    bitmap->width = width;
    bitmap->height = height;
    bitmap->dim_ratio = width/(f32)height;
    bitmap->alignment = alignment;
    
    
    u8 *src_row = (u8 *)src_data;
    u8 *dst_row = (u8 *)bitmap->data;
    for(i32 y = 0;
        y < height;
        y++)
    {
        u32 *dst = (u32 *)dst_row;
        for(i32 x = 0;
            x < width;
            x++)
        {
            u8 alpha = *src_row++;
            
            v4 texel = {
                (f32)(alpha),
                (f32)(alpha),
                (f32)(alpha),
                (f32)(alpha)
            };
            
            texel = srgb_to_light_linear(texel);
            
#if 1               
            // NOTE(ajeej): Pre multiplied alpha
            texel.rgb *= texel.a;
#endif
            
            texel = light_linear_to_srgb(texel);
            
            *dst++ = (((u32)(texel.a + 0.5f) << 24) |
                      ((u32)(texel.r + 0.5f) << 16) |
                      ((u32)(texel.g + 0.5f) <<  8) |
                      ((u32)(texel.b + 0.5f) <<  0));
        }
        
        dst_row += bitmap->width*4;
    }
    
}

// NOTE(ajeej): Create bitmap for the cursor
internal void
create_bitmap(bitmap_t *bitmap, memory_arena_t *arena, u32 width, u32 height,
              v2 alignment, v4 color)
{
    bitmap->data = PushSize(arena, width*height*4);
    bitmap->width = width;
    bitmap->height = height;
    bitmap->dim_ratio = width/(f32)height;
    bitmap->alignment = alignment;
    
    {
        u8 *dst_row = (u8 *)bitmap->data;
        for(i32 y = 0;
            y < height;
            y++)
        {
            u32 *dst = (u32 *)dst_row;
            for(i32 x = 0;
                x < width;
                x++)
            {
                
                v4 texel = color;
#if 1                
                texel.rgb *= texel.a;
#endif
                
                texel = light_linear_to_srgb(texel);
                
                *dst++ = (((u32)(texel.a + 0.5f) << 24) |
                          ((u32)(texel.r + 0.5f) << 16) |
                          ((u32)(texel.g + 0.5f) <<  8) |
                          ((u32)(texel.b + 0.5f) <<  0));
            }
            
            dst_row += bitmap->width*4;
        }
    }
}


internal LRESULT
win32_winproc(HWND window_handle, UINT message,
              WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    
    switch(message)
    {
        case WM_CHAR:
        {
            OutputDebugStringA("A");
        } break;
        
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT:
        {
            global_running = 0;
        } break;
        
        case WM_PAINT: 
        {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window_handle, &paint);
            
            RECT rect = paint.rcPaint;
            u32 width = rect.right - rect.left;
            u32 height = rect.bottom - rect.top;
            StretchDIBits(dc,
                          0, 0, width, height,
                          0, 0, global_framebuffer.width, global_framebuffer.height,
                          global_framebuffer.data,
                          &global_framebuffer.info,
                          DIB_RGB_COLORS, SRCCOPY);
            
            EndPaint(window_handle, &paint);
        } break;
        
        default:
        {
            result = DefWindowProc(window_handle, message,
                                   wparam, lparam);
        } break;
    }
    
    return result;
}

internal void
win32_process_messages(HWND window_handle, input_t *input)
{
    MSG message = {0};
    
    local_persist u64 prev_inputs[16];
    local_persist u64 input_count = 0;
    
    for(u32 inp = 0;
        inp < input_count;
        inp++)
    {
        u64 index = prev_inputs[inp];
        if(input->key_states[index] & Input_State_Pressed)
            input->key_states[index] &= ~Input_State_Pressed;
        else if (input->key_states[index] & Input_State_Released)
            input->key_states[index] &= ~Input_State_Released;
        else if (input->mouse_states[index] & Input_State_Pressed)
            input->mouse_states[index] &= ~Input_State_Pressed;
        else if (input->mouse_states[index] & Input_State_Released)
            input->mouse_states[index] &= ~Input_State_Released;
    }
    input_count = 0;
    
    memset(input->char_buffer, 0, input->char_index*sizeof(*input->char_buffer));
    input->char_index = 0;
    
    for(;;)
    {
        // TODO(ajeej): Assert that pkr_count doesn't go above 16
        BOOL got_message = FALSE;
        DWORD skip_messages[] = { 0x738, 0xFFFFFFFF, };
        DWORD last_message = 0;
        for(u32 skip_index = 0;
            skip_index< ArrayCount(skip_messages);
            skip_index++)
        {
            DWORD skip = skip_messages[skip_index];
            got_message = PeekMessage(&message, 0, last_message, skip-1, PM_REMOVE);
            if(got_message)
                break;
            
            last_message = skip+1;
        }
        
        if(!got_message)
        {
            break;
        }
        
        u64 wparam = message.wParam;
        u64 lparam = message.lParam;
        
        u8 modifiers = 0;
        if(GetKeyState(VK_CONTROL) & 0x0080)
        {
            modifiers |= Input_State_Ctrl;
        }
        if(GetKeyState(VK_MENU) & 0x0080)
        {
            modifiers |= Input_State_Alt;
        }
        if(GetKeyState(VK_SHIFT) & 0x0080)
        {
            modifiers |= Input_State_Shift;
        }
        
        input->caps_on = GetKeyState(VK_CAPITAL) & 1;
        
        
        switch(message.message)
        {
            
            case WM_LBUTTONDOWN:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Left];
                b32 was_down = *mouse_state & Input_State_Down;
                if(update_input_state(mouse_state, 1, was_down, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Left;
            } break;
            
            case WM_LBUTTONUP:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Left];
                if(update_input_state(mouse_state, 0, 0, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Left;
            } break;
            
            case WM_RBUTTONDOWN:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Right];
                b32 was_down = *mouse_state & Input_State_Down;
                if(update_input_state(mouse_state, 1, was_down, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Right;
            } break;
            
            case WM_RBUTTONUP:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Right];
                if(update_input_state(mouse_state, 0, 0, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Right;
            } break;
            
            case WM_MBUTTONDOWN:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Middle];
                b32 was_down = *mouse_state & Input_State_Down;
                if(update_input_state(mouse_state, 1, was_down, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Middle;
            } break;
            
            case WM_MBUTTONUP:
            {
                u8 *mouse_state = &input->mouse_states[Mouse_ID_Middle];
                if(update_input_state(mouse_state, 0, 0, modifiers))
                    prev_inputs[input_count++] = Mouse_ID_Middle;
            } break;
            
            case WM_XBUTTONDOWN:
            {
                u64 btn = 0;
                if(wparam & (1<<6))
                    btn= Mouse_ID_Btn1;
                else if(wparam & (1<<6))
                    btn= Mouse_ID_Btn2;
                
                u8 *mouse_state = &input->mouse_states[btn];
                b32 was_down = *mouse_state & Input_State_Down;
                if(update_input_state(mouse_state, 1, was_down, modifiers))
                    prev_inputs[input_count++] = btn;
            } break;
            
            case WM_XBUTTONUP:
            {
                u64 btn = 0;
                if(wparam & (1<<6))
                    btn= Mouse_ID_Btn1;
                else if(wparam & (1<<6))
                    btn= Mouse_ID_Btn2;
                
                u8 *mouse_state = &input->mouse_states[btn];
                if(update_input_state(mouse_state, 0, 0, modifiers))
                    prev_inputs[input_count++] = btn;
            } break;
            
            // TODO(ajeej): Implement Doubl Click
            
            case WM_SYSKEYUP:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_KEYDOWN:
            {
                u64 key_code = wparam;
                b8 is_down = ((lparam & (1<< 31)) == 0);
                b8 was_down = ((lparam & (1<< 30)) > 0);
                u64 key = 0;
                u8 *key_state = 0;
                b32 mod_char = 0;
                input->caps_on = (input->caps_on > 0)*((modifiers & (Input_State_Shift)) == 0) +
                    (input->caps_on == 0)*((modifiers & (Input_State_Shift)) > 0);
                mod_char = input->caps_on;
                
                if(key_code >= 'A' && key_code <= 'Z')
                    key = Key_ID_A + (key_code - 'A');
                else if (key_code >= '0' && key_code <= '9')
                    key = Key_ID_0 + (key_code - '0');
                else if (key_code == VK_ESCAPE)
                    key = Key_ID_Esc;
                else if (key_code >= VK_F1 && key_code <= VK_F12)
                    key = Key_ID_F1 + (key_code - VK_F1);
                else if (key_code == VK_OEM_3)
                    key = Key_ID_GraveAccent;
                else if (key_code == VK_OEM_MINUS)
                    key = Key_ID_Minus;
                else if (key_code == VK_OEM_PLUS)
                    key = Key_ID_Equal;
                else if (key_code == VK_BACK)
                {
                    key = Key_ID_Backspace;
                    mod_char = 0;
                }
                else if (key_code == VK_TAB)
                    key = Key_ID_Tab;
                else if (key_code == VK_SPACE)
                {
                    key = Key_ID_Space;
                    mod_char = 0;
                }
                else if (key_code == VK_RETURN)
                {
                    key = Key_ID_Enter;
                    mod_char = 0;
                }
                else if (key_code == VK_UP)
                    key = Key_ID_Up;
                else if (key_code == VK_LEFT)
                    key = Key_ID_Left;
                else if (key_code == VK_DOWN)
                    key = Key_ID_Down;
                else if (key_code == VK_RIGHT)
                    key = Key_ID_Right;
                else if (key_code == VK_DELETE)
                {
                    key = Key_ID_Delete;
                    mod_char = 0;
                }
                else if (key_code == VK_PRIOR)
                    key = Key_ID_PageUp;
                else if (key_code == VK_NEXT)
                    key = Key_ID_PageDown;
                else if (key_code == VK_HOME)
                    key = Key_ID_Home;
                else if (key_code == VK_END)
                    key = Key_ID_End;
                else if (key_code == VK_OEM_1)
                    key = Key_ID_Colon;
                else if (key_code == VK_OEM_2)
                    key = Key_ID_ForwardSlash;
                else if (key_code == VK_OEM_5)
                    key = Key_ID_BackSlash;
                else if (key_code == VK_OEM_PERIOD)
                    key = Key_ID_Period;
                else if (key_code == VK_OEM_COMMA)
                    key = Key_ID_Comma;
                else if (key_code == VK_OEM_7)
                    key = Key_ID_Quote;
                else if (key_code == VK_OEM_4)
                    key = Key_ID_LeftBracket;
                else if (key_code == VK_OEM_6)
                    key = Key_ID_RightBracket;
                else if (key_code == VK_CONTROL)
                {
                    key = Key_ID_Ctrl;
                    modifiers &= ~Input_State_Ctrl;
                }
                else if (key_code == VK_SHIFT)
                {
                    key = Key_ID_Shift;
                    modifiers &= ~Input_State_Shift;
                }
                else if (key_code == VK_MENU)
                {
                    key = Key_ID_Alt;
                    modifiers &= ~Input_State_Alt;
                }
                
                if(is_down && ((i32)key-26) >= 0 && (modifiers & Input_State_Ctrl) == 0)
                {
                    input->char_buffer[input->char_index++] = get_character(key, mod_char);
                }
                
                if(update_input_state(&input->key_states[key],
                                      is_down, was_down, modifiers))
                    prev_inputs[input_count++] = key;
                
                
            } break;
            
            
            default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            } break;
        }
    }
}

struct win32_thread_info_t
{
    u32 thread_index;
    work_queue_t *queue;
};

DWORD WINAPI
thread_proc(LPVOID lp_param)
{
    win32_thread_info_t *thread_info = (win32_thread_info_t *)lp_param;
    
    for(;;)
    {
        if(win32_execute_next_queue_entry(thread_info->queue))
        {
            WaitForSingleObjectEx(thread_info->queue->semaphore, INFINITY, FALSE);
        }
    }
}


#define TEXT_BITMAP_COUNT 144
// NOTE(ajeej): Depricated structure
struct text_bitmap_t
{
    bitmap_t bitmaps[TEXT_BITMAP_COUNT];
    f32 widths[TEXT_BITMAP_COUNT];
    f32 heights[TEXT_BITMAP_COUNT];
};

// NOTE(ajeej): Contains font information stored after loading
struct font_t
{
    bitmap_t bitmaps[TEXT_BITMAP_COUNT];
    
    stbtt_fontinfo info;
    f32 scale;
    f32 ascent;
    f32 descent;
    f32 line_gap;
};

// NOTE(ajeej): type of operations
enum text_edit_state_type_t
{
    text_edit_state_type_edit,
    text_edit_state_type_search,
    text_edit_state_type_open_file,
    text_edit_state_type_new_file,
};

// NOTE(ajeej): Contains cursor information
struct text_edit_state_t
{
    i32 cursor;
    
    v2 cursor_pos;
    v2 cursor_dim;
};

struct text_row_info_t
{
    // TODO(ajeej): add positional information
    //              or just leave that for rendering
    u32 char_count;
};

struct text_search_info_t
{
    i32 row_start;
    i32 row_length;
    i32 prev_row_start;
};

struct text_box_t
{
    v2 pos;
    v2 dim;
    v4 color;
    
    text_edit_state_t state;
    
    u32 str_max_size;
    u32 str_start_index;
    char *str;
    f32 font_size;
    v2 str_offset;
    
    font_t *font;
};

#define MAX_FILES 32
// NOTE(ajeej): Application structure
struct text_app_t
{
    text_box_t text_box;
    text_box_t util_box;
    
    text_edit_state_type_t state_type;
    
    char *cur_dir;
    char dir_files[MAX_FILES][MAX_PATH];
    
    char *cur_file;
    
    b32 show_text;
    b32 show_utils;
};


#include "my_text_editor_string.h"
#include "my_text_editor_render_command.h"
#include "my_text_editor_render_command.cpp"

// NOTE(ajeej): Check if the path is pointing to a directory
inline b32
is_dir(char name[MAX_PATH])
{
    b32 is_dir = 0;
    
    u32 size = strlen(name);
    
    if(size)
    {
        if(name[size-1] == '\\')
            is_dir = 1;
    }
    
    return is_dir;
}

// NOTE(ajeej): Move back to the previous directory in the path
inline void
move_back_dir(char path[MAX_PATH])
{
    i32 size = strlen(path);
    i32 i = 0;
    
    for(i = size-2;
        i > 0;
        i--)
    {
        if(path[i] == '\\')
            break;
    }
    
    if(i > 0)
    {
        clear_string(path, i+1);
    }
}

// NOTE(ajeej): Get the file name from path
inline void
get_file_from_path(char path[MAX_PATH], char file[MAX_PATH])
{
    i32 size = strlen(path);
    i32 i = 0;
    i32 j = 0;
    
    for(i = size-2;
        i > 0;
        i--)
    {
        if(path[i] == '\\')
            break;
    }
    
    for(i += 1;
        i < size;
        i++, j++)
    {
        file[j] = path[i];
    }
}


// NOTE(ajeej): Get an array of files from the current directory
internal u32
get_files_from_dir(char files[MAX_FILES][MAX_PATH], char dir[MAX_PATH])
{
    char file_dir[MAX_PATH];
    copy_string(file_dir, dir);
    concatenate_strings(file_dir, "*");
    u32 file_count = 0;
    
    WIN32_FIND_DATA file_data;
    HANDLE file_handle;
    
    file_handle = FindFirstFile(file_dir, &file_data);
    
    if(INVALID_HANDLE_VALUE == file_handle)
        return file_count;
    
    for(u32 i = 0;
        i < MAX_FILES;
        i++)
    {
        if(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            char temp[MAX_PATH];
            copy_string(temp, file_data.cFileName);
            concatenate_strings(temp, "\\");
            copy_string(files[i], temp);
        }
        else
        {
            copy_string(files[i], file_data.cFileName);
        }
        
        file_count++;
        
        if(FindNextFile(file_handle, &file_data) == 0)
            break;
    }
    
    return file_count;
}

// TODO(ajeej): Dynamic size loaded string
// NOTE(ajeej): Load file from path
internal b32
load_file(char path[MAX_PATH], char data[STR_MAX_SIZE])
{
    HANDLE file = {};
    b32 success = 1;
    
    DWORD desired_access = GENERIC_READ|GENERIC_WRITE;
    
    if((file = CreateFile(path, desired_access, FILE_SHARE_READ,
                          0, OPEN_EXISTING,
                          0, 0)) != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_read = 0;
        if(ReadFile(file, data, (STR_MAX_SIZE-1)*sizeof(char),
                    &bytes_read, 0))
        {
            data[bytes_read] = 0;
        }
        else
        {
            success = 0;
        }
        
    }
    else
    {
        success = 0;
    }
    CloseHandle(file);
    
    return success;
}

// NOTE(ajeej): Write to a file at a path
internal b32
write_file(char path[MAX_PATH], char data[STR_MAX_SIZE], u32 size)
{
    HANDLE file = {};
    b32 success = 1;
    
    DWORD desired_access = GENERIC_READ|GENERIC_WRITE;
    
    if((file = CreateFile(path, desired_access, FILE_SHARE_READ,
                          0, OPEN_EXISTING,
                          0, 0)) != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_read = 0;
        if(!WriteFile(file, data, size,
                      &bytes_read, 0))
        {
            success = 0;
        }
    }
    else
    {
        success = 0;
    }
    CloseHandle(file);
    
    return success;
}

internal void
get_text_row_info(text_row_info_t *info, char *str, i32 pos)
{
    u32 size = string_length(str);
    u32 char_count = 0;
    b32 flush_count = 0;
    
    for(u32 i = (u32)pos;
        i < size;
        i++)
    {
        char_count++;
        
        if(str[i] == '\n')
            break;
    }
    
    info->char_count = char_count;
}

internal void
get_text_search_info(text_search_info_t *info, char *str, i32 pos)
{
    text_row_info_t row_info;
    i32 iter = 0;
    i32 prev_start = 0;
    
    for(;;)
    {
        get_text_row_info(&row_info, str, iter);
        
        if(pos < iter+row_info.char_count)
            break;
        
        prev_start = iter;
        iter += row_info.char_count;
    }
    
    info->row_start = iter;
    info->row_length = row_info.char_count;
    info->prev_row_start = prev_start;
}

internal void
clamp_cursor(char *str, text_edit_state_t *state)
{
    u32 n = string_length(str)-1;
    if(state->cursor > n) state->cursor = n;
}

internal void
render_text_box(render_command_buffer_t *render_buffer,
                text_box_t *tb)
{
    // TODO(ajeej): Fix threaded rendering so
    //              it can be more synch
    text_edit_state_t *state = &tb->state;
    
    push_rect(render_buffer, tb->pos, tb->dim, tb->color, {0, 0},
              {8, 8});
    
    push_string(render_buffer, tb->str, tb->font, state, tb->font_size, tb->str_offset + tb->pos);
    
}

internal void
render_text_box(render_command_buffer_t *render_buffer,
                v2 pos, v2 dim, v4 color,
                char *str, font_t *font, v2 str_offset, f32 font_size,
                text_edit_state_t *state)
{
    
    // TODO(ajeej): Add push string that does not take text edit state
    push_rect(render_buffer, pos, dim, color, {0, 0}, {8, 8});
    
    push_string(render_buffer, str, font, state, font_size, str_offset + pos);
}


// NOTE(ajeej): Used for all text editing input processing
internal void
process_edit_input(render_command_buffer_t *render_buffer, input_t *input, text_app_t *app)
{
    text_edit_state_t *state;
    char *str;
    
    switch(app->state_type)
    {
        case text_edit_state_type_edit:
        {
            state = &app->text_box.state;
            str = app->text_box.str;
            
            // NOTE(ajeej): Move cursor
            if(IsKeyPressed(input, Key_ID_Right))
            {
                ++state->cursor;
                clamp_cursor(str, state);
            }
            
            if(IsKeyPressed(input, Key_ID_Left))
            {
                --state->cursor;
                clamp_cursor(str, state);
            }
            
            if(IsKeyPressed(input, Key_ID_Down))
            {
                text_search_info_t search_info;
                text_row_info_t row_info;
                
                get_text_search_info(&search_info, str, state->cursor);
                
                i32 target_x = state->cursor - search_info.row_start;
                
                if(search_info.row_length-1)
                {
                    i32 next_row_start = search_info.row_start + search_info.row_length;
                    state->cursor = next_row_start;
                    
                    get_text_row_info(&row_info, str, state->cursor);
                    
                    for(i32 i = 0;
                        i < row_info.char_count &&
                        i < target_x;
                        i++)
                    {
                        ++state->cursor;
                    }
                    
                    clamp_cursor(str, state);
                }
            }
            
            if(IsKeyPressed(input, Key_ID_Up))
            {
                text_search_info_t search_info;
                text_row_info_t row_info;
                
                get_text_search_info(&search_info, str, state->cursor);
                
                i32 target_x = state->cursor - search_info.row_start;
                
                if(search_info.prev_row_start != search_info.row_start)
                {
                    state->cursor = search_info.prev_row_start;
                    
                    get_text_row_info(&row_info, str, state->cursor);
                    
                    for(i32 i = 0;
                        i < row_info.char_count &&
                        i < target_x;
                        i++)
                    {
                        ++state->cursor;
                    }
                    
                    clamp_cursor(str, state);
                }
            }
            
            // NOTE(ajeej): Save
            if(IsKeyAndModPressed(input, Key_ID_S, Input_State_Ctrl))
            {
                DeleteFile(app->cur_file);
                HANDLE file_handle = CreateFile(app->cur_file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0,
                                                CREATE_NEW, 0, 0);
                
                CloseHandle(file_handle);
                write_file(app->cur_file, app->text_box.str, strlen(app->text_box.str));
            }
            
            // NOTE(ajeej): Search
            if(IsKeyAndModPressed(input, Key_ID_F, Input_State_Ctrl))
            {
                char start_text[] = "Search: ";
                copy_string(app->util_box.str, start_text);
                app->util_box.state.cursor += app->util_box.str_start_index =  strlen(start_text);
                
                app->state_type = text_edit_state_type_search;
            }
            
            // NOTE(ajeej): Open
            if(IsKeyAndModPressed(input, Key_ID_O, Input_State_Ctrl))
            {
                // TODO(ajeej): create function for this
                char start_text[MAX_PATH + ArrayCount("Open: ")] = "Open: ";
                concatenate_strings(start_text, app->cur_dir);
                copy_string(app->util_box.str, start_text);
                app->util_box.str_start_index =  strlen("Open: ");
                app->util_box.state.cursor += strlen(start_text);
                
                app->state_type = text_edit_state_type_open_file;
            }
            
            // NOTE(ajeej): New
            if(IsKeyAndModPressed(input, Key_ID_N, Input_State_Ctrl))
            {
                // TODO(ajeej): create function for this
                char start_text[MAX_PATH + ArrayCount("New: ")] = "New: ";
                concatenate_strings(start_text, app->cur_dir);
                copy_string(app->util_box.str, start_text);
                app->util_box.str_start_index =  strlen("New: ");
                app->util_box.state.cursor += strlen(start_text);
                
                app->state_type = text_edit_state_type_new_file;
            }
            
        } break;
        
        case text_edit_state_type_search:
        {
            local_persist i32 search_index = 0;
            
            state = &app->util_box.state;
            str = app->util_box.str;
            
            char temp[STR_MAX_SIZE] = "";
            
            copy_string(temp, str);
            
            char *temp_ptr = temp + app->util_box.str_start_index;
            
            app->show_utils = 1;
            
            string_search_info_t info = search_substring(app->text_box.str,
                                                         temp_ptr);
            
            if(strlen(str) > 0)
            {
                app->text_box.state.cursor = info.pos[search_index];
            }
            
            if(IsKeyPressed(input, Key_ID_Up))
            {
                search_index--;
                if(search_index < 0) search_index = 0;
            }
            
            if(IsKeyPressed(input, Key_ID_Down))
            {
                search_index++;
                if(search_index >= info.size) search_index = 0;
            }
            
            if(IsKeyPressed(input, Key_ID_Enter) ||
               IsKeyPressed(input, Key_ID_Esc))
            {
                clear_string(str);
                state->cursor = 0;
                app->show_utils = 0;
                
                app->state_type = text_edit_state_type_edit;
            }
        } break;
        
        case text_edit_state_type_open_file:
        case text_edit_state_type_new_file:
        {
            // NOTE(ajeej): Used to keep track of the file/dir in selection
            local_persist i32 selection_index = 0;
            local_persist u32 start_index = 0;
            v4 select_color = {1, 0, 0, 1};
            u32 visible_selection = 5;
            
            
            state = &app->util_box.state;
            str = app->util_box.str;
            app->show_utils = 1;
            text_box_t tb = app->util_box;
            
            //char temp[STR_MAX_SIZE] = "";
            //copy_string(temp, str);
            
            char *temp_ptr = str + app->util_box.str_start_index;
            
            u32 total_files = get_files_from_dir(app->dir_files, temp_ptr);
            
            //render_text_box(render_buffer, pos, dim, {1, 0, 1, 1}, &app->cur_dir, tb.font, {0, 0}, &tb.state);
            
            lower_clamp(&selection_index, 0);
            upper_clamp(&selection_index, total_files-1);
            
            // NOTE(ajeej): Open or Create file
            if(IsKeyPressed(input, Key_ID_Enter))
            {
                
                if(is_dir(app->dir_files[selection_index]))
                {
                    // NOTE(ajeej): Open selected directory
                    if(!is_dir(temp_ptr))
                        move_back_dir(temp_ptr);
                    
                    concatenate_strings(temp_ptr, app->dir_files[selection_index]);
                    
                    clear_string(app->cur_dir);
                    copy_string(app->cur_dir, temp_ptr);
                    
                    state->cursor = strlen(str);
                    selection_index = 0;
                }
                else
                {
                    char file_path[MAX_PATH];
                    
                    if(app->state_type == text_edit_state_type_new_file &&
                       total_files == 0)
                    {
                        // NOTE(ajeej): Create a new file
                        copy_string(file_path, temp_ptr);
                        HANDLE file_handle = CreateFile(file_path, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0,
                                                        CREATE_NEW, 0, 0);
                        
                        CloseHandle(file_handle);
                    }
                    else
                    {
                        // NOTE(ajeej): Open selected file
                        copy_string(file_path, temp_ptr);
                        concatenate_strings(file_path, app->dir_files[selection_index]);
                    }
                    
                    // NOTE(ajeej): Load in file information and
                    //              attach it to text box
                    char data[MAX_PATH];
                    if(load_file(file_path, data))
                    {
                        clear_string(app->text_box.str);
                        copy_string(app->text_box.str, data);
                    }
                    
                    clear_string(app->cur_dir);
                    copy_string(app->cur_dir, temp_ptr);
                    clear_string(app->cur_file);
                    copy_string(app->cur_file, file_path);
                    
                    clear_string(str);
                    state->cursor = 0;
                    app->show_utils = 0;
                    
                    app->state_type = text_edit_state_type_edit;
                    app->text_box.state.cursor = 0;
                    return;
                }
            }
            
            if(IsKeyPressed(input, Key_ID_Down))
            {
                if(++selection_index >= total_files) selection_index = 0;
            }
            
            if(IsKeyPressed(input, Key_ID_Up))
            {
                if(--selection_index < 0) selection_index = total_files - 1;
            }
            
            if(IsKeyPressed(input, Key_ID_Esc))
            {
                clear_string(str);
                state->cursor = 0;
                app->show_utils = 0;
                
                app->state_type = text_edit_state_type_edit;
            }
            
            if(selection_index > visible_selection + start_index)
            {
                if(visible_selection + start_index < total_files - 1)
                    start_index++;
                else
                    start_index = selection_index;
            }
            else if(selection_index < start_index)
            {
                if(start_index < total_files - 1)
                    start_index--;
                else
                    start_index = selection_index;
            }
            
            v2 pos = {tb.pos.x, tb.pos.y+tb.dim.y};
            v2 dim = {tb.dim.x, 1};
            
            for(u32 i = start_index;
                i < total_files && i < visible_selection+start_index+1;
                i++)
            {
                
                v4 color = {1, 0, 1, 1};
                
                if(i == selection_index)
                    color = select_color;
                
                render_text_box(render_buffer, pos, dim, color,
                                app->dir_files[i], tb.font, {0, 0}, 0.6,
                                &tb.state);
                
                pos.y += dim.y; 
            }
            
        } break;
    }
    
    
    // NOTE(ajeej): insert characters from the character buffer
    for(u32 i = 0;
        i < input->char_index;
        i++)
    {
        char c = input->char_buffer[i];
        
        switch(c)
        {
            default:
            {
                if(app->state_type != text_edit_state_type_edit && c == '\n')
                    continue;
                
                insert_char(str, c, state->cursor);
                ++state->cursor;
                clamp_cursor(str, state);
            } break;
            
            // NOTE(ajeej): Backspace
            case '\b':
            {
                if(state->cursor > 0)
                {
                    delete_char(str, --state->cursor);
                }
            } break;
            
            // NOTE(ajeej): Delete
            case '\a':
            {
                delete_char(str, state->cursor);
                clamp_cursor(str, state);
            } break;
            
        };
    }
}

internal void
render_cursor(render_command_buffer_t *render_buffer, memory_arena_t *arena, text_edit_state_t *state)
{
    bitmap_t cursor_bit = {};
    create_bitmap(&cursor_bit, arena, (state->cursor_dim*38).x, (state->cursor_dim*38).y, {0, 0}, V4(1, 1, 1, 1));
    
    push_bitmap(render_buffer, &cursor_bit,
                state->cursor_dim.height, state->cursor_pos, {1, 0},
                {0, 1});
}

int WinMain(HINSTANCE instance, HINSTANCE pinstance,
            LPSTR cmd_line, i32 show_cmd)
{
    init_character_map(character_map);
    
    // NOTE(ajeej): Initialize threads
    //              depends on threads in computer
    win32_thread_info_t thread_info[4];
    
    work_queue_t queue = {};
    
    u32 thread_count = ArrayCount(thread_info);
    queue.semaphore = CreateSemaphoreEx(0,
                                        0,
                                        thread_count,
                                        0, 0, SEMAPHORE_ALL_ACCESS);
    
    // NOTE(ajeej): Create each thread
    for(u32 thread_index = 0;
        thread_index < thread_count;
        thread_index++)
    {
        win32_thread_info_t *info = thread_info + thread_index;
        
        info->queue = &queue;
        info->thread_index = thread_index;
        
        DWORD threadID;
        HANDLE thread = CreateThread(0, 0,
                                     thread_proc,
                                     info, 0,
                                     &threadID);
        CloseHandle(thread);
    }
    
    
    // NOTE(ajeej): Create window class
    WNDCLASS window_class = {};
    {
        window_class.style = CS_HREDRAW|CS_VREDRAW;
        window_class.lpfnWndProc = win32_winproc;
        window_class.hInstance = instance;
        window_class.lpszClassName = "TextEditorWindowClass";
        window_class.hCursor = LoadCursor(0, IDC_ARROW);
    }
    
    if(!RegisterClass(&window_class))
    {
        goto quit;
    }
    
    // NOTE(ajeej): Create window
    HWND window_handle = CreateWindow(window_class.lpszClassName,
                                      "My Text Editor",
                                      WS_OVERLAPPEDWINDOW,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      720, 480, 0 , 0, instance, 0);
    
    if(!window_handle)
    {
        goto quit;
    }
    
    win32_timer timer = {};
    win32_timer_init(&timer);
    
    // NOTE(ajeej): Initalize application memory
    //              One Gigabyte
    memory_t app_memory = {};
    app_memory.storage_size = Gigabyte(1);
    app_memory.storage = VirtualAlloc(0, app_memory.storage_size,
                                      MEM_RESERVE|MEM_COMMIT,
                                      PAGE_READWRITE);
    
    // NOTE(ajeej): Arena for rendering
    memory_arena_t render_arena = {};
    init_memory_arena(&render_arena, Megabyte(100), app_memory.storage);
    
    // NOTE(ajeej): Arena that is cleared every frame
    memory_arena_t trans_arena = {};
    init_memory_arena(&trans_arena, Megabyte(100), (void *)((u8 *)app_memory.storage+render_arena.size));
    
    // NOTE(ajeej): Create render_buffer
    render_command_buffer_t *render_buffer = init_render_command_buffer(&render_arena, Megabyte(10));
    
    // NOTE(ajeej): Create framebuffer
    global_framebuffer = {};
    win32_reset_framebuffer(&global_framebuffer, 720, 480);
    
    input_t input = {};
    
    ShowWindow(window_handle, show_cmd);
    UpdateWindow(window_handle);
    
    // NOTE(ajeej): Read in font information from stb_truetype
    u64 ttf_size;
    unsigned char *ttf_buffer;
    bitmap_t t_bitmap[144];
    
    FILE *ttf_file = fopen("W:/my_text_editor/data/courier.ttf", "rb");
    fseek(ttf_file, 0, SEEK_END);
    ttf_size = ftell(ttf_file);
    fseek(ttf_file, 0, SEEK_SET);
    
    ttf_buffer = (unsigned char *)malloc(ttf_size);
    
    fread(ttf_buffer, ttf_size, 1, ttf_file);
    fclose(ttf_file);
    
    
    stbtt_fontinfo font_info;
    stbtt_InitFont(&font_info, ttf_buffer, 0);
    
    char text[STR_MAX_SIZE] = "Key Bindings:\nShift + F = SEARCH\nShift + O = OPEN FILE\nShift + N = NEW FILE\nShift + S = SAVE\nEscape = END COMMAND";
    char search[STR_MAX_SIZE] = "";
    
    text_edit_state_t state[2]= {};
    
    // NOTE(ajeej): Initialize text boxes
    text_box_t tb = {};
    tb.pos = {1.5, 3};
    tb.dim = {16, 8};
    tb.color = {1, 1, 1, 0.2};
    tb.str = text;
    tb.str_max_size = STR_MAX_SIZE;
    tb.font_size = 0.5f;
    tb.str_offset = {0.2, 0};
    tb.state = state[0];
    
    text_box_t sb = {};
    sb.pos = {0, 0};//{1.5, 0.5};
    sb.dim = {16, 1.5};
    sb.color = {1, 1, 1, 0.2};
    sb.str = search;
    sb.str_max_size = STR_MAX_SIZE;
    sb.font_size = 0.5f;
    sb.str_offset = {0.2, 0.5};
    sb.state = state[1];
    
    font_t font = {};
    font.info = font_info;
    font.scale = stbtt_ScaleForPixelHeight(&font_info, 50);
    
    i32 ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent,
                          &descent, &line_gap);
    
    font.ascent = round_f32_to_i32(ascent * font.scale);
    font.descent = round_f32_to_i32(descent * font.scale);
    
    // NOTE(ajeej): Get all character bitmaps
    for(u32 i = 0;
        i <= ('~'-33);
        i++)
    {
        
        i32 width, height, xoffset, yoffset;
        u8 *monobitmap = (u8 *)stbtt_GetCodepointBitmap(&font_info, 0, font.scale,
                                                        i+33, &width, &height, &xoffset, &yoffset);
        
        create_bitmap(&t_bitmap[i], &render_arena, (u32)width, (u32)height, {0, 0}, monobitmap);
        
        stbtt_FreeBitmap(monobitmap, 0);
    }
    
    text_bitmap_t text_bitmap = {};
    
    
    
    memcpy(font.bitmaps, t_bitmap, TEXT_BITMAP_COUNT*sizeof(*t_bitmap));
    
    tb.font = &font;
    sb.font = &font;
    
    text_app_t app = {};
    
    // NOTE(ajeej): Initialize app
    app.text_box = tb;
    app.util_box = sb;
    app.show_utils = 0;
    app.show_text = 1;
    app.state_type = text_edit_state_type_edit;
    
    char cur_dir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, cur_dir);
    concatenate_strings(cur_dir, "\\");
    app.cur_dir = cur_dir;
    
    char cur_file[MAX_PATH];
    app.cur_file = cur_file;
    
    global_running = true;
    
    win32_timer_begin_frame(&timer);
    
    while(global_running)
    {
        // NOTE(ajeej): Clear trans arena
        memset(trans_arena.base_ptr, 0, trans_arena.size);
        
        
        
        char buffer[256];
        sprintf(buffer, "SPF %F\n", timer.target_sec_per_frame);
        OutputDebugStringA(buffer);
        
        win32_process_messages(window_handle, &input);
        
        // NOTE(ajeej): Clear before input since there is some
        //              implicit rendering
        push_clear(render_buffer, {0, 0, 0, 1}, {8, 8});
        
        
        render_text_box(render_buffer, &app.text_box);
        
        bitmap_t cursor_bit = {};
        text_edit_state_t *state;
        
        state = &app.text_box.state;
        create_bitmap(&cursor_bit, &trans_arena, (state->cursor_dim*38).x, (state->cursor_dim*38).y, {0, 0}, V4(1, 1, 1, 1));
        
        push_bitmap(render_buffer, &cursor_bit,
                    state->cursor_dim.height, state->cursor_pos, {1, 0},
                    {0, 1});
        
        process_edit_input(render_buffer, &input, &app);
        
        if(app.show_utils)
            render_text_box(render_buffer, &app.util_box);
        
        submit_render_command_buffer(&queue, render_buffer, &global_framebuffer.bitmap);
        
        end_render_command_buffer(render_buffer);
        
        //win32_timer_wait(&timer);
        
        // NOTE(ajeej): Present Frame buffer
        HDC window_dc = GetDC(window_handle);
        RECT rect = {};
        GetClientRect(window_handle, &rect);
        StretchDIBits(window_dc,
                      0, 0, rect.right-rect.left, rect.bottom-rect.top,
                      0, 0, global_framebuffer.width, global_framebuffer.height,
                      global_framebuffer.data,
                      &global_framebuffer.info,
                      DIB_RGB_COLORS, SRCCOPY);
        
        ReleaseDC(window_handle, window_dc);
        
        // NOTE(ajeej): Reset trans arena
        trans_arena.used = 0;
        
        win32_timer_end_frame(&timer);
    }
    
    free(ttf_buffer);
    
    quit:
    return 0;
}