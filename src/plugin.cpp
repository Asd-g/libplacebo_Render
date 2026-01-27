#include <iostream>

#include "libplacebo_render.h"
#include "params.h"

const char* AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment* env)
{
    static constexpr int REQUIRED_INTERFACE_VERSION{9};
    static constexpr int REQUIRED_BUGFIX_VERSION{2};
    static constexpr std::string_view required_functions_storage[]{
        "avs_pool_free",           // avs loader helper functions
        "avs_release_clip",        // avs loader helper functions
        "avs_release_value",       // avs loader helper functions
        "avs_release_video_frame", // avs loader helper functions
        "avs_take_clip",           // avs loader helper functions
        "avs_add_function",
        "avs_new_c_filter",
        "avs_new_video_frame_p",
        "avs_set_to_clip",
        "avs_get_frame",
        "avs_get_row_size_p",
        "avs_get_height_p",
        "avs_get_pitch_p",
        "avs_get_video_info",
        "avs_get_read_ptr_p",
        "avs_get_write_ptr_p",
        "avs_num_components",
        "avs_bit_blt",
        "avs_bits_per_component",
        "avs_prop_set_int",
        "avs_get_frame_props_rw",
        "avs_is_420",
        "avs_is_422",
        "avs_get_plane_width_subsampling",
        "avs_get_plane_height_subsampling",
        "avs_component_size",
        "avs_get_frame_props_ro",
        "avs_prop_get_int",
        "avs_prop_get_float",
        "avs_prop_get_float_array",
        "avs_prop_num_elements",
        "avs_prop_get_data",
        "avs_prop_get_data_size",
        "avs_prop_set_float",
        "avs_prop_set_float_array",
        "avs_prop_delete_key",
        "avs_check_version",
        "avs_get_env_property",
        "avs_get_parity",
    };
    static constexpr std::span<const std::string_view> required_functions{required_functions_storage};

    if (!avisynth_c_api_loader::get_api(env, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, required_functions))
    {
        std::cerr << avisynth_c_api_loader::get_last_error() << std::endl;
        return avisynth_c_api_loader::get_last_error();
    }

    static std::string avs_signature{};
    for (const auto& p : filter_params)
    {
        if (p.optional)
        {
            avs_signature += "[";
            avs_signature += p.name;
            avs_signature += "]";
        }
        avs_signature += p.type;
    }
    g_avs_api->avs_add_function(env, "libplacebo_Render", avs_signature.c_str(), create_render, 0);

    return "AviSynth+ libplacebo interface";
}
