/* date = April 17th 2021 0:27 pm */

#ifndef MY_TEXT_EDITOR_INPUT_H
#define MY_TEXT_EDITOR_INPUT_H

// NOTE(ajeej): nicer way to initialize enums with
//              extra information attached in case of
//              debug purposes
enum key_id
{
    // NOTE(ajeej): Look in "input_keys.inc" for definitions
#define Key(name, str) Key_ID_##name,
#include "my_text_editor_key.inc"
#undef Key
    Key_ID_Max,
};

enum mouse_id
{
    Mouse_ID_Left,
    Mouse_ID_Right,
    Mouse_ID_Middle,
    Mouse_ID_Btn1,
    Mouse_ID_Btn2,
    
    Mouse_ID_Max,
};

// NOTE(ajeej): Flags for input state of each button
enum input_state
{
    
    Input_State_Down = (1<<0),
    Input_State_Pressed = (1<<1),
    Input_State_Released = (1<<2),
    Input_State_Double = (1<<3),
    Input_State_Ctrl = (1<<4),
    Input_State_Shift = (1<<5),
    Input_State_Alt = (1<<6),
    
};

struct input_t
{
    u8 key_states[75];
    u8 mouse_states[5];
    char char_buffer[16];
    u8 char_index;
    b8 caps_on;
    v2 mouse_pos;
    v2 mouse_delta;
    v2 mouse_scroll;
};

// NOTE(ajeej): Quick definitions in order to check flags
#define IsKeyPressed(input, id) ((*input).key_states[id] & Input_State_Pressed)
#define IsKeyReleased(input, id) ((*input).key_states[id] & Input_State_Released)
#define IsKeyDown(input, id) ((*input).key_states[id] & Input_State_Down)
#define IsKeyUp(input, id) (!IsKeyDown(input, id))

#define IsMousePressed(input, id) ((*input).mouse_states[id] & Input_State_Pressed)
#define IsMouseReleased(input, id) ((*input).mouse_states[id] & Input_State_Released)
#define IsMouseDown(input, id) ((*input).mouse_states[id] & Input_State_Down)
#define IsMouseUp(input, id) (!IsMouseDown(input, id))

#define IsKeyAndModPressed(input, id, mod) (IsKeyPressed(input, id) && (input->key_states[id] & mod))

// NOTE(ajeej): This is the max number of characters from the 
//              character include file for the input stage of 
//              the character buffer
#define CHAR_MAP_SIZE 102



#endif //MY_TEXT_EDITOR_INPUT_H
