
// NOTE(ajeej): Catch all for updating the input state of any button
internal b32
update_input_state(u8 *input_state, b32 is_down, b32 was_down, u8 modifiers)
{
    b32 was_pressed_or_released = 0;
    
    // TODO(ajeej): Find a way to make modifier update faster
    *input_state &= ~(Input_State_Ctrl|Input_State_Alt|Input_State_Shift);
    *input_state |= modifiers;
    
    if(is_down)
    {
        *input_state |= Input_State_Down;
        if(!was_down)
        {
            *input_state |= Input_State_Pressed;
            was_pressed_or_released = 1;
        }
    }
    else
    {
        *input_state &= ~(Input_State_Down);
        *input_state &= ~(Input_State_Pressed);
        *input_state |= Input_State_Released;
        was_pressed_or_released = 1;
    }
    
    return was_pressed_or_released;
}

inline void 
init_character_map(char map[CHAR_MAP_SIZE])
{
    char characters[CHAR_MAP_SIZE] =
    {
        // NOTE(ajeej): Quick way to initalize array of character
        //              that will be taken into the character buffer
#define Key(id, c1, c2) c1, c2,
#include "my_text_editor_char.inc"
#undef Key
    };
    
    memcpy(map, characters, sizeof(*map)*CHAR_MAP_SIZE);
}

inline char
get_character(u32 key, b32 mod)
{
    key-=26; //offset of characters from array index
    key*=2;  //each character has is mod counterpart
    key+=mod;
    return character_map[key];
}