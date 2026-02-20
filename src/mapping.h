#pragma once

#include <algorithm>
#include <ranges>

#include "libplacebo_render.h"

constexpr unsigned char ascii_tolower(unsigned char c) noexcept
{
    return c + (c >= 'A' && c <= 'Z') * 32;
}

bool iequals(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, [](char ca, char cb) { return ascii_tolower(ca) == ascii_tolower(cb); });
}

template<typename Key, typename Value, std::size_t Size>
struct Map
{
    struct Entry
    {
        Key key;
        Value value;
    };
    std::array<Entry, Size> data;

    constexpr std::optional<Value> find(const Key& search_key) const noexcept
    {
        const auto it{[&]() {
            if constexpr (std::is_convertible_v<Key, std::string_view>)
                return std::ranges::find_if(data, [&](const auto& entry) { return iequals(search_key, entry.key); });
            else
                return std::ranges::find(data, search_key, &Entry::key);
        }()};

        return (it != data.end()) ? std::make_optional(it->value) : std::nullopt;
    }

    constexpr std::optional<Key> find_value(const Value& search_value) const noexcept
    {
        const auto it{[&]() {
            if constexpr (std::is_convertible_v<Value, std::string_view>)
                return std::ranges::find_if(data, [&](const auto& entry) { return iequals(search_value, entry.value); });
            else
                return std::ranges::find(data, search_value, &Entry::value);
        }()};

        return (it != data.end()) ? std::make_optional(it->key) : std::nullopt;
    }
};

// Matrix
inline constexpr Map<pl_color_system, int64_t, 13> map_libpl_avs_matrix{{{
    {PL_COLOR_SYSTEM_RGB, 0},
    {PL_COLOR_SYSTEM_XYZ, 0},
    {PL_COLOR_SYSTEM_BT_709, 1},
    {PL_COLOR_SYSTEM_BT_601, 5},
    {PL_COLOR_SYSTEM_BT_601, 6},
    {PL_COLOR_SYSTEM_SMPTE_240M, 7},
    {PL_COLOR_SYSTEM_YCGCO, 8},
    {PL_COLOR_SYSTEM_BT_2020_NC, 9},
    {PL_COLOR_SYSTEM_BT_2020_C, 10},
    {PL_COLOR_SYSTEM_BT_2100_PQ, 14},
    {PL_COLOR_SYSTEM_BT_2100_HLG, 14},
    {PL_COLOR_SYSTEM_YCGCO_RE, 16},
    {PL_COLOR_SYSTEM_YCGCO_RO, 17},
}}};

inline constexpr Map<std::string_view, pl_color_system, 26> parse_matrix{{{
    {"601", PL_COLOR_SYSTEM_BT_601},
    {"bt601", PL_COLOR_SYSTEM_BT_601},
    {"smpte170m", PL_COLOR_SYSTEM_BT_601},

    {"709", PL_COLOR_SYSTEM_BT_709},
    {"bt709", PL_COLOR_SYSTEM_BT_709},

    {"240m", PL_COLOR_SYSTEM_SMPTE_240M},
    {"smpte240m", PL_COLOR_SYSTEM_SMPTE_240M},

    {"2020nc", PL_COLOR_SYSTEM_BT_2020_NC},
    {"2020", PL_COLOR_SYSTEM_BT_2020_NC},
    {"bt2020", PL_COLOR_SYSTEM_BT_2020_NC},
    {"bt2020nc", PL_COLOR_SYSTEM_BT_2020_NC},

    {"2020c", PL_COLOR_SYSTEM_BT_2020_C},
    {"bt2020c", PL_COLOR_SYSTEM_BT_2020_C},

    {"2100pq", PL_COLOR_SYSTEM_BT_2100_PQ},
    {"bt2100pq", PL_COLOR_SYSTEM_BT_2100_PQ},
    {"ictcp-pq", PL_COLOR_SYSTEM_BT_2100_PQ},

    {"2100hlg", PL_COLOR_SYSTEM_BT_2100_HLG},
    {"bt2100hlg", PL_COLOR_SYSTEM_BT_2100_HLG},
    {"ictcp-hlg", PL_COLOR_SYSTEM_BT_2100_HLG},

    {"dovi", PL_COLOR_SYSTEM_DOLBYVISION},
    {"dolbyvision", PL_COLOR_SYSTEM_DOLBYVISION},

    {"ycgco", PL_COLOR_SYSTEM_YCGCO},
    {"ycgco-re", PL_COLOR_SYSTEM_YCGCO_RE},
    {"ycgco-ro", PL_COLOR_SYSTEM_YCGCO_RO},

    {"rgb", PL_COLOR_SYSTEM_RGB},

    {"xyz", PL_COLOR_SYSTEM_XYZ},
}}};

// Transfer
inline constexpr Map<pl_color_transfer, int64_t, 9> map_libpl_avs_trc{{{
    {PL_COLOR_TRC_BT_1886, 1},
    {PL_COLOR_TRC_GAMMA22, 4},
    {PL_COLOR_TRC_GAMMA28, 5},
    {PL_COLOR_TRC_LINEAR, 8},
    {PL_COLOR_TRC_SRGB, 13},
    {PL_COLOR_TRC_PQ, 16},
    {PL_COLOR_TRC_ST428, 17},
    {PL_COLOR_TRC_HLG, 18},
    {PL_COLOR_TRC_PRO_PHOTO, 30},
}}};

inline constexpr Map<std::string_view, pl_color_transfer, 24> parse_trc{{{
    // SDR
    {"1886", PL_COLOR_TRC_BT_1886},
    {"bt1886", PL_COLOR_TRC_BT_1886},

    {"srgb", PL_COLOR_TRC_SRGB},

    {"linear", PL_COLOR_TRC_LINEAR},

    {"gamma1.8", PL_COLOR_TRC_GAMMA18},

    {"gamma2.0", PL_COLOR_TRC_GAMMA20},

    {"gamma2.2", PL_COLOR_TRC_GAMMA22},

    {"gamma2.4", PL_COLOR_TRC_GAMMA24},

    {"gamma2.6", PL_COLOR_TRC_GAMMA26},

    {"gamma2.8", PL_COLOR_TRC_GAMMA28},

    {"prophoto", PL_COLOR_TRC_PRO_PHOTO},

    {"st428", PL_COLOR_TRC_ST428},

    // HDR
    {"pq", PL_COLOR_TRC_PQ},
    {"st2084", PL_COLOR_TRC_PQ},

    {"hlg", PL_COLOR_TRC_HLG},
    {"arib-std-b67", PL_COLOR_TRC_HLG},

    {"vlog", PL_COLOR_TRC_V_LOG},
    {"v-log", PL_COLOR_TRC_V_LOG},

    {"slog1", PL_COLOR_TRC_S_LOG1},
    {"s-log1", PL_COLOR_TRC_S_LOG1},

    {"slog2", PL_COLOR_TRC_S_LOG2},
    {"s-log2", PL_COLOR_TRC_S_LOG2},

    // Aliases
    {"709", PL_COLOR_TRC_BT_1886},
    {"bt709", PL_COLOR_TRC_BT_1886},
}}};

// Primaries
inline constexpr Map<pl_color_primaries, int64_t, 12> map_libpl_avs_prim{{{
    {PL_COLOR_PRIM_BT_709, 1},
    {PL_COLOR_PRIM_BT_470M, 4},
    {PL_COLOR_PRIM_BT_601_625, 5},
    {PL_COLOR_PRIM_BT_601_525, 6},
    {PL_COLOR_PRIM_BT_601_525, 7},
    {PL_COLOR_PRIM_FILM_C, 8},
    {PL_COLOR_PRIM_BT_2020, 9},
    {PL_COLOR_PRIM_CIE_1931, 10},
    {PL_COLOR_PRIM_DCI_P3, 11},
    {PL_COLOR_PRIM_DISPLAY_P3, 12},
    {PL_COLOR_PRIM_EBU_3213, 22},
    {PL_COLOR_PRIM_PRO_PHOTO, 30},
}}};

inline constexpr Map<std::string_view, pl_color_primaries, 35> parse_prim{{{
    {"ntsc", PL_COLOR_PRIM_BT_601_525},
    {"601-525", PL_COLOR_PRIM_BT_601_525},
    {"bt601-525", PL_COLOR_PRIM_BT_601_525},
    {"smpte-c", PL_COLOR_PRIM_BT_601_525},

    {"pal", PL_COLOR_PRIM_BT_601_625},
    {"secam", PL_COLOR_PRIM_BT_601_625},
    {"601-625", PL_COLOR_PRIM_BT_601_625},
    {"bt601-625", PL_COLOR_PRIM_BT_601_625},

    {"709", PL_COLOR_PRIM_BT_709},
    {"bt709", PL_COLOR_PRIM_BT_709},
    {"srgb", PL_COLOR_PRIM_BT_709},

    {"470m", PL_COLOR_PRIM_BT_470M},
    {"bt470m", PL_COLOR_PRIM_BT_470M},

    {"ebu3213", PL_COLOR_PRIM_EBU_3213},

    {"2020", PL_COLOR_PRIM_BT_2020},
    {"bt2020", PL_COLOR_PRIM_BT_2020},

    {"apple", PL_COLOR_PRIM_APPLE},

    {"adobe", PL_COLOR_PRIM_ADOBE},

    {"prophoto", PL_COLOR_PRIM_PRO_PHOTO},

    {"cie1931", PL_COLOR_PRIM_CIE_1931},

    {"dci-p3", PL_COLOR_PRIM_DCI_P3},
    {"p3", PL_COLOR_PRIM_DCI_P3},

    {"display-p3", PL_COLOR_PRIM_DISPLAY_P3},
    {"p3-d65", PL_COLOR_PRIM_DISPLAY_P3},

    {"v-gamut", PL_COLOR_PRIM_V_GAMUT},
    {"vgamut", PL_COLOR_PRIM_V_GAMUT},

    {"s-gamut", PL_COLOR_PRIM_S_GAMUT},
    {"sgamut", PL_COLOR_PRIM_S_GAMUT},

    {"film", PL_COLOR_PRIM_FILM_C},
    {"film-c", PL_COLOR_PRIM_FILM_C},

    {"aces", PL_COLOR_PRIM_ACES_AP0},
    {"ap0", PL_COLOR_PRIM_ACES_AP0},
    {"aces-ap0", PL_COLOR_PRIM_ACES_AP0},

    {"ap1", PL_COLOR_PRIM_ACES_AP1},
    {"aces-ap1", PL_COLOR_PRIM_ACES_AP1},
}}};

// Levels
inline constexpr Map<pl_color_levels, int64_t, 2> map_libpl_avs_levels{{{
    {PL_COLOR_LEVELS_FULL, 0},
    {PL_COLOR_LEVELS_LIMITED, 1},
}}};

inline constexpr Map<std::string_view, pl_color_levels, 4> parse_levels{{{
    {"limited", PL_COLOR_LEVELS_LIMITED},
    {"tv", PL_COLOR_LEVELS_LIMITED},

    {"full", PL_COLOR_LEVELS_FULL},
    {"pc", PL_COLOR_LEVELS_FULL},
}}};

// Chroma location
inline constexpr Map<pl_chroma_location, int64_t, 6> map_libpl_avs_cplace{{{
    {PL_CHROMA_LEFT, 0},
    {PL_CHROMA_CENTER, 1},
    {PL_CHROMA_TOP_LEFT, 2},
    {PL_CHROMA_TOP_CENTER, 3},
    {PL_CHROMA_BOTTOM_LEFT, 4},
    {PL_CHROMA_BOTTOM_CENTER, 5},
}}};

inline constexpr Map<std::string_view, pl_chroma_location, 6> parse_cplace{{{
    {"left", PL_CHROMA_LEFT},
    {"center", PL_CHROMA_CENTER},
    {"top_left", PL_CHROMA_TOP_LEFT},
    {"top_center", PL_CHROMA_TOP_CENTER},
    {"bottom_left", PL_CHROMA_BOTTOM_LEFT},
    {"bottom_center", PL_CHROMA_BOTTOM_CENTER},
}}};

// Alpha plane
inline constexpr Map<std::string_view, pl_alpha_mode, 3> parse_alpha{{{
    {"independent", PL_ALPHA_INDEPENDENT},
    {"premultiplied", PL_ALPHA_PREMULTIPLIED},
    {"none", PL_ALPHA_NONE},
}}};

// Deinterlace
inline constexpr Map<pl_field, int64_t, 3> map_libpl_avs_field{{{
    {PL_FIELD_NONE, 0},
    {PL_FIELD_BOTTOM, 1},
    {PL_FIELD_TOP, 2},
}}};

inline constexpr Map<std::string_view, pl_deinterlace_algorithm, 4> parse_deint_algo{{{
    {"weave", PL_DEINTERLACE_WEAVE},
    {"bob", PL_DEINTERLACE_BOB},
    {"yadif", PL_DEINTERLACE_YADIF},
    {"bwdif", PL_DEINTERLACE_BWDIF},
}}};

inline constexpr Map<std::string_view, pl_dither_method, 4> parse_dither_m{{{
    {"blue", PL_DITHER_BLUE_NOISE},
    {"ordered_lut", PL_DITHER_ORDERED_LUT},
    {"ordered", PL_DITHER_ORDERED_FIXED},
    {"white", PL_DITHER_WHITE_NOISE},
}}};

inline constexpr Map<std::string_view, pl_hdr_metadata_type, 5> parse_hdr_metadata{{{
    {"any", PL_HDR_METADATA_ANY},
    {"none", PL_HDR_METADATA_NONE},
    {"hdr10", PL_HDR_METADATA_HDR10},
    {"hdr10plus", PL_HDR_METADATA_HDR10},
    {"cie_y", PL_HDR_METADATA_CIE_Y},
}}};

inline constexpr Map<std::string_view, std::string_view, 12> parse_error_diffusion_k{{{
    {"simple", "simple"},
    {"false-fs", "false-fs"},
    {"sierra-lite", "sierra-lite"},
    {"floyd-steinberg", "floyd-steinberg"},
    {"fs", "floyd-steinberg"}, // Alias
    {"atkinson", "atkinson"},
    {"jarvis-judice-ninke", "jarvis-judice-ninke"},
    {"jjn", "jarvis-judice-ninke"}, // Alias
    {"stucki", "stucki"},
    {"burkes", "burkes"},
    {"sierra-2", "sierra-2"},
    {"sierra-3", "sierra-3"},
}}};

inline constexpr Map<std::string_view, pl_lut_type, 4> parse_lut_type{{{
    {"unknown", PL_LUT_UNKNOWN},
    {"native", PL_LUT_NATIVE},
    {"normalized", PL_LUT_NORMALIZED},
    {"conversion", PL_LUT_CONVERSION},
}}};

inline constexpr Map<std::string_view, int, 3> parse_aspect_mode{{{
    {"stretch", 0},
    {"fit", 1},
    {"fill", 2},
}}};

inline constexpr Map<std::string_view, pl_clear_mode, 3> parse_clear_mode{{{
    {"color", PL_CLEAR_COLOR},
    //{"tiles", PL_CLEAR_TILES},
    {"skip", PL_CLEAR_SKIP},
    {"blur", PL_CLEAR_BLUR},
}}};

bool apply_csp_preset(std::string_view str, pl_color_space& csp, pl_color_repr& repr, std::string_view* out_fmt = nullptr) noexcept
{
    struct csp_preset
    {
        std::string_view name;
        pl_color_primaries prim;
        pl_color_transfer trc;
        pl_color_system sys;
        pl_color_levels levels;
        std::string_view fmt;
    };

    if (str.empty())
        return false;

    static constexpr std::array<csp_preset, 24> presets{{
        // Standard SDR (HD)
        {"sdr", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED, "YV12"},
        {"709", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED, "YV12"},
        {"bt709", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED, "YV12"},
        {"rec709", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_709, PL_COLOR_LEVELS_LIMITED, "YV12"},

        // Standard SDR (SD - NTSC)
        {"601_525", PL_COLOR_PRIM_BT_601_525, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_601, PL_COLOR_LEVELS_LIMITED, "YV12"},
        {"ntsc", PL_COLOR_PRIM_BT_601_525, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_601, PL_COLOR_LEVELS_LIMITED, "YV12"},

        // Standard SDR (SD - PAL)
        {"601_625", PL_COLOR_PRIM_BT_601_625, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_601, PL_COLOR_LEVELS_LIMITED, "YV12"},
        {"pal", PL_COLOR_PRIM_BT_601_625, PL_COLOR_TRC_BT_1886, PL_COLOR_SYSTEM_BT_601, PL_COLOR_LEVELS_LIMITED, "YV12"},

        // HDR10 (PQ)
        {"hdr10", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},
        {"pq", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},
        {"2020", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},
        {"bt2020", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},

        // HLG
        {"hlg", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_HLG, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},
        {"arib-std-b67", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_HLG, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},

        // Dolby Vision (Profile 5/8)
        {"dovi", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},
        {"dolbyvision", PL_COLOR_PRIM_BT_2020, PL_COLOR_TRC_PQ, PL_COLOR_SYSTEM_BT_2020_NC, PL_COLOR_LEVELS_LIMITED, "YUV420P10"},

        // sRGB (PC)
        {"srgb", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_SRGB, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "RGBP"},

        // JPEG (Full Range YUV)
        {"jpeg", PL_COLOR_PRIM_BT_709, PL_COLOR_TRC_SRGB, PL_COLOR_SYSTEM_BT_601, PL_COLOR_LEVELS_FULL, "RGBP"},

        // Adobe RGB
        {"adobe", PL_COLOR_PRIM_ADOBE, PL_COLOR_TRC_GAMMA22, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "YV12"},
        {"adobergb", PL_COLOR_PRIM_ADOBE, PL_COLOR_TRC_GAMMA22, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "YV12"},

        // DCI-P3 (Cinema)
        {"p3", PL_COLOR_PRIM_DCI_P3, PL_COLOR_TRC_GAMMA26, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "YV12"},
        {"dci-p3", PL_COLOR_PRIM_DCI_P3, PL_COLOR_TRC_GAMMA26, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "YV12"},

        // Display P3 (Apple/Web)
        {"display-p3", PL_COLOR_PRIM_DISPLAY_P3, PL_COLOR_TRC_SRGB, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "RGBP"},
        {"p3-d65", PL_COLOR_PRIM_DISPLAY_P3, PL_COLOR_TRC_SRGB, PL_COLOR_SYSTEM_RGB, PL_COLOR_LEVELS_FULL, "RGBP"},
    }};

    for (const auto& p : presets)
    {
        if (iequals(str, p.name))
        {
            csp.primaries = p.prim;
            csp.transfer = p.trc;
            repr.sys = p.sys;
            repr.levels = p.levels;
            if (out_fmt)
                *out_fmt = p.fmt;
            return true;
        }
    }

    return false; // Unknown preset
}

bool parse_out_fmt(std::string_view str, AVS_VideoInfo* vi, bool& is_rgb) noexcept
{
    struct format_info
    {
        std::string_view alias1;
        std::string_view alias2;
        int pixel_type;
        bool is_rgb;
    };

    static constexpr std::array<format_info, 54> video_formats{{
        {"Y8", "", AVS_CS_Y8, false},
        {"YUV420P8", "YV12", AVS_CS_YV12, false},
        {"YUV422P8", "YV16", AVS_CS_YV16, false},
        {"YUV444P8", "YV24", AVS_CS_YV24, false},
        {"RGBP", "", AVS_CS_RGBP, true},

        {"YUVA420P8", "", AVS_CS_YUVA420, false},
        {"YUVA422P8", "", AVS_CS_YUVA422, false},
        {"YUVA444P8", "", AVS_CS_YUVA444, false},
        {"RGBAP", "", AVS_CS_RGBAP, true},

        {"Y10", "", AVS_CS_Y10, false},
        {"YUV420P10", "", AVS_CS_YUV420P10, false},
        {"YUV422P10", "", AVS_CS_YUV422P10, false},
        {"YUV444P10", "", AVS_CS_YUV444P10, false},
        {"RGBP10", "", AVS_CS_RGBP10, true},

        {"YUVA420P10", "", AVS_CS_YUVA420P10, false},
        {"YUVA422P10", "", AVS_CS_YUVA422P10, false},
        {"YUVA444P10", "", AVS_CS_YUVA444P10, false},
        {"RGBAP10", "", AVS_CS_RGBAP10, true},

        {"Y12", "", AVS_CS_Y12, false},
        {"YUV420P12", "", AVS_CS_YUV420P12, false},
        {"YUV422P12", "", AVS_CS_YUV422P12, false},
        {"YUV444P12", "", AVS_CS_YUV444P12, false},
        {"RGBP12", "", AVS_CS_RGBP12, true},

        {"YUVA420P12", "", AVS_CS_YUVA420P12, false},
        {"YUVA422P12", "", AVS_CS_YUVA422P12, false},
        {"YUVA444P12", "", AVS_CS_YUVA444P12, false},
        {"RGBAP12", "", AVS_CS_RGBAP12, true},

        {"Y14", "", AVS_CS_Y14, false},
        {"YUV420P14", "", AVS_CS_YUV420P14, false},
        {"YUV422P14", "", AVS_CS_YUV422P14, false},
        {"YUV444P14", "", AVS_CS_YUV444P14, false},
        {"RGBP14", "", AVS_CS_RGBP14, true},

        {"YUVA420P14", "", AVS_CS_YUVA420P14, false},
        {"YUVA422P14", "", AVS_CS_YUVA422P14, false},
        {"YUVA444P14", "", AVS_CS_YUVA444P14, false},
        {"RGBAP14", "", AVS_CS_RGBAP14, true},

        {"Y16", "", AVS_CS_Y16, false},
        {"YUV420P16", "", AVS_CS_YUV420P16, false},
        {"YUV422P16", "", AVS_CS_YUV422P16, false},
        {"YUV444P16", "", AVS_CS_YUV444P16, false},
        {"RGBP16", "", AVS_CS_RGBP16},

        {"YUVA420P16", "", AVS_CS_YUVA420P16, false},
        {"YUVA422P16", "", AVS_CS_YUVA422P16, false},
        {"YUVA444P16", "", AVS_CS_YUVA444P16, false},
        {"RGBAP16", "", AVS_CS_RGBAP16, true},

        {"Y32", "", AVS_CS_Y32, false},
        {"YUV420PS", "", AVS_CS_YUV420PS, false},
        {"YUV422PS", "", AVS_CS_YUV422PS, false},
        {"YUV444PS", "", AVS_CS_YUV444PS, false},
        {"RGBPS", "", AVS_CS_RGBPS, true},

        {"YUVA420PS", "", AVS_CS_YUVA420PS, false},
        {"YUVA422PS", "", AVS_CS_YUVA422PS, false},
        {"YUVA444PS", "", AVS_CS_YUVA444PS, false},
        {"RGBAPS", "", AVS_CS_RGBAPS, true},
    }};

    auto it{std::find_if(video_formats.begin(), video_formats.end(),
        [&](const auto& fmt) { return iequals(str, fmt.alias1) || (!fmt.alias2.empty() && iequals(str, fmt.alias2)); })};

    if (it != video_formats.end())
    {
        vi->pixel_type = it->pixel_type;
        is_rgb = it->is_rgb;
        return true;
    }

    return false;
}
