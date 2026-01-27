#include <format>

#include "libplacebo_render.h"

static_assert(PL_API_VER >= 351, "libplacebo version must be at least v7.351.0.");

static void pl_logging_cb(void* log_priv, pl_log_level level, const char* msg) noexcept
{
    if (!log_priv)
        return;

    auto* p{static_cast<priv*>(log_priv)};
    p->log_buffer << std::format("[libplacebo] {}\n", msg);

    if (level <= PL_LOG_WARN)
        std::fputs(std::format("[libplacebo] {}\n", msg).c_str(), stderr);
}

std::unique_ptr<priv> avs_libplacebo_init(const VkPhysicalDevice& device, std::string& err_msg)
{
    std::unique_ptr<priv> p{std::make_unique<priv>()};
    const pl_log_params log_params{
        .log_cb = pl_logging_cb,
        .log_priv = p.get(),
        .log_level = PL_LOG_ERR,
    };
    p->log.reset(pl_log_create(PL_API_VER, &log_params));

    pl_vulkan_params vp{};
    vp.allow_software = true;
    if (device)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        vp.device_name = properties.deviceName;
    }

    p->vk.reset(pl_vulkan_create(p->log.get(), &vp));
    if (!p->vk)
    {
        err_msg = p->log_buffer.str();
        return nullptr;
    }

    const auto& gpu{p->vk->gpu};
    // 50MB limit should be reasonable default
    pl_cache_params cache_params{
        .log = p->log.get(),
        .max_total_size = 50 * 1024 * 1024,
    };
    p->cache_obj.reset(pl_cache_create(&cache_params));
    pl_gpu_set_cache(gpu, p->cache_obj.get());

    p->dp.reset(pl_dispatch_create(p->log.get(), gpu));
    if (!p->dp)
    {
        err_msg = p->log_buffer.str();
        return nullptr;
    }

    p->rr.reset(pl_renderer_create(p->log.get(), gpu));
    if (!p->rr)
    {
        err_msg = p->log_buffer.str();
        return nullptr;
    }
    return p;
}

AVS_Value devices_info(AVS_Clip* clip, AVS_ScriptEnvironment* env, std::vector<VkPhysicalDevice>& devices, VkInstance& inst,
    std::string& msg, const std::string& name, const int device, const int list_device)
{
    static constexpr uint32_t min_vk_ver{PL_VK_MIN_VERSION};

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = min_vk_ver;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app_info;

    uint32_t instance_version{VK_API_VERSION_1_0};
    if (vkEnumerateInstanceVersion != nullptr)
    {
        vkEnumerateInstanceVersion(&instance_version);
    }
    if (instance_version < min_vk_ver)
    {
        msg = name + ": Vulkan instance version too low (needs 1.2+). Update drivers/runtime.";
        return avs_new_value_error(msg.c_str());
    }
    if (vkCreateInstance(&info, nullptr, &inst))
    {
        msg = name + ": failed to create instance.";
        return avs_new_value_error(msg.c_str());
    }

    uint32_t dev_count{0};
    if (vkEnumeratePhysicalDevices(inst, &dev_count, nullptr))
    {
        vkDestroyInstance(inst, nullptr);
        msg = name + ": failed to get devices number.";
        return avs_new_value_error(msg.c_str());
    }
    if (device < -1 || device > static_cast<int>(dev_count) - 1)
    {
        vkDestroyInstance(inst, nullptr);
        msg = name + ": device must be between -1 and " + std::to_string(dev_count - 1);
        return avs_new_value_error(msg.c_str());
    }

    devices.resize(dev_count);
    if (vkEnumeratePhysicalDevices(inst, &dev_count, devices.data()))
    {
        vkDestroyInstance(inst, nullptr);
        msg = name + ": failed to get devices.";
        return avs_new_value_error(msg.c_str());
    }
    if (list_device)
    {
        for (size_t i{0}; i < devices.size(); ++i)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            msg += std::to_string(i) + ": " + std::string(properties.deviceName) + "\n";
        }
        vkDestroyInstance(inst, nullptr);

        AVS_Value cl;
        g_avs_api->avs_set_to_clip(&cl, clip);
        avs_helpers::avs_value_guard cl_guard(cl);
        AVS_Value args_[2]{cl_guard.get(), avs_new_value_string(msg.c_str())};
        AVS_Value inv{g_avs_api->avs_invoke(env, "Text", avs_new_value_array(args_, 2), 0)};

        return inv;
    }
    else
    {
        return avs_void;
    }
}
