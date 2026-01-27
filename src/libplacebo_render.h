#pragma once

#include <array>
#include <sstream>

#include "avs_c_api_loader.hpp"
#include "utils.h"

extern "C" {
#include "libplacebo/dispatch.h"
#include "libplacebo/shaders.h"
#include "libplacebo/utils/upload.h"
}

std::unique_ptr<struct priv> avs_libplacebo_init(const VkPhysicalDevice& device, std::string& err_msg);

[[maybe_unused]]
AVS_Value devices_info(AVS_Clip* clip, AVS_ScriptEnvironment* env, std::vector<VkPhysicalDevice>& devices, VkInstance& inst,
    std::string& msg, const std::string& name, const int device, const int list_device);

struct cached_frame
{
    int frame_idx{-1};
    uint64_t last_used{};
    std::array<pl_tex, 4> planes{};
};

struct priv
{
    pl_log_ptr log;
    pl_vulkan_ptr vk;

    pl_cache_ptr cache_obj;

    pl_dispatch_ptr dp;
    pl_renderer_ptr rr;

    static constexpr size_t CACHE_SIZE{8};
    std::array<cached_frame, CACHE_SIZE> cache;
    uint64_t timer{};

    std::array<pl_tex, 4> tex_out;
    pl_tex fix_fbo_in;
    pl_tex fix_fbo_out;

    std::ostringstream log_buffer;

    ~priv()
    {
        auto& gpu{vk->gpu};
        if (gpu)
        {
            pl_tex_destroy(gpu, &fix_fbo_out);
            pl_tex_destroy(gpu, &fix_fbo_in);
            for (auto& tex : tex_out)
                pl_tex_destroy(gpu, &tex);

            for (auto& entry : cache)
            {
                for (auto& tex : entry.planes)
                    pl_tex_destroy(gpu, &tex);
            }
        }
    }
};

AVS_Value AVSC_CC create_render(AVS_ScriptEnvironment* env, AVS_Value args, void* param);
std::unique_ptr<pl_dovi_metadata> create_dovi_meta(DoviRpuOpaque* rpu, const DoviRpuDataHeader& hdr);
