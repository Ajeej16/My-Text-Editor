
// NOTE(ajeej): Currently not used
//              intended for subpixel sampling
//              too slow for little visual benefit
internal sub_pixel_sample_t
sub_pixel_sampling(bitmap_t *bitmap, u32 x, u32 y)
{
    sub_pixel_sample_t sample = {};
    
    u8 *texel = ((u8 *)bitmap->data) + (y*bitmap->width + x)*4;
    
    sample.center = *(u32 *)texel;
    sample.right = *(u32 *)(texel + sizeof(u32));
    sample.bottom = *(u32 *)(texel + bitmap->width*sizeof(u32));
    sample.diagonal = *(u32 *)(texel + (bitmap->width+1)*sizeof(u32));
    
    return sample;
}

internal v4
sub_pixel_blend(sub_pixel_sample_t sample, f32 dx, f32 dy)
{
    v4 t_center = srgb_to_light_linear(u32_to_v4(sample.center));
    v4 t_right = srgb_to_light_linear(u32_to_v4(sample.right));
    v4 t_bottom = srgb_to_light_linear(u32_to_v4(sample.bottom));
    v4 t_diagonal = srgb_to_light_linear(u32_to_v4(sample.diagonal));
    
#if 0
    t_center.rgb *= t_center.a;
    t_right.rgb *= t_right.a;
    t_bottom.rgb *= t_bottom.a;
    t_diagonal.rgb *= t_diagonal.a;
#endif
    
    v4 blend = lerp(lerp(t_center, t_right, dx),
                    lerp(t_bottom, t_diagonal, dx), dy);
    
    
    return blend;
}

internal basis_transformation_result_t
transform_position_space(render_space_t *space, v2 in_pos)
{
    basis_transformation_result_t result = {};
    
    v2 pos = in_pos + space->offset;
    
    result.pos = space->origin + space->cms_to_pixels*pos;
    result.scale = space->cms_to_pixels;
    
    return result;
}

internal render_command_buffer_t *
init_render_command_buffer(memory_arena_t *arena,
                           u32 max_size)
{
    render_command_buffer_t *result = 0;
    
    result = PushStruct(arena, render_command_buffer_t);
    
    result->buffer_base_ptr = (u8 *)PushSize(arena, max_size);
    result->max_buffer_size = max_size;
    result->buffer_size = 0;
    
    result->space.cms_to_pixels = 38.0f;
    
    return result;
}

// NOTE(ajeej): Push any command into the command buffer and advance
//              the buffer size (specifics base pointer)
internal void *
push_render_command(render_command_buffer_t *buffer,
                    render_command_type_t type,
                    u32 size)
{
    size += sizeof(render_command_header_t);
    
    void *result = NULL;
    
    if((buffer->buffer_size + size) < buffer->max_buffer_size)
    {
        render_command_header_t *header = (render_command_header_t *)(buffer->buffer_base_ptr + buffer->buffer_size);
        
        header->type = type;
        result = (u8 *)header + sizeof(*header);
        buffer->buffer_size += size;
    }
    
    return result;
    
}

internal void
render_bitmap(bitmap_t *out_bitmap, bitmap_t *in_bitmap,
              v2 origin, v2 xbase, v2 ybase, v4 color = V4(1, 1, 1, 1))
{
    color.rgb *= color.a;
    
    i32 max_width = (i32)out_bitmap->width;
    i32 max_height = (i32)out_bitmap->height;
    
    f32 inv_length2_x = 1/length_square(xbase);
    f32 inv_length2_y = 1/length_square(ybase);
    
    irect2d bound = {max_width, max_height, 0, 0};
    
    v2 test_points[4] = {
        origin, 
        origin + xbase, 
        origin + ybase, 
        origin + xbase + ybase
    };
    
    // NOTE(ajeej): Create a bounding box over the bitmap
    //              Very versatile since it handles rotation
    for(u32 point_index = 0;
        point_index < ArrayCount(test_points);
        point_index++)
    {
        v2 point = test_points[point_index];
        
        i32 floorx = floor_to_i32(point.x);
        i32 ceilx = ceil_to_i32(point.x);
        i32 floory = floor_to_i32(point.y);
        i32 ceily = ceil_to_i32(point.y);
        
        upper_clamp(&bound.xmin, floorx);
        upper_clamp(&bound.ymin, floory);
        
        lower_clamp(&bound.xmax, ceilx);
        lower_clamp(&bound.ymax, ceily);
    }
    
    lower_clamp(&bound.xmin, 0);
    lower_clamp(&bound.ymin, 0);
    
    upper_clamp(&bound.xmax, max_width);
    upper_clamp(&bound.ymax, max_height);
    
    
    u8 *dst_row = (u8 *)out_bitmap->data + ((bound[1]*out_bitmap->width + bound[0])*4);
    
    for(u32 y = bound.ymin;
        y < bound.ymax;
        y++)
    {
        u32 *dst_pixel = (u32 *)dst_row;
        for(u32 x = bound.xmin;
            x < bound.xmax;
            x++)
        {
            
            v2 point = V2(x, y);
            
            v2 distance = point - origin;
            
            f32 edge_test0 = dot_product(distance, {xbase.y, -xbase.x});
            f32 edge_test1 = dot_product(distance - xbase, {ybase.y, -ybase.x}); 
            f32 edge_test2 = dot_product(distance - xbase - ybase, {-xbase.y, xbase.x});
            f32 edge_test3 = dot_product(distance - ybase, {-ybase.y, ybase.x}); 
            
            if(edge_test0 < 0.0f &&
               edge_test1 < 0.0f &&
               edge_test2 < 0.0f &&
               edge_test3 < 0.0f)
            {
                // NOTE(ajeej): get UV coordinates of the texture of the bitmap
                //              in order to handle scaling properly
                f32 u = inv_length2_x*dot_product(distance, xbase);
                f32 v = inv_length2_y*dot_product(distance, ybase);
                
#if 0
                // NOTE(ajeej): Removed sampling
                f32 texel_x = ((u * (f32)(in_bitmap->width - 2)));
                f32 texel_y = ((v * (f32)(in_bitmap->height - 2)));
                
                i32 pixel_x = (i32)texel_x;
                i32 pixel_y = (i32)texel_y;
                
                
                f32 sub_pixel_dx = texel_x - (f32)pixel_x;
                f32 sub_pixel_dy = texel_y - (f32)pixel_y;
                
                sub_pixel_sample_t sample = sub_pixel_sampling(in_bitmap,
                                                               pixel_x, pixel_y);
                
                v4 texel = sub_pixel_blend(sample, 
                                           sub_pixel_dx,
                                           sub_pixel_dy);
                
#else
                
                f32 texel_x = ((u * (f32)(in_bitmap->width)));
                f32 texel_y = ((v * (f32)(in_bitmap->height)));
                
                i32 pixel_x = (i32)texel_x;
                i32 pixel_y = (i32)texel_y;
                
                u32 *texel_u32 = (u32 *)(((u8 *)in_bitmap->data) + (pixel_y*in_bitmap->width + pixel_x)*4);
                
                // NOTE(ajeej): Used for gamma correction; however,
                //              the visual benefit is hard to tell
                v4 texel = srgb_to_light_linear(u32_to_v4(*texel_u32));
#endif
                
                
                
                texel *= color;
                
                v4 dst = {
                    (f32)((*dst_pixel >> 16) & 0xFF),
                    (f32)((*dst_pixel >> 8 ) & 0xFF),
                    (f32)((*dst_pixel >> 0 ) & 0xFF),
                    (f32)((*dst_pixel >> 24) & 0xFF)
                };
                
                dst = srgb_to_light_linear(dst);
                
                v4 final = (1.0f-texel.a)*dst + texel;
                
                final = light_linear_to_srgb(final);
                
                *dst_pixel = (((u32)(final.a + 0.5f) << 24) |
                              ((u32)(final.r + 0.5f) << 16) |
                              ((u32)(final.g + 0.5f) <<  8) |
                              ((u32)(final.b + 0.5f) <<  0));
                
                
            }
            
            ++dst_pixel;
        }
        
        dst_row += out_bitmap->width*4;
    }
}


// NOTE(ajeej): Less computationally expensive version of the previous
//              function
internal void
render_bitmap_basic(bitmap_t *out_bitmap, bitmap_t *in_bitmap, v2 origin, v2 dim, v4 color = V4(1, 1, 1, 1))
{
    color.rgb *= color.a;
    
    v2 axisx = V2(dim.x, 0);
    v2 axisy = V2(0, dim.y);
    
    i32 max_width = (i32)out_bitmap->width;
    i32 max_height = (i32)out_bitmap->height;
    
    f32 inv_length2_x = 1/square(dim.x);
    f32 inv_length2_y = 1/square(dim.y);
    
    irect2d bound = {(i32)origin.x, (i32)origin.y, (i32)(origin.x+dim.x), (i32)(origin.y+dim.y)};
    
    lower_clamp(&bound.xmin, 0);
    lower_clamp(&bound.ymin, 0);
    
    upper_clamp(&bound.xmax, max_width);
    upper_clamp(&bound.ymax, max_height);
    
    
    u8 *dst_row = (u8 *)out_bitmap->data + ((bound[1]*out_bitmap->width + bound[0])*4);
    
    for(u32 y = bound.ymin;
        y < bound.ymax;
        y++)
    {
        u32 *dst_pixel = (u32 *)dst_row;
        for(u32 x = bound.xmin;
            x < bound.xmax;
            x++)
        {
            
            v2 point = V2(x, y);
            
            v2 distance = point - origin;
            
            f32 u = inv_length2_x*(distance.x*dim.x);
            f32 v = inv_length2_y*(distance.y*dim.y);
            
            f32 texel_x = ((u * (f32)(in_bitmap->width - 2)));
            f32 texel_y = ((v * (f32)(in_bitmap->height - 2)));
            
            i32 pixel_x = (i32)texel_x;
            i32 pixel_y = (i32)texel_y;
            
            f32 sub_pixel_dx = texel_x - (f32)pixel_x;
            f32 sub_pixel_dy = texel_y - (f32)pixel_y;
            
            sub_pixel_sample_t sample = sub_pixel_sampling(in_bitmap,
                                                           pixel_x, pixel_y);
            
            v4 texel = sub_pixel_blend(sample, 
                                       sub_pixel_dx,
                                       sub_pixel_dy);
            
            texel *= color;
            
            v4 dst = {
                (f32)((*dst_pixel >> 16) & 0xFF),
                (f32)((*dst_pixel >> 8 ) & 0xFF),
                (f32)((*dst_pixel >> 0 ) & 0xFF),
                (f32)((*dst_pixel >> 24) & 0xFF)
            };
            
            dst = srgb_to_light_linear(dst);
            
            v4 final = (1.0f-texel.a)*dst + texel;
            
            final = light_linear_to_srgb(final);
            
            *dst_pixel = (((u32)(final.a + 0.5f) << 24) |
                          ((u32)(final.r + 0.5f) << 16) |
                          ((u32)(final.g + 0.5f) <<  8) |
                          ((u32)(final.b + 0.5f) <<  0));
            
            
            ++dst_pixel;
        }
        
        dst_row += out_bitmap->width*4;
    }
}


// NOTE(ajeej): For rendering plan colored rectangles
internal void
render_rect(bitmap_t *out_bitmap, v2 pos, v2 dim, v4 color)
{
    color.rgb *= color.a;
    
    i32 maxx = pos.x + dim.x;
    i32 maxy = pos.y + dim.y;
    
    i32 minx = pos.x;
    i32 miny = pos.y;
    
    lower_clamp(&minx, 0);
    lower_clamp(&miny, 0);
    upper_clamp(&maxx, out_bitmap->width-1);
    upper_clamp(&maxy, out_bitmap->height-1);
    
    u8 *row = (u8 *)out_bitmap->data + (u64)((minx + miny*out_bitmap->width)*4);
    for(i32 y = miny;
        y < maxy;
        y++)
    {
        u32 *pixel = (u32 *)row;
        for(i32 x = minx;
            x < maxx;
            x++)
        {
            v4 final;
            
            
            if(color.a < 1)
            {
                v4 dst = u32_to_v4(*pixel);
                
                dst = srgb_to_light_linear(dst);
                
                final = (1.0f-color.a)*dst + color;
                
            }
            else
            {
                final = color;
            }
            
            final = light_linear_to_srgb(final);
            
            *pixel++= (((u32)(final.a) << 24) |
                       ((u32)(final.r) << 16) |
                       ((u32)(final.g) <<  8) |
                       ((u32)(final.b) <<  0));
            
            
        }
        
        row += 4*out_bitmap->width;
    }
}

// NOTE(ajeej): Thread functions that are called within the work queue
internal void
threaded_clear(work_queue_t *queue, void *params)
{
    render_command_clear_t *work = (render_command_clear_t *)params;
    
    render_rect(work->out, work->pos, work->dim, work->color);
}

internal void
threaded_render_rect(work_queue_t *queue, void *params)
{
    render_command_rect_t *work = (render_command_rect_t *)params;
    
    render_rect(work->out, work->pos, work->dim, work->color);
}

internal void
threaded_render_bitmap(work_queue_t *queue, void *params)
{
    render_command_bitmap_t *work = (render_command_bitmap_t *)params;
    
    render_bitmap(work->out, work->in, work->pos, work->xbase,
                  work->ybase,
                  work->color);
    
}

internal void
submit_render_command_buffer(work_queue_t *queue, render_command_buffer_t *buffer, bitmap_t *out)
{
    
    // NOTE(ajeej): Iterate through the render command buffer to submit 
    //              work into the work queue for threads to later handle
    //              the rendering
    for(u32 base = 0;
        base < buffer->buffer_size;
        )
    {
        render_command_header_t *header = (render_command_header_t *)(buffer->buffer_base_ptr + base);
        base += sizeof(*header);
        
        void *data = (u8 *)header + sizeof(*header);
        switch(header->type)
        {
            case render_command_type_clear:
            {
                render_command_clear_t *clear_command = (render_command_clear_t *)data;
                
                u32 maxx = clear_command->tilex;
                u32 maxy = clear_command->tiley;
                u32 width = out->width;
                u32 height = out->height;
                
                clear_command->out = out;
                clear_command->dim *= V2((f32)width, (f32)height);
                clear_command->pos = {clear_command->dim.x*maxx, 
                    clear_command->dim.y*maxy};
                
                
                win32_add_entry(queue, threaded_clear, data);
                
                
                base += sizeof(*clear_command);
                
            } break;
            
            case render_command_type_bitmap:
            {
                
                render_command_bitmap_t *bitmap_command = (render_command_bitmap_t *)data;
                
                bitmap_command->out = out;
                
                win32_add_entry(queue, threaded_render_bitmap, data);
                
                base += sizeof(*bitmap_command);
                
            } break;
            
            case render_command_type_rect:
            {
                render_command_rect_t * rect_command = (render_command_rect_t *)data;
                
                rect_command->out = out;
                
                win32_add_entry(queue, threaded_render_rect, data);
                
                
                base += sizeof(*rect_command);
            };
            
        }
    }
    
    win32_complete_queue(queue);
}


internal void
push_clear(render_command_buffer_t *buffer, v4 color, v2 tiles)
{
    v2 dim = invert(tiles);
    
    // NOTE(ajeej): The clear is broken up into mutliple rectangle
    //              for multiple threads to work on it at the same
    //              time to improve performance
    for(u32 y = 0;
        y < tiles.y;
        y++)
    {
        for(u32 x = 0;
            x < tiles.x;
            x++)
        {
            render_command_clear_t *command =
                (render_command_clear_t *)push_render_command(buffer,
                                                              render_command_type_clear,
                                                              sizeof(*command));
            
            if(command)
            {
                command->color = color;
                command->dim = dim;
                command->tilex = x;
                command->tiley = y;
            }
        }
    }
}

internal void
push_rect(render_command_buffer_t *buffer, v2 pos, v2 dim, v4 color,
          v2 alignment,
          v2 tile)
{
    // NOTE(ajeej): Same concept as the push_clear
    
    v2 align = hadamard_product(alignment, dim);
    pos -= align;
    
    basis_transformation_result_t trans = transform_position_space(&buffer->space, pos);
    
    
    dim *= trans.scale;
    dim *= invert(tile);
    
    for(u32 y = 0;
        y < tile.y;
        y++)
    {
        for(u32 x = 0;
            x < tile.x;
            x++)
        {
            render_command_rect_t *command = (render_command_rect_t *)push_render_command(buffer, render_command_type_rect,
                                                                                          sizeof(*command));
            
            if(command)
            {
                v2 tile_pos = trans.pos + (dim * V2(x, y));
                
                command->pos = tile_pos;
                command->color = color;
                command->dim = dim;
            }
        }
    }
}

internal void
push_bitmap(render_command_buffer_t *buffer,
            bitmap_t *bitmap, f32 height,
            v2 offset, v2 xbase, v2 ybase,
            v4 color = V4(1, 1, 1 ,1))
{
    v2 size = V2(height*bitmap->dim_ratio, height);
    v2 align = hadamard_product(bitmap->alignment, size);
    v2 pos = offset - align;
    
    basis_transformation_result_t trans = transform_position_space(&buffer->space, pos);
    
    render_command_bitmap_t *command = (render_command_bitmap_t *)push_render_command(buffer, render_command_type_bitmap,
                                                                                      sizeof(*command));
    if(command)
    {
        v2 dim = trans.scale*size;
        
        command->in = bitmap;
        command->pos = trans.pos;
        command->color = color;
        command->xbase = xbase *dim.x;
        command->ybase = ybase *dim.y;
    }
}

internal void
push_string(render_command_buffer_t *buffer,
            char *str, font_t *font,
            text_edit_state_t *state,
            f32 factor, v2 pos, v4 color = V4(1, 1, 1, 1))
{
    // TODO(ajeej): Better but slower
    
    u32 size = string_length(str);
    u32 row = 0;
    u32 col = 0;
    v2 original_pos = pos;
    f32 def_space = 0.4*factor;
    
    for(u32 i = 0;
        i < size;
        i++)
    {
        // NOTE(ajeej): Push string bitmap on a character basis
        char c = str[i];
        switch(c)
        {
            
            default:
            {
                bitmap_t *character = font->bitmaps + (c-33);
                
                // NOTE(ajeej): Get advancement and the left padding for the
                //              character
                i32 advance_x, left_side_bearing; 
                stbtt_GetCodepointHMetrics(&font->info,
                                           c, &advance_x,
                                           &left_side_bearing);
                
                pos.x += pixels_to_cms(left_side_bearing*font->scale)*factor;
                
                // NOTE(ajeej): Get bounding box of the character bitmap
                i32 x1, y1, x2, y2;
                stbtt_GetCodepointBitmapBox(&font->info, c, font->scale,
                                            font->scale,
                                            &x1, &y1,
                                            &x2, &y2);
                f32 offset =  pixels_to_cms(font->ascent + y1)*factor;
                
                v2 c_pos = {pos.x, pos.y + offset};
                
                f32 height = pixels_to_cms(character->height)*factor;
                
                push_bitmap(buffer, character, height, c_pos, {1, 0}, {0, 1}, color);
                
                // NOTE(ajeej): Apply kerning between current character and 
                //              the character after it
                i32 kern = 0;
                if(str[i+1])
                    kern = stbtt_GetCodepointKernAdvance(&font->info,
                                                         c, str[i+1]);
                
                f32 width = pixels_to_cms((advance_x - left_side_bearing + kern) * font->scale)*factor;
                
                
                // NOTE(ajeej): Update cursor position
                if(state->cursor == i)
                {
                    state->cursor_pos = 
                    {
                        pos.x,
                        pos.y + pixels_to_cms(font->ascent)*factor,
                    };
                    state->cursor_dim = {width, 0.15};
                }
                
                pos.x += width;
            } break;
            
            case ' ':
            {
                // NOTE(ajeej): Update cursor position
                if(state->cursor == i)
                {
                    state->cursor_pos = 
                    {
                        pos.x,
                        pos.y + pixels_to_cms(font->ascent)*factor,
                    };
                    state->cursor_dim = {def_space, 0.15};
                }
                
                pos.x += 0.4f;
            } break;
            
            case '\n':
            {
                // NOTE(ajeej): Update cursor position
                if(state->cursor == i)
                {
                    state->cursor_pos = 
                    {
                        pos.x,
                        pos.y + pixels_to_cms(font->ascent)*factor,
                    };
                    state->cursor_dim = {def_space, 0.15};
                }
                
                pos.y += pixels_to_cms((font->ascent - font->descent) + font->line_gap)*factor;
                pos.x = original_pos.x;
            } break;
            
            case '\0':
            {
                // NOTE(ajeej): Update cursor position
                if(state->cursor == i)
                {
                    state->cursor_pos = 
                    {
                        pos.x,
                        pos.y + pixels_to_cms(font->ascent)*factor,
                    };
                    state->cursor_dim = {def_space, 0.15};
                }
                
            } break;;
            
        }
    }
}

internal void
end_render_command_buffer(render_command_buffer_t *buffer)
{
    // NOTE(ajeej): Clear the command buffer by simply setting
    //              the buffer_size back to 0
    buffer->buffer_size = 0;
}
