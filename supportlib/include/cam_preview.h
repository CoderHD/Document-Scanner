#include "types.h"
#include "backend.h"
#include "assets.h"
#include "nn.h"

struct ACameraDevice;
struct ANativeWindow;

NAMESPACE_BEGIN

struct cam_preview {
    shader_program preview_program;
    ACameraDevice* cam;

    shader_buffer cam_quad_buffer;

    u32 nn_input_buffer_size, nn_output_buffer_size;
    u8* nn_input_buffer, *nn_output_buffer;

    texture* nn_input_tex;
    texture nn_output_tex;

    neural_network nn;

    uvec2 cam_tex_size;
    uvec2 preview_size;
    f32 cam_tex_left, cam_tex_right;

    u64 last_time;
    
    texture_downsampler tex_downsampler;

    bool is_init;

    void pre_init(int* cam_width, int* cam_height);
    void init_backend(uvec2 preview_size, file_context* file_ctx);
    void init_cam(ANativeWindow* texture_window);
    void render();
};

NAMESPACE_END