#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

#define global static
#define local_persist static
#define internal static

#define ASSERT(expr) if(!(expr)) { *(u32*)0 = 0;}

#define KILOBYTES(x) x*1024
#define MEGABYTES(x) KILOBYTES(x)*1024 
#define GIGABYTES(x) MEGABYTES(x)*1024
#define TERABYTES(x) GIGABYTES(x)*1024

typedef struct
{
    void *memory;
    s32 width;
    s32 height;
    s32 stride;
}AppBackbuffer;


typedef struct
{
    s64  permanent_storage_size;
    s64  transient_storage_size;
    void *permanent_storage;
    void *transient_storage;
    
    bool is_memory_init;
}AppMemory;


#endif //PLATFORM_H
