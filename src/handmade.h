// Mock handmade.h for asset system
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef int32_t i32;  // Alias for s32 (some code uses i32)
typedef float f32;
typedef double f64;
typedef int32_t b32;
typedef size_t memory_index;

#define internal static
#define MEGABYTES(x) ((x) * 1024 * 1024)

#define Pi32 3.14159265358979323846f
#define Minimum(a, b) ((a) < (b) ? (a) : (b))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))
#define Align16(value) (((value) + 15) & ~15)

typedef struct arena arena;
typedef struct platform_state platform_state;
typedef struct work_queue work_queue;
typedef struct file_watcher file_watcher;

void platform_log(platform_state* platform, char* format, ...);
arena* arena_create(platform_state* platform, u64 size);
void* arena_push_size(arena* arena, u64 size, u32 alignment);
void arena_destroy(arena* arena);
work_queue* work_queue_create(platform_state* platform, u32 thread_count);
void work_queue_destroy(work_queue* queue);
file_watcher* file_watcher_create(platform_state* platform);
void file_watcher_destroy(file_watcher* watcher);

#define arena_push_struct(arena, type) (type*)arena_push_size(arena, sizeof(type), 16)
