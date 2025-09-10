#include "handmade_threading.h"
#include "simple_gui.h"
#include "handmade_assets.h"
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Parallel rendering command buffer
typedef struct RenderCommand {
    enum {
        RENDER_CMD_CLEAR,
        RENDER_CMD_DRAW_MESH,
        RENDER_CMD_DRAW_SPRITE,
        RENDER_CMD_DRAW_TEXT,
        RENDER_CMD_SET_SHADER,
        RENDER_CMD_SET_TEXTURE,
    } type;
    
    union {
        struct {
            f32 r, g, b, a;
        } clear;
        
        struct {
            u32 mesh_id;
            f32 transform[16];
        } mesh;
        
        struct {
            u32 texture_id;
            f32 x, y, w, h;
        } sprite;
        
        struct {
            char text[256];
            f32 x, y;
            u32 color;
        } text;
    };
} RenderCommand;

typedef struct RenderCommandBuffer {
    RenderCommand commands[4096];
    atomic_uint command_count;
    atomic_uint read_index;
} RenderCommandBuffer;

// Async asset loading context
typedef struct AsyncAssetContext {
    char filepath[256];
    void* data;
    usize size;
    AssetType type;
    atomic_bool ready;
    atomic_bool error;
} AsyncAssetContext;

// Thread-safe GUI update queue
typedef struct GUIUpdateCommand {
    enum {
        GUI_UPDATE_LABEL,
        GUI_UPDATE_SLIDER,
        GUI_UPDATE_CHECKBOX,
        GUI_UPDATE_WINDOW_POS,
        GUI_UPDATE_PROGRESS,
    } type;
    
    u32 widget_id;
    union {
        char text[128];
        f32 value;
        bool checked;
        struct { f32 x, y; } pos;
    };
} GUIUpdateCommand;

typedef struct GUIUpdateQueue {
    GUIUpdateCommand commands[256];
    atomic_uint write_index;
    atomic_uint read_index;
} GUIUpdateQueue;

// Global threading context
static struct {
    ThreadPool* main_pool;
    ThreadPool* render_pool;
    RenderCommandBuffer* command_buffers[4];  // Multiple buffers for parallel generation
    GUIUpdateQueue gui_queue;
    
    // Statistics
    atomic_uint frames_rendered;
    atomic_ulong total_frame_time_us;
    atomic_uint assets_loading;
    atomic_uint assets_loaded;
} g_threading_context;

// Initialize threading system
bool threading_init(MemoryArena* arena) {
    // Create main thread pool (all cores)
    u32 core_count = get_cpu_count();
    g_threading_context.main_pool = thread_pool_create(core_count, arena);
    
    // Create render thread pool (half cores for rendering)
    g_threading_context.render_pool = thread_pool_create(core_count / 2, arena);
    
    // Allocate command buffers
    for (u32 i = 0; i < 4; i++) {
        g_threading_context.command_buffers[i] = (RenderCommandBuffer*)
            (arena->base + arena->used);
        arena->used += sizeof(RenderCommandBuffer);
        memset(g_threading_context.command_buffers[i], 0, sizeof(RenderCommandBuffer));
    }
    
    // Initialize GUI queue
    atomic_store(&g_threading_context.gui_queue.write_index, 0);
    atomic_store(&g_threading_context.gui_queue.read_index, 0);
    
    printf("Threading system initialized: %u worker threads, %u render threads\n",
           g_threading_context.main_pool->thread_count,
           g_threading_context.render_pool->thread_count);
    
    return true;
}

// Parallel render command generation
typedef struct RenderJobData {
    RenderCommandBuffer* buffer;
    void* scene_data;
    u32 object_start;
    u32 object_count;
} RenderJobData;

static void generate_render_commands_job(void* data, u32 thread_index) {
    RenderJobData* job_data = (RenderJobData*)data;
    RenderCommandBuffer* buffer = job_data->buffer;
    
    // Generate commands for subset of scene objects
    for (u32 i = 0; i < job_data->object_count; i++) {
        u32 cmd_index = atomic_fetch_add(&buffer->command_count, 1);
        if (cmd_index >= 4096) break;
        
        RenderCommand* cmd = &buffer->commands[cmd_index];
        
        // Example: Generate mesh draw command
        cmd->type = RENDER_CMD_DRAW_MESH;
        cmd->mesh.mesh_id = job_data->object_start + i;
        
        // Simple transform (would come from scene data)
        memset(cmd->mesh.transform, 0, sizeof(cmd->mesh.transform));
        cmd->mesh.transform[0] = 1.0f;
        cmd->mesh.transform[5] = 1.0f;
        cmd->mesh.transform[10] = 1.0f;
        cmd->mesh.transform[15] = 1.0f;
    }
}

// Generate render commands in parallel
void parallel_generate_render_commands(void* scene_data, u32 object_count) {
    // Reset command buffer
    RenderCommandBuffer* buffer = g_threading_context.command_buffers[0];
    atomic_store(&buffer->command_count, 0);
    atomic_store(&buffer->read_index, 0);
    
    // Split work across threads
    u32 objects_per_job = (object_count + 3) / 4;
    
    Job* jobs[4];
    RenderJobData job_data[4];
    
    for (u32 i = 0; i < 4 && i * objects_per_job < object_count; i++) {
        job_data[i].buffer = buffer;
        job_data[i].scene_data = scene_data;
        job_data[i].object_start = i * objects_per_job;
        job_data[i].object_count = (i == 3) ? 
            (object_count - i * objects_per_job) : objects_per_job;
        
        jobs[i] = thread_pool_submit_job(g_threading_context.render_pool,
                                         generate_render_commands_job,
                                         &job_data[i],
                                         JOB_PRIORITY_HIGH);
    }
    
    // Wait for all command generation to complete
    for (u32 i = 0; i < 4 && i * objects_per_job < object_count; i++) {
        thread_pool_wait_for_job(g_threading_context.render_pool, jobs[i]);
    }
}

// Execute render commands (on render thread)
void execute_render_commands(RenderCommandBuffer* buffer) {
    u32 count = atomic_load(&buffer->command_count);
    
    for (u32 i = 0; i < count; i++) {
        RenderCommand* cmd = &buffer->commands[i];
        
        switch (cmd->type) {
            case RENDER_CMD_CLEAR:
                glClearColor(cmd->clear.r, cmd->clear.g, cmd->clear.b, cmd->clear.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                break;
                
            case RENDER_CMD_DRAW_MESH:
                // Draw mesh (would use actual mesh rendering)
                glPushMatrix();
                glMultMatrixf(cmd->mesh.transform);
                // ... draw mesh cmd->mesh.mesh_id
                glPopMatrix();
                break;
                
            case RENDER_CMD_DRAW_SPRITE:
                // Draw sprite
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, cmd->sprite.texture_id);
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(cmd->sprite.x, cmd->sprite.y);
                glTexCoord2f(1, 0); glVertex2f(cmd->sprite.x + cmd->sprite.w, cmd->sprite.y);
                glTexCoord2f(1, 1); glVertex2f(cmd->sprite.x + cmd->sprite.w, cmd->sprite.y + cmd->sprite.h);
                glTexCoord2f(0, 1); glVertex2f(cmd->sprite.x, cmd->sprite.y + cmd->sprite.h);
                glEnd();
                glDisable(GL_TEXTURE_2D);
                break;
                
            case RENDER_CMD_DRAW_TEXT:
                // Would render text here
                break;
                
            case RENDER_CMD_SET_SHADER:
                // Would set shader here
                break;
                
            case RENDER_CMD_SET_TEXTURE:
                // Would bind texture here
                break;
        }
    }
}

// Async asset loading
static void load_asset_job(void* data, u32 thread_index) {
    AsyncAssetContext* ctx = (AsyncAssetContext*)data;
    
    // Simulate loading from disk
    FILE* file = fopen(ctx->filepath, "rb");
    if (!file) {
        atomic_store(&ctx->error, true);
        atomic_store(&ctx->ready, true);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    ctx->size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate from thread-local temp memory
    ThreadContext* thread_ctx = tls_current_context;
    ctx->data = thread_pool_alloc_temp(thread_ctx, ctx->size);
    
    if (!ctx->data) {
        fclose(file);
        atomic_store(&ctx->error, true);
        atomic_store(&ctx->ready, true);
        return;
    }
    
    fread(ctx->data, 1, ctx->size, file);
    fclose(file);
    
    // Process based on type
    switch (ctx->type) {
        case ASSET_TYPE_TEXTURE:
            // Would decode texture here
            break;
        case ASSET_TYPE_MESH:
            // Would parse mesh here
            break;
        case ASSET_TYPE_SHADER:
            // Would compile shader here
            break;
        default:
            break;
    }
    
    atomic_fetch_add(&g_threading_context.assets_loaded, 1);
    atomic_store(&ctx->ready, true);
}

// Start async asset load
AsyncAssetContext* async_load_asset(const char* filepath, AssetType type) {
    // Allocate context (would use proper allocator)
    static AsyncAssetContext contexts[256];
    static u32 next_context = 0;
    
    AsyncAssetContext* ctx = &contexts[next_context++ & 255];
    strncpy(ctx->filepath, filepath, sizeof(ctx->filepath) - 1);
    ctx->type = type;
    atomic_store(&ctx->ready, false);
    atomic_store(&ctx->error, false);
    
    atomic_fetch_add(&g_threading_context.assets_loading, 1);
    
    // Submit as IO-bound job
    thread_pool_submit_job_with_flags(g_threading_context.main_pool,
                                      load_asset_job,
                                      ctx,
                                      JOB_PRIORITY_NORMAL,
                                      JOB_FLAG_IO_BOUND);
    
    return ctx;
}

// Check if asset is ready
bool is_asset_ready(AsyncAssetContext* ctx) {
    return atomic_load(&ctx->ready);
}

// Thread-safe GUI updates
void gui_queue_update(GUIUpdateCommand* cmd) {
    u32 write_index = atomic_fetch_add(&g_threading_context.gui_queue.write_index, 1) & 255;
    g_threading_context.gui_queue.commands[write_index] = *cmd;
}

// Process GUI updates on main thread
void process_gui_updates(simple_gui* gui) {
    u32 read_index = atomic_load(&g_threading_context.gui_queue.read_index);
    u32 write_index = atomic_load(&g_threading_context.gui_queue.write_index);
    
    while (read_index != write_index) {
        GUIUpdateCommand* cmd = &g_threading_context.gui_queue.commands[read_index & 255];
        
        switch (cmd->type) {
            case GUI_UPDATE_LABEL:
                // Update label text
                break;
            case GUI_UPDATE_SLIDER:
                // Update slider value
                break;
            case GUI_UPDATE_CHECKBOX:
                // Update checkbox state
                break;
            case GUI_UPDATE_WINDOW_POS:
                // Update window position
                break;
            case GUI_UPDATE_PROGRESS:
                // Update progress bar
                break;
        }
        
        read_index++;
    }
    
    atomic_store(&g_threading_context.gui_queue.read_index, read_index);
}

// Parallel physics update
typedef struct PhysicsObject {
    f32 position[3];
    f32 velocity[3];
    f32 mass;
    f32 radius;
} PhysicsObject;

static void physics_update_job(void* data, u32 index, u32 thread_index) {
    PhysicsObject* objects = (PhysicsObject*)data;
    PhysicsObject* obj = &objects[index];
    
    // Simple physics update
    const f32 dt = 1.0f / 60.0f;
    const f32 gravity = -9.8f;
    
    obj->velocity[1] += gravity * dt;
    
    obj->position[0] += obj->velocity[0] * dt;
    obj->position[1] += obj->velocity[1] * dt;
    obj->position[2] += obj->velocity[2] * dt;
    
    // Ground collision
    if (obj->position[1] < obj->radius) {
        obj->position[1] = obj->radius;
        obj->velocity[1] = -obj->velocity[1] * 0.8f;  // Bounce with damping
    }
}

// Update physics for all objects in parallel
void parallel_physics_update(PhysicsObject* objects, u32 count) {
    thread_pool_parallel_for(g_threading_context.main_pool,
                             count,
                             16,  // Process 16 objects per batch
                             physics_update_job,
                             objects);
}

// Parallel frustum culling
typedef struct CullingData {
    f32 frustum_planes[6][4];
    f32* object_positions;
    f32* object_radii;
    u8* visibility_mask;
    atomic_uint visible_count;
} CullingData;

static void frustum_culling_job(void* data, u32 index, u32 thread_index) {
    CullingData* cull_data = (CullingData*)data;
    
    f32 x = cull_data->object_positions[index * 3 + 0];
    f32 y = cull_data->object_positions[index * 3 + 1];
    f32 z = cull_data->object_positions[index * 3 + 2];
    f32 radius = cull_data->object_radii[index];
    
    // Test against all frustum planes
    bool visible = true;
    for (u32 p = 0; p < 6; p++) {
        f32 distance = cull_data->frustum_planes[p][0] * x +
                      cull_data->frustum_planes[p][1] * y +
                      cull_data->frustum_planes[p][2] * z +
                      cull_data->frustum_planes[p][3];
        
        if (distance < -radius) {
            visible = false;
            break;
        }
    }
    
    cull_data->visibility_mask[index] = visible ? 1 : 0;
    if (visible) {
        atomic_fetch_add(&cull_data->visible_count, 1);
    }
}

// Perform frustum culling in parallel
u32 parallel_frustum_culling(f32 frustum_planes[6][4], f32* positions, 
                             f32* radii, u8* visibility, u32 count) {
    CullingData cull_data = {0};
    memcpy(cull_data.frustum_planes, frustum_planes, sizeof(f32) * 6 * 4);
    cull_data.object_positions = positions;
    cull_data.object_radii = radii;
    cull_data.visibility_mask = visibility;
    atomic_store(&cull_data.visible_count, 0);
    
    thread_pool_parallel_for(g_threading_context.main_pool,
                             count,
                             32,  // Process 32 objects per batch
                             frustum_culling_job,
                             &cull_data);
    
    return atomic_load(&cull_data.visible_count);
}

// Performance monitoring
void threading_print_stats(void) {
    ThreadPoolStats stats;
    thread_pool_get_stats(g_threading_context.main_pool, &stats);
    
    printf("\n=== Threading Performance ===\n");
    printf("Total jobs: %lu completed, %lu submitted\n",
           stats.total_jobs_completed, stats.total_jobs_submitted);
    printf("Average wait time: %lu ns\n", stats.average_wait_time_ns);
    printf("Active threads: %u\n", stats.active_thread_count);
    
    printf("\nPer-thread utilization:\n");
    for (u32 i = 0; i < g_threading_context.main_pool->thread_count; i++) {
        printf("  Thread %u: %.1f%% utilized, %lu jobs, %lu steals\n",
               i,
               stats.thread_utilization[i] * 100.0f,
               stats.jobs_per_thread[i],
               stats.steal_count_per_thread[i]);
    }
    
    u32 frames = atomic_load(&g_threading_context.frames_rendered);
    u64 total_time = atomic_load(&g_threading_context.total_frame_time_us);
    if (frames > 0) {
        printf("\nRendering: %u frames, avg %.2f ms/frame\n",
               frames, (f32)total_time / (f32)frames / 1000.0f);
    }
    
    printf("Assets: %u loaded, %u loading\n",
           atomic_load(&g_threading_context.assets_loaded),
           atomic_load(&g_threading_context.assets_loading));
    
    printf("=============================\n");
}

// Cleanup
void threading_shutdown(void) {
    if (g_threading_context.main_pool) {
        thread_pool_destroy(g_threading_context.main_pool);
    }
    if (g_threading_context.render_pool) {
        thread_pool_destroy(g_threading_context.render_pool);
    }
}