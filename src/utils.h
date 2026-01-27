#pragma once

#include <memory>

extern "C" {
#include "libdovi/rpu_parser.h"

#include "libplacebo/cache.h"
#include "libplacebo/renderer.h"
#include "libplacebo/vulkan.h"
}

struct pl_log_deleter
{
    void operator()(const pl_log_t* ptr) const noexcept
    {
        pl_log_destroy(&ptr);
    }
};

struct pl_vulkan_deleter
{
    void operator()(const pl_vulkan_t* ptr) const noexcept
    {
        pl_vulkan_destroy(&ptr);
    }
};

struct pl_dispatch_deleter
{
    void operator()(pl_dispatch_t* ptr) const noexcept
    {
        pl_dispatch_destroy(&ptr);
    }
};

struct pl_renderer_deleter
{
    void operator()(pl_renderer_t* ptr) const noexcept
    {
        pl_renderer_destroy(&ptr);
    }
};

struct pl_cache_deleter
{
    void operator()(const pl_cache_t* ptr) const noexcept
    {
        pl_cache_destroy(&ptr);
    }
};

using pl_log_ptr = std::unique_ptr<const pl_log_t, pl_log_deleter>;
using pl_vulkan_ptr = std::unique_ptr<const pl_vulkan_t, pl_vulkan_deleter>;
using pl_dispatch_ptr = std::unique_ptr<pl_dispatch_t, pl_dispatch_deleter>;
using pl_renderer_ptr = std::unique_ptr<pl_renderer_t, pl_renderer_deleter>;
using pl_cache_ptr = std::unique_ptr<const pl_cache_t, pl_cache_deleter>;

struct pl_hook_deleter
{
    void operator()(const pl_hook* hook) const noexcept
    {
        if (hook)
            pl_mpv_user_shader_destroy(&hook);
    }
};

struct pl_custom_lut_deleter
{
    void operator()(const pl_custom_lut* lut) const noexcept
    {
        if (lut)
        {
            pl_custom_lut* mutable_lut{const_cast<pl_custom_lut*>(lut)};
            pl_lut_free(&mutable_lut);
        }
    }
};

using pl_hook_ptr = std::unique_ptr<const pl_hook, pl_hook_deleter>;
using pl_custom_lut_ptr = std::unique_ptr<const pl_custom_lut, pl_custom_lut_deleter>;

struct DoviDeleter
{
    void operator()(DoviRpuOpaque* p) const
    {
        dovi_rpu_free(p);
    }
    void operator()(const DoviRpuDataHeader* p) const
    {
        dovi_rpu_free_header(p);
    }
    void operator()(const DoviRpuDataMapping* p) const
    {
        dovi_rpu_free_data_mapping(p);
    }
    void operator()(const DoviVdrDmData* p) const
    {
        dovi_rpu_free_vdr_dm_data(p);
    }
};

template<typename T>
using dovi_ptr = std::unique_ptr<T, DoviDeleter>;
