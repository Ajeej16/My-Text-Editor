/* date = April 15th 2021 9:01 am */

#ifndef MY_TEXT_EDITOR_RENDER_COMMAND_H
#define MY_TEXT_EDITOR_RENDER_COMMAND_H



enum render_command_type_t
{
    render_command_type_clear,
    render_command_type_bitmap,
    render_command_type_rect,
};

struct render_command_header_t
{
    render_command_type_t type;
};

struct render_command_bitmap_t
{
    bitmap_t *in;
    bitmap_t *out;
    
    v4 color;
    v2 pos;
    v2 xbase;
    v2 ybase;
};

struct render_command_rect_t
{
    bitmap_t *out;
    
    v2 pos;
    v2 dim;
    v4 color;
};

struct render_command_clear_t
{
    bitmap_t *out;
    
    v2 pos;
    u32 tilex;
    u32 tiley;
    v2 dim;
    v4 color;
};

struct render_space_t
{
    f32 cms_to_pixels;
    v2 origin;
    v2 offset;
};

// NOTE(ajeej): Used mostly for the tranformation of
//              cms units to pixel units
struct basis_transformation_result_t
{
    v2 pos;
    f32 scale;
};

// NOTE(ajeej): Render command buffer contains array of
//              render commands which will be pulled out for
//              per thread execution when it is submitted
struct render_command_buffer_t
{ 
    render_space_t space;
    
    u32 max_buffer_size;
    u32 buffer_size;
    //u32 base;
    u8 *buffer_base_ptr;
};

struct sub_pixel_sample_t
{
    u32 center;
    u32 right;
    u32 bottom;
    u32 diagonal;
};

#define CmToPixel 1.0f/38.0f

#endif //MY_TEXT_EDITOR_RENDER_COMMAND_H
