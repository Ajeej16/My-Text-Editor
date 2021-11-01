/* date = April 25th 2021 9:24 am */

#ifndef MY_TEXT_EDITOR_STRING_H
#define MY_TEXT_EDITOR_STRING_H

// NOTE(ajeej): Utility string functions for string manipulation

// NOTE(ajeej): With null character
inline u32
string_length(char *str)
{
    return strlen(str)+1;
}

inline void
clear_string(char *str, u32 pos)
{
    u32 size = strlen(str);
    
    for(u32 i = pos;
        i < size;
        i++)
    {
        str[i] = 0;
    }
    
}

inline void
clear_string_with(char *str, char c, u32 pos)
{
    u32 size = strlen(str);
    
    for(u32 i = pos;
        i < size;
        i++)
    {
        str[i] = c;
    }
    
}

inline void
clear_string(char *str)
{
    u32 size = strlen(str);
    for(u32 i = 0;
        i < size;
        i++)
    {
        str[i] = 0;
    }
}

inline void
clear_string_with(char *str, char c)
{
    u32 size = strlen(str);
    for(u32 i = 0;
        i < size;
        i++)
    {
        str[i] = c;
    }
}

inline void
copy_string(char *dest, char*src, u32 pos)
{
    while((*dest++ = *src++) && pos > 0) pos--;
}

inline void
copy_string(char *dest, char*src)
{
    while(*dest++ = *src++);
}


inline void
concatenate_strings(char *dest, char *src)
{
    while(*dest) dest++;
    while(*dest++ = *src++);
}

inline void
append_char(char *dest, char c)
{
    while(*dest) dest++;
    *dest++ = c;
    *dest = '\0';
}

internal void
insert_char(char *dest, char c, i32 pos)
{
    char *temp_str = (char *)malloc(sizeof(c)*(strlen(dest)+2));
    copy_string(temp_str, dest, pos);
    temp_str[pos] = '\0';
    append_char(temp_str, c);
    concatenate_strings(temp_str, dest+pos);
    copy_string(dest, temp_str);
    free(temp_str);
}

internal void
delete_char(char *dest, i32 pos)
{
    char *temp_str = (char *)malloc(sizeof(char)*(strlen(dest)));
    copy_string(temp_str, dest, pos);
    temp_str[pos] = '\0';
    concatenate_strings(temp_str, dest+pos+1);
    copy_string(dest, temp_str);
    free(temp_str);
}

#define CHAR_COUNT 256

inline void
badchar_heuristic(char *str,
                  u32 size,
                  i32 badchar[CHAR_COUNT])
{
    for(u32 i = 0; 
        i < CHAR_COUNT; 
        i++)
        badchar[i] = -1;
    
    for(u32 i = 0;
        i < size;
        i++)
        badchar[(u32)str[i]] = i;
}


#define STR_MAX_SIZE 10000

struct string_search_info_t
{
    u32 size;
    i32 pos[STR_MAX_SIZE];
};

// NOTE(ajeej): Boyer-Moore string search algorithm
//              Used to find each individual index for 
//              the location of a substring
internal string_search_info_t
search_substring(char *text,
                 char *sub)
{
    string_search_info_t result = {};
    
    i32 sub_size = strlen(sub);
    i32 text_size = strlen(text);
    
    i32 badchar[CHAR_COUNT];
    
    badchar_heuristic(sub, sub_size, badchar);
    
    i32 s = 0;
    u32 index = 0;
    
    while(s <= (text_size - sub_size))
    {
        i32 j = sub_size - 1;
        
        while(j >= 0 && sub[j] == text[s+j])
            j--;
        
        if(j < 0)
        {
            result.pos[index++] = s;
            
            s += (s+sub_size < text_size) ? sub_size-badchar[text[s+sub_size]] : 1;
        }
        else
        {
            s += Max(1, j - badchar[text[s+j]]);
        }
        
    }
    
    result.size = index;
    
    return result;
}

#endif //MY_TEXT_EDITOR_STRING_H
