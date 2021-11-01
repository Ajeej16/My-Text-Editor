
// NOTE(ajeej): Thread safe entry addition
internal void
win32_add_entry(work_queue_t *queue,
                work_queue_callback *callback,
                void *params)
{
    u32 new_write_index = (queue->next_write_index + 1) % ArrayCount(queue->entries);
    work_queue_entry_t *entry = queue->entries + queue->next_write_index;
    entry->callback = callback;
    entry->params = params;
    ++queue->target;
    // NOTE(ajeej): Make sure all the write are complete until the
    // write index is advanced and the semaphore is released
    _WriteBarrier();
    _mm_sfence();
    queue->next_write_index = new_write_index;
    ReleaseSemaphore(queue->semaphore, 1, 0);
}

// NOTE(ajeej): Execute the next function within the work queue
internal b32
win32_execute_next_queue_entry(work_queue_t *queue)
{
    b32 sleep = 0;
    
    u32 next_read_index = queue->next_read_index;
    // NOTE(ajeej): Queue is supported as a circular buffer
    u32 new_read_index = (next_read_index + 1)%ArrayCount(queue->entries);
    
    if(next_read_index != queue->next_write_index)
    {
        u32 index = InterlockedCompareExchange((LONG volatile *)&queue->next_read_index,
                                               new_read_index,
                                               next_read_index);
        if(index == next_read_index)
        {
            work_queue_entry_t entry = queue->entries[index];
            // NOTE(ajeej): Call function
            entry.callback(queue, entry.params);
            InterlockedIncrement((LONG volatile *)&queue->count);
        }
    }
    else
    {
        sleep = 1;
    }
    
    return sleep;
}

// NOTE(ajeej): Complete the entire queue
// TODO(ajeej): Find a way to do this asynchronously
internal void
win32_complete_queue(work_queue_t *queue)
{
    while(queue->target != queue->count)
    {
        win32_execute_next_queue_entry(queue);
    }
    
    queue->target = 0;
    queue->count = 0;
}