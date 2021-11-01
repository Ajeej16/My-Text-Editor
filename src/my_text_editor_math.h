/* date = April 10th 2021 4:31 pm */

#ifndef MY_TEXT_EDITOR_MATH_H
#define MY_TEXT_EDITOR_MATH_H

#include "math.h"

// NOTE(ajeej): Math operation used for rendering

inline f32
absolute(f32 x)
{
    return fabs(x);
}

inline i32
round_f32_to_i32(f32 x)
{
    return (i32)roundf(x);
}

inline u32
round_f32_to_u32(f32 x)
{
    return (u32)roundf(x);
}

inline f32
square_root(f32 x)
{
#if 0
    return sqrt(x);
#endif
    // NOTE(ajeej): More effeciate square root
    //              to speed up software rendering
    union {int i; float f;} u;
    u.i = 0x2035AD0C + (*(int*)&x >> 1);
    return x / u.f + u.f * 0.25f;
}

inline i32
floor_to_i32(f32 x)
{
    return floorf(x);
}

inline i32
ceil_to_i32(f32 x)
{
    return ceilf(x);
}

inline f32
square(f32 x)
{
    return x*x;
}

inline void
upper_clamp(u32 *x, u32 max)
{
    if(*x > max)
    {
        *x = max;
    }
}

inline void
upper_clamp(i32 *x, i32 max)
{
    if(*x > max)
    {
        *x = max;
    }
}

inline void
lower_clamp(u32 *x, u32 min)
{
    if(*x < min)
    {
        *x = min;
    }
}

inline void
lower_clamp(i32 *x, i32 min)
{
    if(*x < min)
    {
        *x = min;
    }
}

inline f32
lerp(f32 a, f32 b, f32 t)
{
    return (1.0f - t)*a + t*b;
}

inline f32
pixels_to_cms(f32 x)
{
    return x/38.0f;
}

inline f32
cms_to_pixel(f32 x)
{
    return x*38.0f;
}

// NOTE(ajeej): use union in order to
//              have different names for vector
//              in different situations
union v2
{
    
    struct
    {
        f32 x;
        f32 y;
    };
    
    struct
    {
        f32 width;
        f32 height;
    };
    
    f32 e[2];
    
    inline f32& operator[](i32 index)
    {
        return e[index];
    }
    
};

union iv2
{
    
    struct
    {
        i32 x;
        i32 y;
    };
    
    struct
    {
        i32 width;
        i32 height;
    };
    
    i32 e[2];
    
    inline i32& operator[](i32 index)
    {
        return e[index];
    }
    
};

union uv2
{
    
    struct
    {
        u32 x;
        u32 y;
    };
    
    struct
    {
        u32 width;
        u32 height;
    };
    
    u32 e[2];
    
    inline u32& operator[](i32 index)
    {
        return e[index];
    }
    
};

union v3
{
    
    struct
    {
        f32 x;
        f32 y;
        f32 z;
    };
    
    struct
    {
        v2 xy;
        f32 _z;
    };
    
    struct
    {
        f32 r;
        f32 g;
        f32 b;
    };
    
    struct
    {
        f32 width;
        f32 height;
        f32 depth;
    };
    
    f32 e[3];
    
    inline f32& operator[](i32 index)
    {
        return e[index];
    }
    
};

union v4
{
    
    struct
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };
    
    struct
    {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };
    
    struct
    {
        v3 xyz;
        f32 _w;
    };
    
    struct
    {
        v3 rgb;
        f32 _a;
    };
    
    f32 e[4];
    
    inline f32& operator[](i32 index)
    {
        return e[index];
    }
    
};

// NOTE(ajeej): Quick construtor definitions
#define _v2(x, y) v2{x,y}
#define _uv2(x, y) uv2{x,y}
#define _iv2(x, y) iv2{x,y}

#define _v3(x, y, z) v3{x,y,z}
#define _v4(x, y, z, w) v4{x,y,z,w}
#define _v3_to_v4(xyz, w) v4{xyz[0], xyz[1], xyz[2], w}


// NOTE(ajeej): This can be don with a constructor within each
//              class; however, I prefer it like this
inline v2 
V2(f32 x, f32 y)
{
    return _v2(x, y);
}

inline uv2 
UV2(u32 x, u32 y)
{
    return _uv2(x, y);
}

inline iv2 
IV2(i32 x, i32 y)
{
    return _iv2(x, y);
}

inline v3 
V3(f32 x, f32 y, f32 z)
{
    return _v3(x, y, z);
}

inline v3 
V3(v2 xy, f32 z)
{
    return _v3(xy.x, xy.y, z);
}

inline v3 
V3(f32 x, v2 yz)
{
    return _v3(x, yz.x, yz.y);
}

inline v4
V4(f32 x, f32 y, f32 z, f32 w)
{
    return _v4(x, y, z, w);
}

inline v4
V4(v2 xy, f32 z, f32 w)
{
    return _v4(xy.x, xy.y, z, w);
}

inline v4
V4(f32 x, f32 y, v2 zw)
{
    return _v4(x, y, zw.x, zw.y);
}

inline v4
V4(v2 xy, v2 zw)
{
    return _v4(xy.x, xy.y, zw.x, zw.y);
}

inline v4
V4(f32 x, v2 yz, f32 w)
{
    return _v4(x, yz.x, yz.y, w);
}


// NOTE(ajeej): Operator overloads
inline v2
operator+(v2 a, v2 b)
{
    return V2(a.x+b.x, a.y+b.y);
}

inline v2
operator-(v2 a, v2 b)
{
    return V2(a.x-b.x, a.y-b.y);
}

inline v2&
operator-=(v2 &a, v2 b)
{
    a = V2(a.x-b.x, a.y-b.y);
    
    return a;
}


inline v2&
operator+=(v2 &a, v2 b)
{
    a = V2(a.x+b.x, a.y+b.y);
    
    return a;
}

inline v2
operator-(v2 a)
{
    return V2(-a.x, -a.y);
}

inline v2
operator*(f32 a, v2 b)
{
    return V2(b.x*a, b.y*a);
}

inline v2
operator*(v2 b, f32 a)
{
    return V2(b.x*a, b.y*a);
}

inline v2
operator*(v2 b, v2 a)
{
    return V2(b.x*a.x, b.y*a.y);
}

inline v2&
operator*=(v2 &a, f32 b)
{
    a = a*b;
    return a;
}

inline v2&
operator*=(v2 &a, v2 b)
{
    a = V2(a.x*b.x, a.y*b.y);
    return a;
}

inline v3
operator+(v3 a, v3 b)
{
    return V3(a.x+b.x, a.y+b.y, a.z+b.z);
}

inline v3
operator-(v3 a, v3 b)
{
    return V3(a.x-b.x, a.y-b.y, a.z-b.z);
}

inline v3
operator*(f32 a, v3 b)
{
    return V3(b.x*a, b.y*a, b.z*a);
}

inline v3
operator*(v3 b, f32 a)
{
    return V3(b.x*a, b.y*a, b.z*a);
}

inline v3&
operator*=(v3 &a, f32 b)
{
    a = a*b;
    return a;
}

inline v4
operator+(v4 a, v4 b)
{
    return V4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
}

inline v4
operator-(v4 a, v4 b)
{
    return V4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);
}

inline v4
operator*(f32 a, v4 b)
{
    return V4(b.x*a, b.y*a, b.z*a, b.w*a);
}

inline v4
operator*(v4 b, f32 a)
{
    return V4(b.x*a, b.y*a, b.z*a, b.w*a);
}

inline v4&
operator*=(v4 &a, f32 b)
{
    a = a*b;
    return a;
}

inline v4&
operator*=(v4 &a, v4 b)
{
    a = V4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);
    return a;
}


// NOTE(ajeej): Vector math functions
inline v4
u32_to_v4(u32 x)
{
    v4 result = {
        (f32)((x >> 16) & 0xFF),
        (f32)((x >>  8) & 0xFF),
        (f32)((x >>  0) & 0xFF),
        (f32)((x >> 24) & 0xFF)
    };
    
    return result;
}

inline v2
hadamard_product(v2 a, v2 b)
{
    v2 result = V2(a.x*b.x, a.y*b.y);
    return result;
}

inline v2
invert(v2 x)
{
    return V2(1/x.x, 1/x.y);
}

inline f32
dot_product(v2 a, v2 b)
{
    return (a.x*b.x) + (a.y*b.y);
}

inline f32
length_square(v2 a)
{
    return dot_product(a, a);
}

inline v4
light_linear_to_srgb(v4 color)
{
    v4 corrected = {};
    corrected.r = 255.0f*square_root(color.r);
    corrected.g = 255.0f*square_root(color.g);
    corrected.b = 255.0f*square_root(color.b);
    corrected.a = 255.0f*color.a;
    
    return corrected;
}

inline v4
srgb_to_light_linear(v4 color)
{
    v4 corrected = {};
    f32 inv255 = 1/255.0f;
    
    corrected.r = square(inv255*color.r);
    corrected.g = square(inv255*color.g);
    corrected.b = square(inv255*color.b);
    corrected.a = inv255*color.a;
    
    return corrected;
}

inline v4
lerp(v4 a, v4 b, f32 t)
{
    v4 result = {
        lerp(a.x, b.x, t),
        lerp(a.x, b.x, t),
        lerp(a.x, b.x, t),
        lerp(a.x, b.x, t),
    };
    
    return result;
}

union rect2d
{
    struct
    {
        f32 xmin;
        f32 ymin;
        f32 xmax;
        f32 ymax;
    };
    
    struct
    {
        f32 left;
        f32 top;
        f32 right;
        f32 bottom;
    };
    
    f32 e[4];
    
    inline f32& operator[](i32 index)
    {
        return e[index];
    }
};

union irect2d
{
    struct
    {
        i32 xmin;
        i32 ymin;
        i32 xmax;
        i32 ymax;
    };
    
    struct
    {
        i32 left;
        i32 top;
        i32 right;
        i32 bottom;
    };
    
    i32 e[4];
    
    inline i32& operator[](i32 index)
    {
        return e[index];
    }
};

union urect2d
{
    struct
    {
        u32 xmin;
        u32 ymin;
        u32 xmax;
        u32 ymax;
    };
    
    struct
    {
        u32 left;
        u32 top;
        u32 right;
        u32 bottom;
    };
    
    u32 e[4];
    
    inline u32& operator[](i32 index)
    {
        return e[index];
    }
};

#endif //MY_TEXT_EDITOR_MATH_H
