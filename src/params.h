#pragma once

#include <array>
#include <string>
#include <string_view>

struct param_def
{
    std::string_view name;
    std::string_view type;
    bool optional{true};
};

inline constexpr std::array filter_params{
    param_def{"clip", "c", false},
    param_def{"preset", "s"},

    // --- GEOMETRY ---
    param_def{"width", "i"},
    param_def{"height", "i"},
    param_def{"src_left", "f"},
    param_def{"src_top", "f"},
    param_def{"src_width", "f"},
    param_def{"src_height", "f"},

    param_def{"aspect_mode", "s"},

    // --- CUSTOM SHADER ---
    param_def{"custom_shader_path", "s"},
    param_def{"custom_shader_param", "s"},

    // --- SCALER ---
    param_def{"upscaler", "s"},
    param_def{"upscaler_kernel", "s"},
    param_def{"upscaler_window", "s"},
    param_def{"upscaler_radius", "f"},
    param_def{"upscaler_clamp", "f"},
    param_def{"upscaler_taper", "f"},
    param_def{"upscaler_blur", "f"},
    param_def{"upscaler_antiring", "f"},
    param_def{"upscaler_param1", "f"},
    param_def{"upscaler_param2", "f"},
    param_def{"upscaler_wparam1", "f"},
    param_def{"upscaler_wparam2", "f"},
    param_def{"upscaler_polar", "b"},

    param_def{"downscaler", "s"},
    param_def{"downscaler_kernel", "s"},
    param_def{"downscaler_window", "s"},
    param_def{"downscaler_radius", "f"},
    param_def{"downscaler_clamp", "f"},
    param_def{"downscaler_taper", "f"},
    param_def{"downscaler_blur", "f"},
    param_def{"downscaler_antiring", "f"},
    param_def{"downscaler_param1", "f"},
    param_def{"downscaler_param2", "f"},
    param_def{"downscaler_wparam1", "f"},
    param_def{"downscaler_wparam2", "f"},
    param_def{"downscaler_polar", "b"},

    param_def{"plane_upscaler", "s"},
    param_def{"plane_upscaler_kernel", "s"},
    param_def{"plane_upscaler_window", "s"},
    param_def{"plane_upscaler_radius", "f"},
    param_def{"plane_upscaler_clamp", "f"},
    param_def{"plane_upscaler_taper", "f"},
    param_def{"plane_upscaler_blur", "f"},
    param_def{"plane_upscaler_antiring", "f"},
    param_def{"plane_upscaler_param1", "f"},
    param_def{"plane_upscaler_param2", "f"},
    param_def{"plane_upscaler_wparam1", "f"},
    param_def{"plane_upscaler_wparam2", "f"},
    param_def{"plane_upscaler_polar", "b"},

    param_def{"plane_downscaler", "s"},
    param_def{"plane_downscaler_kernel", "s"},
    param_def{"plane_downscaler_window", "s"},
    param_def{"plane_downscaler_radius", "f"},
    param_def{"plane_downscaler_clamp", "f"},
    param_def{"plane_downscaler_taper", "f"},
    param_def{"plane_downscaler_blur", "f"},
    param_def{"plane_downscaler_antiring", "f"},
    param_def{"plane_downscaler_param1", "f"},
    param_def{"plane_downscaler_param2", "f"},
    param_def{"plane_downscaler_wparam1", "f"},
    param_def{"plane_downscaler_wparam2", "f"},
    param_def{"plane_downscaler_polar", "b"},

    param_def{"antiringing_strength", "f"},

    // --- LINEAR LIGHT / SIGMOID ---
    param_def{"linear_scaling", "b"},
    param_def{"sigmoid", "b"},
    param_def{"sigmoid_center", "f"},
    param_def{"sigmoid_slope", "f"},

    // --- DEBAND ---
    param_def{"deband", "b"},
    param_def{"deband_iterations", "i"},
    param_def{"deband_threshold", "f"},
    param_def{"deband_radius", "f"},
    param_def{"deband_grain", "f"},
    // param_def{"deband_grain_neutral", "f*"},

    // --- COLOR SPACE ---
    param_def{"src_csp", "s"},
    param_def{"src_matrix", "s"},
    param_def{"src_trc", "s"},
    param_def{"src_prim", "s"},
    param_def{"src_levels", "s"},
    param_def{"src_alpha", "s"},
    param_def{"src_cplace", "s"},

    param_def{"dst_csp", "s"},
    param_def{"dst_matrix", "s"},
    param_def{"dst_trc", "s"},
    param_def{"dst_prim", "s"},
    param_def{"dst_levels", "s"},
    param_def{"dst_alpha", "s"},
    param_def{"dst_cplace", "s"},

    param_def{"color_map_preset", "s"},

    // --- GAMUT MAPPING ---
    param_def{"gamut_mapping", "s"},
    param_def{"perceptual_deadzone", "f"},
    param_def{"perceptual_strength", "f"},
    param_def{"colorimetric_gamma", "f"},
    param_def{"softclip_knee", "f"},
    param_def{"softclip_desat", "f"},
    param_def{"lut3d_size_i", "i"},
    param_def{"lut3d_size_c", "i"},
    param_def{"lut3d_size_h", "i"},
    param_def{"lut3d_tricubic", "b"},
    param_def{"gamut_expansion", "b"},

    // --- TONE MAPPING ---
    param_def{"tone_mapping_function", "s"},
    param_def{"tone_constants", "s*"},
    param_def{"inverse_tone_mapping", "b"},
    param_def{"tone_lut_size", "i"},
    param_def{"contrast_recovery", "f"},
    param_def{"contrast_smoothness", "f"},

    param_def{"peak_detect", "b"},
    param_def{"peak_detection_preset", "s"},
    param_def{"peak_smoothing_period", "f"},
    param_def{"scene_threshold_low", "f"},
    param_def{"scene_threshold_high", "f"},
    param_def{"peak_percentile", "f"},
    param_def{"black_cutoff", "f"},

    param_def{"src_max", "f"},
    param_def{"src_min", "f"},
    param_def{"dst_max", "f"},
    param_def{"dst_min", "f"},

    param_def{"tone_map_metadata", "s"},
    param_def{"dovi_metadata", "b"},

    // --- COLOR ADJUSTMENT ---
    param_def{"brightness", "f"},
    param_def{"contrast", "f"},
    param_def{"saturation", "f"},
    param_def{"hue", "f"},
    param_def{"gamma", "f"},
    param_def{"temperature", "f"},

    // --- DEINTERLACE ---
    param_def{"deinterlace_algo", "s"},
    param_def{"field", "i"},
    param_def{"spatial_check", "b"},

    // --- DITHER ---
    param_def{"dither", "b"},
    param_def{"dither_method", "s"},
    param_def{"dither_lut_size", "i"},
    param_def{"dither_temporal", "b"},
    param_def{"error_diffusion_k", "s"},

    // --- DEBUG ---
    param_def{"visualize_lut", "b"},
    param_def{"visualize_lut_x0", "f"},
    param_def{"visualize_lut_y0", "f"},
    param_def{"visualize_lut_x1", "f"},
    param_def{"visualize_lut_y1", "f"},
    param_def{"visualize_hue", "f"},
    param_def{"visualize_theta", "f"},

    param_def{"show_clipping", "b"},

    // --- GLOBAL OUTPUT ---
    param_def{"out_fmt", "s"},
    param_def{"lut", "s"},
    param_def{"lut_type", "s"},
    param_def{"border", "s"},
    param_def{"border_color", "f*"},
    param_def{"background_transparency", "f"},
    param_def{"blur_radius", "f"},
    param_def{"corner_rounding", "f"},
    param_def{"device", "i"},
    param_def{"list_devices", "b"},
    param_def{"cache_path", "s"},
};

template<size_t N>
struct string_literal
{
    constexpr string_literal(const char (&str)[N])
    {
        std::copy_n(str, N, value);
    }
    char value[N];
};

template<string_literal Name>
consteval int get_param_idx() noexcept
{
    for (int i{0}; i < static_cast<int>(filter_params.size()); ++i)
    {
        if (filter_params[i].name == Name.value)
            return i;
    }
    return -1;
}
