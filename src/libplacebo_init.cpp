#include <format>

#include "libplacebo_render.h"

static_assert(PL_API_VER >= 358, "libplacebo version must be at least v7.358.0.");

static void pl_logging_cb(void* log_priv, pl_log_level level, const char* msg) noexcept
{
    if (!log_priv)
        return;

    auto* p{static_cast<priv*>(log_priv)};
    p->log_buffer << std::format("[libplacebo] {}\n", msg);

    if (level <= PL_LOG_WARN)
        std::fputs(std::format("[libplacebo] {}\n", msg).c_str(), stderr);
}

std::unique_ptr<priv> avs_libplacebo_init(vk_inst_ptr& inst, const VkPhysicalDevice device, std::string& err_msg)
{
    std::unique_ptr<priv> p{std::make_unique<priv>()};
    p->vk_inst = std::move(inst);

    const pl_log_params log_params{
        .log_cb = pl_logging_cb,
        .log_priv = p.get(),
        .log_level = PL_LOG_ERR,
    };
    p->log.reset(pl_log_create(PL_API_VER, &log_params));

    pl_vulkan_params vp{};
    vp.instance = p->vk_inst.get();
    vp.device = device;
    vp.allow_software = true;
    vp.max_api_version = PL_VK_MIN_VERSION;

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

std::optional<std::string> devices_info(
    AVS_Clip* clip, AVS_ScriptEnvironment* env, std::vector<VkPhysicalDevice>& devices, vk_inst_ptr& inst, int& device, int list_devices)
{
    uint32_t instance_version{VK_API_VERSION_1_0};
    if (vkEnumerateInstanceVersion != nullptr)
        vkEnumerateInstanceVersion(&instance_version);
    if (instance_version < PL_VK_MIN_VERSION)
        return std::format("libplacebo_Render: Vulkan instance version too low `{}` (needs {}+). Update drivers/runtime.", instance_version,
            PL_VK_MIN_VERSION);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = instance_version;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app_info;

    VkInstance raw_inst{};
    if (vkCreateInstance(&info, nullptr, &raw_inst))
        return "libplacebo_Render: failed to create instance.";
    inst.reset(raw_inst);

    uint32_t dev_count{0};
    if (vkEnumeratePhysicalDevices(inst.get(), &dev_count, nullptr))
        return "libplacebo_Render: failed to get devices number.";
    if (device < -1 || device > static_cast<int>(dev_count) - 1)
        return std::format("libplaebo_Render: device must be between -1 and {}.", dev_count - 1);

    devices.resize(dev_count);
    if (vkEnumeratePhysicalDevices(inst.get(), &dev_count, devices.data()))
        return "libplaebo_Render: failed to get devices.";

    if (device == -1 || list_devices)
    {
        auto score_of{[](VkPhysicalDevice d) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(d, &props);

            switch (props.deviceType)
            {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    return 10;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    return 5;
                default:
                    return 0;
            }
        }};

        auto it{std::ranges::max_element(devices, std::less<>{}, score_of)};
        device = (it != devices.end()) ? static_cast<int>(std::ranges::distance(devices.begin(), it)) : 0;
    }

    return std::nullopt;
}
