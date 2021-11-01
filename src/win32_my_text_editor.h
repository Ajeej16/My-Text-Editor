/* date = April 17th 2021 0:54 pm */

#ifndef WIN32_MY_TEXT_EDITOR_H
#define WIN32_MY_TEXT_EDITOR_H

struct work_queue_t;
typedef void work_queue_callback(work_queue_t *queue, void *params);
struct work_queue_entry_t
{
    work_queue_callback *callback;
    void *params;
};

struct work_queue_t
{
    u32 volatile target;
    u32 volatile count;
    
    u32 volatile next_write_index;
    u32 volatile next_read_index;
    HANDLE semaphore;
    
    work_queue_entry_t entries[10000];
};

struct win32_timer
{
    i64 performance_freq;
    LARGE_INTEGER begin_frame;
    b32 sleep_is_granular;
    f32 target_sec_per_frame;
};


struct memory_t
{
    u32 storage_size;
    void *storage;
};

struct memory_arena_t
{
    u8 *base_ptr;
    u32 size;
    u32 used;
};


#endif //WIN32_MY_TEXT_EDITOR_H
