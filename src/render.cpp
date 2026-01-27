#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <ranges>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include "libplacebo_render.h"
#include "mapping.h"
#include "params.h"

extern "C" {
#include "libplacebo/utils/dolbyvision.h"
}

namespace
{
    inline const char* avs_pool_str(AVS_ScriptEnvironment* env, std::string_view s)
    {
        return g_avs_api->avs_save_string(env, s.data(), static_cast<int>(s.size()));
    }

    inline AVS_Value avs_err_val(AVS_ScriptEnvironment* env, std::string_view s)
    {
        return avs_new_value_error(avs_pool_str(env, s));
    }

    template<typename T, typename Map_ptr>
    void check_set_prop(AVS_FilterInfo* AVS_RESTRICT fi, const AVS_Map* AVS_RESTRICT props, bool is_defined, const char* AVS_RESTRICT name,
        Map_ptr map, T& target, bool unspec = true) noexcept
    {
        if (is_defined)
            return;

        int err;
        const auto val{[&]() {
            if constexpr (std::is_floating_point_v<T>)
                return g_avs_api->avs_prop_get_float(fi->env, props, name, 0, &err);
            else
                return g_avs_api->avs_prop_get_int(fi->env, props, name, 0, &err);
        }()};

        if (err)
            return;

        if constexpr (std::is_same_v<Map_ptr, std::nullptr_t>)
        {
            target = static_cast<T>(val);
        }
        else
        {
            if (unspec && val != 2)
            {
                if (auto pl_val = map->find_value(val))
                    target = *pl_val;
            }
            else
            {
                if (auto pl_val = map->find_value(val))
                    target = *pl_val;
            }
        }
    }

    struct render_context
    {
        std::mutex mtx;
        std::unique_ptr<priv> vf;

        std::unique_ptr<pl_filter_config> upscaler_config;
        std::unique_ptr<pl_filter_config> downscaler_config;
        std::unique_ptr<pl_filter_config> plane_upscaler_config;
        std::unique_ptr<pl_filter_config> plane_downscaler_config;

        std::unique_ptr<pl_sigmoid_params> sigmoid_data;
        std::unique_ptr<pl_deband_params> deband_data;
        std::unique_ptr<pl_dither_params> dither_data;
        std::unique_ptr<pl_deinterlace_params> deinterlace_data;

        std::unique_ptr<pl_color_adjustment> color_adjustment_params;
        std::unique_ptr<pl_color_map_params> color_map_params;
        std::unique_ptr<pl_peak_detect_params> peak_detect_params;

        std::unique_ptr<pl_render_params> render_data;

        pl_hook_ptr shader;
        std::array<const pl_hook*, 1> shader_hooks;
        pl_custom_lut_ptr lut_ptr;
        std::unique_ptr<pl_dovi_metadata> dovi_meta;

        pl_chroma_location src_cplace;
        pl_chroma_location dst_cplace;
        int src_num_planes;
        int dst_num_planes;
        std::array<int, 4> src_planes;
        std::array<int, 4> dst_planes;
        int src_comp_size;

        pl_frame src_frame;
        pl_frame dst_frame;
        pl_fmt_type src_fmt_type;

        bool is_matrix_def;
        bool is_trc_def;
        bool is_prim_def;
        bool is_levels_def;
        bool is_src_hdr_max_luma_def;
        bool is_src_hdr_min_luma_def;

        int field;

        std::filesystem::path cache_path;
        uint64_t cache_signature;
    };

    int fix_chroma_offset(render_context* d, pl_tex source, pl_tex target, bool is_input) noexcept
    {
        const auto& vf{d->vf};
        const auto& gpu{vf->vk->gpu};

        pl_shader sh{pl_dispatch_begin(vf->dp.get())};
        const pl_sample_src sample{.tex = source};
        pl_shader_sample_direct(sh, &sample);

        const pl_custom_shader custom{
            .description = "Fix AviSynth+ Float Chroma Offset",
            .body = (is_input) ? "color.r += 0.5;" : "color.r -= 0.5;",
            .input = PL_SHADER_SIG_COLOR,
            .output = PL_SHADER_SIG_COLOR,
        };
        pl_shader_custom(sh, &custom);

        const pl_dispatch_params params{
            .shader = &sh,
            .target = target,
        };

        if (!pl_dispatch_finish(vf->dp.get(), &params))
            return -1;

        return 0;
    }

    const std::array<pl_tex, 4>* get_cached_planes(render_context* AVS_RESTRICT d, AVS_VideoFrame* AVS_RESTRICT src, int n) noexcept
    {
        const auto& vf{d->vf};
        const auto& gpu{vf->vk->gpu};
        auto& cache{vf->cache};
        vf->timer++;

        for (auto& entry : cache)
        {
            if (entry.frame_idx == n)
            {
                entry.last_used = vf->timer;
                return &entry.planes;
            }
        }

        const size_t active_cache_size{d->deinterlace_data ? vf->CACHE_SIZE : 1};
        cached_frame* lru_entry{&cache[0]};
        for (size_t i{0}; i < active_cache_size; ++i)
        {
            if (cache[i].frame_idx == -1)
            {
                lru_entry = &vf->cache[i];
                break;
            }
            if (cache[i].last_used < lru_entry->last_used)
                lru_entry = &vf->cache[i];
        }

        const int src_comp_size{d->src_comp_size};
        const int src_comp_bits{src_comp_size * 8};
        const auto& src_fmt_type{d->src_fmt_type};
        const auto& src_planes{d->src_planes};

        for (int i{0}; i < d->src_num_planes; ++i)
        {
            const int plane{src_planes[i]};
            const pl_plane_data source{
                .type = src_fmt_type,
                .width = g_avs_api->avs_get_row_size_p(src, plane) / src_comp_size,
                .height = g_avs_api->avs_get_height_p(src, plane),
                .component_size = {src_comp_bits},
                .component_map = {i},
                .pixel_stride = static_cast<size_t>(src_comp_size),
                .row_stride = static_cast<size_t>(g_avs_api->avs_get_pitch_p(src, plane)),
                .pixels = g_avs_api->avs_get_read_ptr_p(src, plane),
            };

            if (!pl_upload_plane(gpu, NULL, &lru_entry->planes[i], &source))
                return nullptr;

            if (src_fmt_type == PL_FMT_FLOAT && (plane == AVS_PLANAR_U || plane == AVS_PLANAR_V))
            {
                pl_tex_params t_fix{lru_entry->planes[i]->params};
                t_fix.renderable = true;
                t_fix.host_writable = false;

                auto& fix_fbo_in{vf->fix_fbo_in};
                if (!pl_tex_recreate(gpu, &fix_fbo_in, &t_fix))
                    return nullptr;
                if (fix_chroma_offset(d, lru_entry->planes[i], fix_fbo_in, true))
                    return nullptr;

                std::swap(lru_entry->planes[i], fix_fbo_in);
            }
        }

        lru_entry->frame_idx = n;
        lru_entry->last_used = vf->timer;
        return &lru_entry->planes;
    }

    int render_filter(AVS_VideoFrame* AVS_RESTRICT dst, AVS_VideoFrame* AVS_RESTRICT src, int n, render_context* AVS_RESTRICT d,
        AVS_FilterInfo* AVS_RESTRICT fi) noexcept
    {
        const int src_num_planes{d->src_num_planes};

        const auto& vf{d->vf};
        const auto& gpu{vf->vk->gpu};

        const auto textures_curr{get_cached_planes(d, src, n)};
        if (!textures_curr)
            return -1;

        auto& src_frame{d->src_frame};
        for (int i{0}; i < d->src_num_planes; ++i)
            src_frame.planes[i].texture = (*textures_curr)[i];

        pl_frame_set_chroma_location(&src_frame, d->src_cplace);

        pl_frame f_prev;
        pl_frame f_next;

        if (d->deinterlace_data)
        {
            const int max_f{fi->vi.num_frames - 1};
            const int max_p{(std::max)(0, n - 1)};
            const int max_n{(std::min)(max_f, n + 1)};

            const auto p_ptr{avs_helpers::avs_video_frame_ptr{g_avs_api->avs_get_frame(fi->child, max_p)}};
            auto n_ptr{avs_helpers::avs_video_frame_ptr{g_avs_api->avs_get_frame(fi->child, max_n)}};

            auto tex_prev{get_cached_planes(d, p_ptr.get(), max_p)};
            auto tex_next{get_cached_planes(d, n_ptr.get(), max_n)};

            if (tex_prev && tex_next)
            {
                f_prev = src_frame;
                f_next = src_frame;

                for (int i{0}; i < d->src_num_planes; ++i)
                {
                    f_prev.planes[i].texture = (*tex_prev)[i];
                    f_next.planes[i].texture = (*tex_next)[i];
                }

                src_frame.prev = &f_prev;
                src_frame.next = &f_next;
            }
        }

        auto& dst_frame{d->dst_frame};
        pl_frame_set_chroma_location(&dst_frame, d->dst_cplace);

        if (!pl_render_image(vf->rr.get(), &src_frame, &dst_frame, d->render_data.get()))
            return -1;

        // Download planes
        const auto& dst_planes{d->dst_planes};
        const int dst_bit_depth{dst_frame.repr.bits.color_depth};
        for (int i{0}; i < d->dst_num_planes; ++i)
        {
            const int plane{dst_planes[i]};
            auto& tex_out{vf->tex_out[i]};

            if (dst_bit_depth == 32 && (plane == AVS_PLANAR_U || plane == AVS_PLANAR_V))
            {
                pl_tex_params t_fix{tex_out->params};
                t_fix.renderable = true;

                auto& fix_fbo_out{vf->fix_fbo_out};
                if (!pl_tex_recreate(gpu, &fix_fbo_out, &t_fix))
                    return -1;
                if (fix_chroma_offset(d, tex_out, fix_fbo_out, false))
                    return -1;

                std::swap(tex_out, fix_fbo_out);
            }

            const pl_tex_transfer_params ttr{
                .tex = tex_out,
                .row_pitch = static_cast<size_t>(g_avs_api->avs_get_pitch_p(dst, plane)),
                .ptr = g_avs_api->avs_get_write_ptr_p(dst, plane),
            };

            if (!pl_tex_download(gpu, &ttr))
                return -1;
        }

        return 0;
    }

    AVS_VideoFrame* AVSC_CC render_get_frame(AVS_FilterInfo* fi, int n) noexcept
    {
        auto* d{reinterpret_cast<render_context*>(fi->user_data)};
        const auto& env{fi->env};

        const int is_double_rate{d->field == -2 || d->field > 1};
        const int src_n{is_double_rate ? (n >> 1) : n};

        const auto src_ptr{avs_helpers::avs_video_frame_ptr{g_avs_api->avs_get_frame(fi->child, src_n)}};
        if (!src_ptr)
            return nullptr;
        auto dst_ptr{avs_helpers::avs_video_frame_ptr{g_avs_api->avs_new_video_frame_p(env, &fi->vi, src_ptr.get())}};

        const auto set_err{[&](std::string_view msg) {
            fi->error = avs_pool_str(env, msg);
            return nullptr;
        }};

        std::scoped_lock lock(d->mtx);

        const AVS_Map* props{g_avs_api->avs_get_frame_props_ro(env, src_ptr.get())};
        auto& src_repr{d->src_frame.repr};
        auto& src_pl_csp{d->src_frame.color};
        const auto& dovi_meta{d->dovi_meta};
        src_repr.dovi = nullptr;

        if (!dovi_meta)
            check_set_prop(fi, props, d->is_matrix_def, "_Matrix", &map_libpl_avs_matrix, src_repr.sys);

        check_set_prop(fi, props, d->is_trc_def, "_Transfer", &map_libpl_avs_trc, src_pl_csp.transfer);
        check_set_prop(fi, props, d->is_prim_def, "_Primaries", &map_libpl_avs_prim, src_pl_csp.primaries);
        check_set_prop(fi, props, d->is_levels_def, "_ColorRange", &map_libpl_avs_levels, src_repr.levels, false);

        int err;
        const auto& deinterlace_data{d->deinterlace_data};
        if (deinterlace_data)
        {
            const int is_second_field{n & 1};
            const int64_t field{g_avs_api->avs_prop_get_int(env, props, "_FieldBased", 0, &err)};
            const bool is_tff{(d->field > -1) ? (d->src_frame.first_field == PL_FIELD_TOP)
                                              : ((field == 2) || (err && g_avs_api->avs_get_parity(fi->child, src_n)))};
            const pl_field first_f{is_tff ? PL_FIELD_TOP : PL_FIELD_BOTTOM};

            if (is_double_rate)
                d->src_frame.field = is_second_field ? is_tff ? PL_FIELD_BOTTOM : PL_FIELD_TOP : first_f;
            else
                d->src_frame.field = first_f;

            if (d->field < 0)
                d->src_frame.first_field = first_f;
        }

        if (pl_color_transfer_is_hdr(src_pl_csp.transfer))
        {
            auto& hdr_props{src_pl_csp.hdr};

            const double max_cll{g_avs_api->avs_prop_get_float(env, props, "ContentLightLevelMax", 0, &err)};
            if (!err)
                hdr_props.max_cll = max_cll;
            const double max_fall{g_avs_api->avs_prop_get_float(env, props, "ContentLightLevelAverage", 0, &err)};
            if (!err)
                hdr_props.max_fall = max_fall;

            check_set_prop(fi, props, d->is_src_hdr_max_luma_def, "MasteringDisplayMaxLuminance", nullptr, hdr_props.max_luma, false);
            check_set_prop(fi, props, d->is_src_hdr_min_luma_def, "MasteringDisplayMinLuminance", nullptr, hdr_props.min_luma, false);

            if (const double* primaries_x{g_avs_api->avs_prop_get_float_array(env, props, "MasteringDisplayPrimariesX", &err)}; !err)
            {
                hdr_props.prim.red.x = primaries_x[0];
                hdr_props.prim.green.x = primaries_x[1];
                hdr_props.prim.blue.x = primaries_x[2];
            }
            if (const double* primaries_y{g_avs_api->avs_prop_get_float_array(env, props, "MasteringDisplayPrimariesY", &err)}; !err)
            {
                hdr_props.prim.red.y = primaries_y[0];
                hdr_props.prim.green.y = primaries_y[1];
                hdr_props.prim.blue.y = primaries_y[2];
            }
            if (const double whitePoint_x{g_avs_api->avs_prop_get_float(env, props, "MasteringDisplayWhitePointX", 0, &err)}; !err)
                hdr_props.prim.white.x = whitePoint_x;
            if (const double whitePoint_y{g_avs_api->avs_prop_get_float(env, props, "MasteringDisplayWhitePointY", 0, &err)}; !err)
                hdr_props.prim.white.y = whitePoint_y;

            // DOVI Metadata
            if (dovi_meta)
            {
                const uint8_t* doviRpu{
                    reinterpret_cast<const uint8_t*>(g_avs_api->avs_prop_get_data(env, props, "DolbyVisionRPU", 0, &err))};
                if (err)
                    return set_err("libplacebo_Render: missing DolbyVisionRPU frame property!");

                const size_t doviRpuSize{static_cast<size_t>(g_avs_api->avs_prop_get_data_size(env, props, "DolbyVisionRPU", 0, &err))};
                if (err || !doviRpuSize)
                    return set_err("libplacebo_Render: invalid DolbyVisionRPU frame property!");

                dovi_ptr<DoviRpuOpaque> rpu{dovi_parse_unspec62_nalu(doviRpu, doviRpuSize)};
                if (!rpu)
                    return set_err("libplacebo_Render: failed to parse RPU NALU");

                dovi_ptr<const DoviRpuDataHeader> header{dovi_rpu_get_header(rpu.get())};
                if (!header)
                    return set_err(std::format("libplacebo_Render: failed parsing RPU: {}", dovi_rpu_get_error(rpu.get())));

                d->dovi_meta = create_dovi_meta(rpu.get(), *header);
                src_repr.dovi = d->dovi_meta.get();

                if (header->guessed_profile == 5 && src_repr.levels != PL_COLOR_LEVELS_FULL)
                {
                    check_set_prop(fi, props, false, "_ColorRange", &map_libpl_avs_levels, src_repr.levels, false);

                    if (src_repr.levels != PL_COLOR_LEVELS_FULL)
                        return set_err("libplacebo_Render: Dolby Vision Profile 5 requires full levels.");
                }

                pl_hdr_metadata_from_dovi_rpu(&hdr_props, doviRpu, doviRpuSize);

                if (header->vdr_dm_metadata_present_flag)
                {
                    if (dovi_ptr<const DoviVdrDmData> dm_data{dovi_rpu_get_vdr_dm_data(rpu.get())})
                    {
                        // Should avoid changing the source black point when mapping to PQ
                        // As the source image already has a specific black point,
                        // and the RPU isn't necessarily ground truth on the actual coded values

                        // Set target black point to the same as source
                        if (d->dst_frame.color.transfer == PL_COLOR_TRC_PQ)
                            d->dst_frame.color.hdr.min_luma = hdr_props.min_luma;
                        else
                            hdr_props.min_luma = pl_hdr_rescale(PL_HDR_PQ, PL_HDR_NITS, dm_data->source_min_pq / 4095.0f);

                        hdr_props.max_luma = pl_hdr_rescale(PL_HDR_PQ, PL_HDR_NITS, dm_data->source_max_pq / 4095.0f);
                    }
                }
            }
        }

        auto& dst_pl_csp{d->dst_frame.color};
        pl_color_space_infer_map(&src_pl_csp, &dst_pl_csp);

        if (render_filter(dst_ptr.get(), src_ptr.get(), src_n, d, fi))
            return set_err(std::format("libplacebo_Render: {}", d->vf->log_buffer.str()));

        const auto& dst_repr{d->dst_frame.repr};
        const auto& dst_sys{dst_repr.sys};

        AVS_Map* dst_props{g_avs_api->avs_get_frame_props_rw(env, dst_ptr.get())};
        const auto sync{[&](const char* name, const auto val, const auto& map) {
            if (auto res{map.find(val)})
                g_avs_api->avs_prop_set_int(env, dst_props, name, *res, 0);
            else
                g_avs_api->avs_prop_delete_key(env, dst_props, name);
        }};

        sync("_ColorRange", dst_repr.levels, map_libpl_avs_levels);
        sync("_Matrix", dst_sys, map_libpl_avs_matrix);
        sync("_Transfer", dst_pl_csp.transfer, map_libpl_avs_trc);
        sync("_Primaries", dst_pl_csp.primaries, map_libpl_avs_prim);

        if (dst_sys != PL_COLOR_SYSTEM_RGB)
            sync("_ChromaLocation", d->dst_cplace, map_libpl_avs_cplace);

        if (deinterlace_data)
        {
            sync("_FieldBased", d->dst_frame.field, map_libpl_avs_field);

            if (is_double_rate)
            {
                const auto v{g_avs_api->avs_prop_get_int(env, props, "_DurationDen", 0, &err)};
                if (!err)
                    g_avs_api->avs_prop_set_int(env, dst_props, "_DurationDen", v * 2, 0);
            }
        }

        static constexpr std::array hdr_keys{
            std::string_view{"ContentLightLevelMax"},
            std::string_view{"ContentLightLevelAverage"},
            std::string_view{"MasteringDisplayMaxLuminance"},
            std::string_view{"MasteringDisplayMinLuminance"},
            std::string_view{"MasteringDisplayPrimariesX"},
            std::string_view{"MasteringDisplayPrimariesY"},
            std::string_view{"MasteringDisplayWhitePointX"},
            std::string_view{"MasteringDisplayWhitePointY"},
        };

        if (!pl_color_transfer_is_hdr(dst_pl_csp.transfer))
        {
            for (const auto& k : hdr_keys)
                g_avs_api->avs_prop_delete_key(env, dst_props, k.data());
        }
        else
        {
            const auto& dst_hdr_props{dst_pl_csp.hdr};
            g_avs_api->avs_prop_set_float(env, dst_props, "ContentLightLevelMax", dst_hdr_props.max_cll, 0);
            g_avs_api->avs_prop_set_float(env, dst_props, "ContentLightLevelAverage", dst_hdr_props.max_fall, 0);
            g_avs_api->avs_prop_set_float(env, dst_props, "MasteringDisplayMaxLuminance", dst_hdr_props.max_luma, 0);
            g_avs_api->avs_prop_set_float(env, dst_props, "MasteringDisplayMinLuminance", dst_hdr_props.min_luma, 0);

            const std::array<double, 3> prims_x{
                dst_hdr_props.prim.red.x,
                dst_hdr_props.prim.green.x,
                dst_hdr_props.prim.blue.x,
            };
            const std::array<double, 3> prims_y{
                dst_hdr_props.prim.red.y,
                dst_hdr_props.prim.green.y,
                dst_hdr_props.prim.blue.y,
            };

            g_avs_api->avs_prop_set_float_array(env, dst_props, "MasteringDisplayPrimariesX", prims_x.data(), 3);
            g_avs_api->avs_prop_set_float_array(env, dst_props, "MasteringDisplayPrimariesY", prims_y.data(), 3);
            g_avs_api->avs_prop_set_float(env, dst_props, "MasteringDisplayWhitePointX", dst_hdr_props.prim.white.x, 0);
            g_avs_api->avs_prop_set_float(env, dst_props, "MasteringDisplayWhitePointY", dst_hdr_props.prim.white.y, 0);
        }

        return dst_ptr.release();
    }

    void AVSC_CC free_render(AVS_FilterInfo* fi) noexcept
    {
        render_context* d{reinterpret_cast<render_context*>(fi->user_data)};
        const auto& vf{d->vf};
        const auto& cache_path{d->cache_path};

        if (!cache_path.empty() && vf && vf->cache_obj && d->cache_signature != pl_cache_signature(vf->cache_obj.get()))
        {
            const size_t size{pl_cache_save(vf->cache_obj.get(), nullptr, 0)};
            std::vector<std::byte> data(size);
            size_t written{pl_cache_save(vf->cache_obj.get(), reinterpret_cast<uint8_t*>(data.data()), size)};

            if (written > 0)
            {
                if (const auto parent{cache_path.parent_path()}; !parent.empty())
                {
                    std::error_code ec;
                    std::filesystem::create_directories(parent, ec);
                }

                std::ofstream f(cache_path, std::ios::binary | std::ios::trunc);
                if (f.good())
                    f.write(reinterpret_cast<const char*>(data.data()), written);
            }
        }

        delete d;
    }

    int AVSC_CC render_set_cache_hints(AVS_FilterInfo* fi, int cachehints, int frame_range) noexcept
    {
        return cachehints == AVS_CACHE_GET_MTMODE ? 2 : 0;
    }

    int AVSC_CC render_get_parity(AVS_FilterInfo* fi, int n) noexcept
    {
        render_context* d{reinterpret_cast<render_context*>(fi->user_data)};

        if (d->field > -1)
            return (d->field > 1) ? (d->field - 2) : d->field;

        const bool is_double_rate{d->field == -2 || d->field > 1};
        const int src_n{is_double_rate ? (n >> 1) : n};
        const int child_parity{g_avs_api->avs_get_parity(fi->child, src_n)};

        if (is_double_rate)
            return child_parity ^ (n & 1);

        return child_parity;
    }

    std::optional<std::string> load_file_content(const char* path, std::string& err_msg)
    {
        const auto open_file{[](const char* p) -> FILE* {
#ifdef _WIN32
            const auto to_wide{[](const char* s, UINT cp) {
                const int size = MultiByteToWideChar(cp, 0, s, -1, nullptr, 0);
                std::wstring w(size, 0);
                MultiByteToWideChar(cp, 0, s, -1, w.data(), size);
                return w;
            }};
            FILE* f{_wfopen(to_wide(p, CP_UTF8).c_str(), L"rb")};
            if (!f)
                f = _wfopen(to_wide(p, CP_ACP).c_str(), L"rb");
            return f;
#else
            return std::fopen(p, "rb");
#endif
        }};

        FILE* f{open_file(path)};
        if (!f)
        {
            err_msg = std::format("error opening file: {}", std::strerror(errno));
            return std::nullopt;
        }

        std::fseek(f, 0, SEEK_END);
        const long size{std::ftell(f)};
        if (size < 0)
        {
            std::fclose(f);
            err_msg = "error determining file size";
            return std::nullopt;
        }

        std::rewind(f);
        std::string buffer(size, '\0');

        if (std::fread(buffer.data(), 1, size, f) != static_cast<size_t>(size))
        {
            err_msg = "Error reading file content";
            std::fclose(f);
            return std::nullopt;
        }

        std::fclose(f);
        return buffer;
    }

    template<typename T, typename TTarget, typename Func = std::identity>
    void update_param(std::optional<T> val, TTarget& target, Func&& transform = {})
    {
        if (val)
            target = static_cast<TTarget>(std::forward<Func>(transform)(*val));
    }

    template<typename T, typename TTarget, typename Func = std::identity>
    bool update_param(std::optional<T> val, TTarget& target, std::string_view name, std::string& err_msg,
        std::type_identity_t<std::optional<T>> min = std::nullopt, std::type_identity_t<std::optional<T>> max = std::nullopt,
        Func&& transform = {})
    {
        if (!val)
            return true;

        if ((min && *val < *min) || (max && *val > *max))
        {
            if (min && max)
                err_msg = std::format("libplacebo_Render: {} must be between {} and {}.", name, *min, *max);
            else if (min)
                err_msg = std::format("libplacebo_Render: {} must be at least {}.", name, *min);
            else if (max)
                err_msg = std::format("libplacebo_Render: {} must be at most {}.", name, *max);

            return false;
        }

        target = static_cast<TTarget>(std::forward<Func>(transform)(*val));
        return true;
    }
} // namespace

AVS_Value AVSC_CC create_render(AVS_ScriptEnvironment* env, AVS_Value args, void* param)
{
    AVS_FilterInfo* fi;
    const avs_helpers::avs_clip_ptr clip_ptr{g_avs_api->avs_new_c_filter(env, &fi, avs_array_elt(args, get_param_idx<"clip">()), 1)};
    AVS_Clip* clip{clip_ptr.get()};
    auto params{std::make_unique<render_context>()};

    auto& vi{fi->vi};
    if (!avs_is_planar(&vi))
        return avs_new_value_error("libplacebo_Render: clip must be in planar format.");

    const int src_num_comp{g_avs_api->avs_num_components(&vi)};
    const int is_src_rgb{avs_is_rgb(&vi)};
    const int src_bit_depth{g_avs_api->avs_bits_per_component(&vi)};
    std::string msg;

    // --- Device Initialization ---
    {
        const int device{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"device">()).value_or(-1)};
        const int list_devices{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"list_devices">()).value_or(0)};
        if (list_devices || device > -1)
        {
            std::vector<VkPhysicalDevice> devices{};
            VkInstance inst{};

            AVS_Value dev_info{devices_info(clip, fi->env, devices, inst, msg, "libplacebo_Render", device, list_devices)};
            if (avs_is_error(dev_info) || avs_is_clip(dev_info))
            {
                fi->user_data = params.release();
                fi->free_filter = free_render;
                return dev_info;
            }

            params->vf = avs_libplacebo_init(devices[device], msg);
            vkDestroyInstance(inst, nullptr);
        }
        else
        {
            if (device < -1)
                return avs_new_value_error("libplacebo_Render: device must be greater than or equal to -1.");

            params->vf = avs_libplacebo_init(nullptr, msg);
        }

        if (!msg.empty())
            return avs_err_val(env, std::format("libplacebo_Render: {}", msg));
    }

    const auto& gpu{params->vf->vk->gpu};

    // --- Shader Cache Loading ---
    if (const auto cache_path_arg{avs_helpers::get_opt_arg<const char*>(env, args, get_param_idx<"shader_cache">())};
        cache_path_arg && *cache_path_arg)
    {
        params->cache_path = *cache_path_arg;
        const auto& cache_path{params->cache_path};
        std::error_code ec;

        if (std::filesystem::exists(cache_path) && std::filesystem::is_regular_file(cache_path))
        {
            const auto size{std::filesystem::file_size(cache_path, ec)};
            if (!ec && size > 0)
            {
                std::vector<std::byte> data(size);
                std::ifstream f(cache_path, std::ios::binary);
                if (f.read(reinterpret_cast<char*>(data.data()), size))
                    pl_cache_load(params->vf->cache_obj.get(), reinterpret_cast<const uint8_t*>(data.data()), size);

                params->cache_signature = pl_cache_signature(params->vf->cache_obj.get());
            }
        }
    }

    // --- Preset & Render Params ---
    const auto preset{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"preset">())};
    if (preset)
    {
        const std::optional<pl_render_params> base_params{[&]() -> std::optional<pl_render_params> {
            if (iequals(*preset, "high_quality"))
                return pl_render_high_quality_params;
            if (iequals(*preset, "fast"))
                return pl_render_fast_params;
            if (iequals(*preset, "default"))
                return pl_render_default_params;

            return std::nullopt;
        }()};

        if (!base_params)
            return avs_err_val(env, std::format("libplacebo_Render: Invalid preset '{}'.", *preset));

        params->render_data = std::make_unique<pl_render_params>(*base_params);
    }
    else
        params->render_data = std::make_unique<pl_render_params>();

    const auto& render_data{params->render_data};

    // --- Geometry ---
    const int src_w{vi.width};
    const int src_h{vi.height};
    if (!update_param(avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"width">()), vi.width, "width", msg, 16))
        return avs_err_val(env, msg);
    if (!update_param(avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"height">()), vi.height, "height", msg, 16))
        return avs_err_val(env, msg);

    const float crop_x{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_left">()).value_or(0.0f)};
    const float crop_y{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_top">()).value_or(0.0f)};
    const float crop_w{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_width">()).value_or(0.0f)};
    const float crop_h{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_height">()).value_or(0.0f)};

    // --- Custom Shader ---
    if (const auto custom_shader{avs_helpers::get_opt_arg<const char*>(env, args, get_param_idx<"custom_shader_path">())})
    {
        auto content{load_file_content(*custom_shader, msg)};
        if (!content)
            return avs_err_val(env, std::format("libplacebo_Render: {}", msg));

        if (const auto shader_param{avs_helpers::get_opt_arg<std::string_view>(env, args, get_param_idx<"custom_shader_param">())})
        {
            std::string_view shader_p{*shader_param};
            const auto replace_define{[](std::string& source, std::string_view key, std::string_view value) {
                std::string search_target{"#define " + std::string(key)};
                const size_t pos{source.find(search_target)};
                if (pos != std::string::npos)
                {
                    const size_t val_start{source.find_first_not_of(" \t", pos + search_target.length())};
                    if (val_start == std::string::npos)
                        return;

                    size_t val_end{source.find_first_of(" \t\n\r/", val_start)};
                    if (val_end == std::string::npos)
                        val_end = source.length();

                    source.replace(val_start, val_end - val_start, value);
                }
            }};

            size_t start{0};
            const std::string_view delimiters{" ,;"};
            while (start < shader_p.size())
            {
                start = shader_p.find_first_not_of(delimiters, start);
                if (start == std::string_view::npos)
                    break;

                const size_t eq_pos = shader_p.find('=', start);
                if (eq_pos == std::string_view::npos)
                    return avs_new_value_error("libplacebo_Render: invalid format (missing '=')");

                size_t next_delim{shader_p.find_first_of(delimiters, eq_pos)};
                if (next_delim == std::string_view::npos)
                    next_delim = shader_p.size();

                std::string_view key{shader_p.substr(start, eq_pos - start)};
                std::string_view value{shader_p.substr(eq_pos + 1, next_delim - (eq_pos + 1))};
                if (!key.empty() && !value.empty())
                    replace_define(*content, key, value);

                start = next_delim;
            }
        }

        params->shader.reset(pl_mpv_user_shader_parse(gpu, content->c_str(), content->size()));
        if (!params->shader)
            return avs_new_value_error("libplacebo_Render: failed parsing shader!");

        params->shader_hooks[0] = params->shader.get();
        render_data->hooks = params->shader_hooks.data();
        render_data->num_hooks = 1;
    }

    // --- Scaler
    {
        struct scaler_args_ids
        {
            int name;
            int kernel;
            int window;
            int polar;
            int radius;
            int clamp;
            int taper;
            int blur;
            int antiring;
            int param1;
            int param2;
            int wparam1;
            int wparam2;
        };

        auto parse_scaler{[&](const scaler_args_ids& ids, std::unique_ptr<pl_filter_config>& storage,
                              const char* default_name) -> const pl_filter_config* {
            const auto opt_k{avs_helpers::get_opt_arg<const char*>(env, args, ids.kernel)};
            const auto opt_w{avs_helpers::get_opt_arg<const char*>(env, args, ids.window)};
            const auto opt_p{avs_helpers::get_opt_arg<bool>(env, args, ids.polar)};
            const auto opt_r{avs_helpers::get_opt_arg<float>(env, args, ids.radius)};
            const auto opt_cl{avs_helpers::get_opt_arg<float>(env, args, ids.clamp)};
            const auto opt_t{avs_helpers::get_opt_arg<float>(env, args, ids.taper)};
            const auto opt_b{avs_helpers::get_opt_arg<float>(env, args, ids.blur)};
            const auto opt_a{avs_helpers::get_opt_arg<float>(env, args, ids.antiring)};

            const auto opt_p1{avs_helpers::get_opt_arg<float>(env, args, ids.param1)};
            const auto opt_p2{avs_helpers::get_opt_arg<float>(env, args, ids.param2)};
            const auto opt_wp1{avs_helpers::get_opt_arg<float>(env, args, ids.wparam1)};
            const auto opt_wp2{avs_helpers::get_opt_arg<float>(env, args, ids.wparam2)};

            const bool is_mod_defined{
                opt_k || opt_w || opt_p || opt_r || opt_cl || opt_t || opt_b || opt_a || opt_p1 || opt_p2 || opt_wp1 || opt_wp2};
            auto name{avs_helpers::get_opt_arg<const char*>(env, args, ids.name)};
            if (!name)
            {
                if (!is_mod_defined && preset)
                    return nullptr;
                name = default_name;
            }

            if (iequals(*name, "none"))
                return nullptr;

            const pl_filter_usage filter_usage{(ids.name == get_param_idx<"upscaler">() || ids.name == get_param_idx<"plane_upscaler">())
                                                   ? PL_FILTER_UPSCALING
                                                   : PL_FILTER_DOWNSCALING};
            const pl_filter_config* base{pl_find_filter_config(*name, filter_usage)};
            if (!base)
            {
                msg = std::format("libplacebo_Render: Invalid scaler '{}'.", *name);
                return nullptr;
            }

            if (is_mod_defined)
            {
                storage = std::make_unique<pl_filter_config>(*base);
                pl_filter_config* const cfg{storage.get()};

                if (opt_k)
                {
                    cfg->kernel = pl_find_filter_function(*opt_k);
                    if (!cfg->kernel)
                    {
                        msg = std::format("libplacebo_Render: Invalid kernel '{}'.", *opt_k);
                        return nullptr;
                    }

                    if (!opt_r)
                        cfg->radius = cfg->kernel->radius;
                }

                if (opt_w)
                {
                    cfg->window = pl_find_filter_function(*opt_w);
                    if (!cfg->window)
                    {
                        msg = std::format("libplacebo_Render: Invalid window '{}'.", *opt_w);
                        return nullptr;
                    }
                }

                update_param(opt_p, cfg->polar);
                if (!update_param(opt_r, cfg->radius, "radius", msg, 0.0f, 16.0f))
                    return nullptr;
                if (!update_param(opt_cl, cfg->clamp, "clamp", msg, 0.0f, 1.0f))
                    return nullptr;
                if (!update_param(opt_t, cfg->taper, "taper", msg, 0.0f, 1.0f))
                    return nullptr;
                if (!update_param(opt_b, cfg->blur, "blur", msg, 0.0f, 100.0f))
                    return nullptr;
                if (!update_param(opt_a, cfg->antiring, "antiring", msg, 0.0f, 1.0f))
                    return nullptr;
                update_param(opt_p1, cfg->params[0]);
                update_param(opt_p2, cfg->params[1]);
                update_param(opt_wp1, cfg->wparams[0]);
                update_param(opt_wp2, cfg->wparams[1]);

                return cfg;
            }

            return base;
        }};

        scaler_args_ids up_ids{
            get_param_idx<"upscaler">(),
            get_param_idx<"upscaler_kernel">(),
            get_param_idx<"upscaler_window">(),
            get_param_idx<"upscaler_polar">(),
            get_param_idx<"upscaler_radius">(),
            get_param_idx<"upscaler_clamp">(),
            get_param_idx<"upscaler_taper">(),
            get_param_idx<"upscaler_blur">(),
            get_param_idx<"upscaler_antiring">(),
            get_param_idx<"upscaler_param1">(),
            get_param_idx<"upscaler_param2">(),
            get_param_idx<"upscaler_wparam1">(),
            get_param_idx<"upscaler_wparam2">(),
        };
        scaler_args_ids down_ids{
            get_param_idx<"downscaler">(),
            get_param_idx<"downscaler_kernel">(),
            get_param_idx<"downscaler_window">(),
            get_param_idx<"downscaler_polar">(),
            get_param_idx<"downscaler_radius">(),
            get_param_idx<"downscaler_clamp">(),
            get_param_idx<"downscaler_taper">(),
            get_param_idx<"downscaler_blur">(),
            get_param_idx<"downscaler_antiring">(),
            get_param_idx<"downscaler_param1">(),
            get_param_idx<"downscaler_param2">(),
            get_param_idx<"downscaler_wparam1">(),
            get_param_idx<"downscaler_wparam2">(),
        };

        scaler_args_ids p_up_ids{
            get_param_idx<"plane_upscaler">(),
            get_param_idx<"plane_upscaler_kernel">(),
            get_param_idx<"plane_upscaler_window">(),
            get_param_idx<"plane_upscaler_polar">(),
            get_param_idx<"plane_upscaler_radius">(),
            get_param_idx<"plane_upscaler_clamp">(),
            get_param_idx<"plane_upscaler_taper">(),
            get_param_idx<"plane_upscaler_blur">(),
            get_param_idx<"plane_upscaler_antiring">(),
            get_param_idx<"plane_upscaler_param1">(),
            get_param_idx<"plane_upscaler_param2">(),
            get_param_idx<"plane_upscaler_wparam1">(),
            get_param_idx<"plane_upscaler_wparam2">(),
        };
        scaler_args_ids p_down_ids{
            get_param_idx<"plane_downscaler">(),
            get_param_idx<"plane_downscaler_kernel">(),
            get_param_idx<"plane_downscaler_window">(),
            get_param_idx<"plane_downscaler_polar">(),
            get_param_idx<"plane_downscaler_radius">(),
            get_param_idx<"plane_downscaler_clamp">(),
            get_param_idx<"plane_downscaler_taper">(),
            get_param_idx<"plane_downscaler_blur">(),
            get_param_idx<"plane_downscaler_antiring">(),
            get_param_idx<"plane_downscaler_param1">(),
            get_param_idx<"plane_downscaler_param2">(),
            get_param_idx<"plane_downscaler_wparam1">(),
            get_param_idx<"plane_downscaler_wparam2">(),
        };

        const auto parsed_upscaler{parse_scaler(up_ids, params->upscaler_config, "ewa_lanczossharp")};
        if (!msg.empty())
            return avs_err_val(env, msg);
        const auto parsed_downscaler{parse_scaler(down_ids, params->downscaler_config, "catmull_rom")};
        if (!msg.empty())
            return avs_err_val(env, msg);

        const auto parsed_plane_upscaler{parse_scaler(p_up_ids, params->plane_upscaler_config, "spline36")};
        if (!msg.empty())
            return avs_err_val(env, msg);
        const auto parsed_plane_downscaler{parse_scaler(p_down_ids, params->plane_downscaler_config, "spline36")};
        if (!msg.empty())
            return avs_err_val(env, msg);

        render_data->upscaler = parsed_upscaler;
        render_data->downscaler = parsed_downscaler;
        render_data->plane_upscaler = parsed_plane_upscaler;
        render_data->plane_downscaler = parsed_plane_downscaler;

        if (!update_param(avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"antiringing_strength">()),
                render_data->antiringing_strength, "antiringing_strength", msg, 0.0f, 1.0f))
            return avs_err_val(env, msg);
    }

    // --- Linear Scaling & Sigmoid ---
    {
        update_param(avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"linear_scaling">()), render_data->disable_linear_scaling,
            [](bool val) { return !val; });

        if (!render_data->disable_linear_scaling)
        {
            const auto sigmoid{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"sigmoid">())};
            const auto sigmoid_center{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"sigmoid_center">())};
            const auto sigmoid_slope{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"sigmoid_slope">())};

            if (sigmoid && !*sigmoid)
            {
                render_data->sigmoid_params = nullptr;
            }
            else if (sigmoid || sigmoid_center || sigmoid_slope)
            {
                params->sigmoid_data = std::make_unique<pl_sigmoid_params>(
                    (!render_data->sigmoid_params) ? pl_sigmoid_default_params : *render_data->sigmoid_params);

                if (!update_param(sigmoid_center, params->sigmoid_data->center, "sigmoid_center", msg, 0.0f, 1.0f))
                    return avs_err_val(env, msg);
                if (!update_param(sigmoid_slope, params->sigmoid_data->slope, "sigmoid_slope", msg, 1.0f, 20.0f))
                    return avs_err_val(env, msg);

                render_data->sigmoid_params = params->sigmoid_data.get();
            }
        }
    }

    // --- Deband ---
    {
        const auto deband{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"deband">())};
        const auto deband_it{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"deband_iterations">())};
        const auto deband_thr{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"deband_threshold">())};
        const auto deband_rad{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"deband_radius">())};
        const auto deband_gr{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"deband_grain">())};
        // const auto deband_gr_neutral{avs_helpers::get_opt_array_as_vector<float>(env, args, get_param_idx<"deband_grain_neutral">())};

        if (deband && !*deband)
        {
            render_data->deband_params = nullptr;
        }
        else if (deband || deband_it || deband_thr || deband_rad || deband_gr)
        {
            params->deband_data =
                std::make_unique<pl_deband_params>((!render_data->deband_params) ? pl_deband_default_params : *render_data->deband_params);
            const auto& deband_data{params->deband_data};

            if (!update_param(deband_it, deband_data->iterations, "deband_iterations", msg, 0, 16))
                return avs_err_val(env, msg);
            if (!update_param(deband_thr, deband_data->threshold, "deband_threshold", msg, 0.0f, 1000.0f))
                return avs_err_val(env, msg);
            if (!update_param(deband_rad, deband_data->radius, "deband_radius", msg, 0.0f, 1000.0f))
                return avs_err_val(env, msg);
            if (!update_param(deband_gr, deband_data->grain, "deband_grain", msg, 0.0f, 1000.0f))
                return avs_err_val(env, msg);

            /*
            if (const size_t grain_neutral_num{deband_gr_neutral.size()})
            {
                if (grain_neutral_num > src_num_comp)
                    return avs_new_value_error("libplacebo_Render: grain_neutral index out of range.");

                auto& grain_neutral{deband_data->grain_neutral};
                for (int i{0}; i < grain_neutral_num; ++i)
                {
                    grain_neutral[i] = deband_gr_neutral[i];
                    if (grain_neutral[i] < 0.0f)
                        return avs_new_value_error("libplacebo_Deband: grain_neutral must be greater than or equal to 0.0");
                }
            }
            */

            render_data->deband_params = deband_data.get();
        }
    }

    // --- Source Color Space ---
    params->src_num_planes = src_num_comp;
    params->src_comp_size = g_avs_api->avs_component_size(&vi);

    auto& src_frame{params->src_frame};
    src_frame = {
        .num_planes = src_num_comp,
        .crop = {.x0 = crop_x,
            .y0 = crop_y,
            .x1 = (crop_w > 0.0f) ? crop_x + crop_w : src_w + crop_w,
            .y1 = (crop_h > 0.0f) ? crop_y + crop_h : src_h + crop_h},
    };

    const auto opt_src_csp{avs_helpers::get_opt_arg<std::string_view>(env, args, get_param_idx<"src_csp">())};
    std::string src_csp{!opt_src_csp ? is_src_rgb ? "srgb" : "sdr" : *opt_src_csp};
    if (!apply_csp_preset(src_csp, src_frame.color, src_frame.repr))
        return avs_err_val(env, std::format("libplacebo_Render: Unknown src_csp preset '{}'.", src_csp));

    auto& src_sys{src_frame.repr.sys};

    const auto process_param = [&](std::optional<std::string> opt_val, const auto& map, auto& target, std::string_view name,
                                   bool* out_present = nullptr) -> std::optional<std::string> {
        if (out_present)
            *out_present = opt_src_csp || opt_val;

        if (opt_val)
        {
            if (const auto parsed = map.find(*opt_val))
                target = *parsed;
            else
                msg = std::format("libplacebo_Render: Invalid {} '{}'.", name, *opt_val);
        }
        return opt_val;
    };

    {
        if (process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_matrix">()), parse_matrix, src_sys,
                "src_matrix", &params->is_matrix_def);
            !msg.empty())
            return avs_err_val(env, msg);
        if (process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_trc">()), parse_trc, src_frame.color.transfer,
                "src_trc", &params->is_trc_def);
            !msg.empty())
            return avs_err_val(env, msg);
        if (process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_prim">()), parse_prim,
                src_frame.color.primaries, "src_prim", &params->is_prim_def);
            !msg.empty())
            return avs_err_val(env, msg);

        if (const auto specified{process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_levels">()),
                parse_levels, src_frame.repr.levels, "src_levels", &params->is_levels_def)};
            !msg.empty())
            return avs_err_val(env, msg);
        else if (!specified)
            src_frame.repr.levels = (is_src_rgb || src_bit_depth == 32) ? PL_COLOR_LEVELS_FULL : PL_COLOR_LEVELS_LIMITED;

        if (const auto specified{process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_alpha">()), parse_alpha,
                src_frame.repr.alpha, "src_alpha")};
            !msg.empty())
            return avs_err_val(env, msg);
        else if (!specified)
            src_frame.repr.alpha = (src_num_comp > 3) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE;

        if (const auto specified{process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"src_cplace">()),
                parse_cplace, params->src_cplace, "src_cplace")};
            !msg.empty())
            return avs_err_val(env, msg);
        else if (!specified)
            params->src_cplace = PL_CHROMA_LEFT;

        if (is_src_rgb)
        {
            if (src_sys != PL_COLOR_SYSTEM_RGB && src_sys != PL_COLOR_SYSTEM_XYZ && src_sys != PL_COLOR_SYSTEM_UNKNOWN)
                return avs_err_val(env, std::format("libplacebo_Render: Input is RGB, but src_matrix is set to a YUV format ({})",
                                            pl_color_system_name(src_sys)));
        }
        else
        {
            if (src_sys == PL_COLOR_SYSTEM_RGB)
                return avs_new_value_error("libplacebo_Render: Input is YUV, but src_matrix is set to RGB.");
        }

        if (src_sys == PL_COLOR_SYSTEM_BT_2100_PQ || src_sys == PL_COLOR_SYSTEM_BT_2100_HLG)
        {
            if (is_src_rgb)
                return avs_new_value_error("libplacebo_Render: BT.2100 ICtCp requires a YUV clip.");

            if (!pl_color_transfer_is_hdr(src_frame.color.transfer))
                return avs_new_value_error("libplacebo_Render: BT.2100 ICtCp requires an HDR transfer function (PQ/HLG).");
        }
    }

    // --- Destination color ---
    auto& dst_frame{params->dst_frame};

    const auto dst_csp{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_csp">())};
    std::string_view dst_csp_fmt;
    if (dst_csp)
    {
        if (*dst_csp == "dovi" || *dst_csp == "dolbyvision")
            return avs_new_value_error("libplacebo_Render: Dolby Vision dst_csp is not supported.");
        if (!apply_csp_preset(*dst_csp, dst_frame.color, dst_frame.repr, &dst_csp_fmt))
            return avs_err_val(env, std::format("libplacebo_Render: Unknown dst_csp preset '{}'", *dst_csp));
    }
    else
    {
        dst_frame.color = src_frame.color;
        dst_frame.repr = src_frame.repr;
    }

    const auto dst_matrix{process_param(
        avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_matrix">()), parse_matrix, dst_frame.repr.sys, "dst_matrix")};
    if (!msg.empty())
        return avs_err_val(env, msg);
    if (dst_frame.repr.sys == PL_COLOR_SYSTEM_DOLBYVISION)
        return avs_new_value_error("libplacebo_Render: Dolby Vision dst_matrix is not supported.");

    if (process_param(
            avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_trc">()), parse_trc, dst_frame.color.transfer, "dst_trc");
        !msg.empty())
        return avs_err_val(env, msg);
    if (process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_prim">()), parse_prim, dst_frame.color.primaries,
            "dst_prim");
        !msg.empty())
        return avs_err_val(env, msg);

    const auto dst_levels{process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_levels">()), parse_levels,
        dst_frame.repr.levels, "dst_levels")};
    if (!msg.empty())
        return avs_err_val(env, msg);
    const auto dst_alpha{process_param(
        avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_alpha">()), parse_alpha, dst_frame.repr.alpha, "dst_alpha")};
    if (!msg.empty())
        return avs_err_val(env, msg);
    // Handle default dst_alpha later with out_fmt.

    if (const auto s{process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dst_cplace">()), parse_cplace,
            params->dst_cplace, "dst_cplace")};
        !msg.empty())
        return avs_err_val(env, msg);
    else if (!s)
        params->dst_cplace = PL_CHROMA_LEFT;

    const pl_color_map_params* color_map_base{[&]() -> const pl_color_map_params* {
        if (const auto s{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"color_map_preset">())})
        {
            if (iequals(*s, "default"))
                return &pl_color_map_default_params;
            else if (iequals(*s, "high_quality"))
                return &pl_color_map_high_quality_params;
            else
                return nullptr;
        }
        else
        {
            return render_data->color_map_params ? render_data->color_map_params : &pl_color_map_high_quality_params;
        }
    }()};
    if (!color_map_base)
        return avs_new_value_error("libplacebo_Render: wrong color_map_preset.");

    // --- Gamut mapping ---
    auto& color_map_params{params->color_map_params};
    if (src_frame.color.primaries != dst_frame.color.primaries)
    {
        color_map_params = std::make_unique<pl_color_map_params>(*color_map_base);

        const auto g_mapping{avs_helpers::get_opt_arg<const char*>(env, args, get_param_idx<"gamut_mapping">())};
        const auto p_deadzone{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"perceptual_deadzone">())};
        const auto p_strength{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"perceptual_strength">())};
        const auto c_gamma{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"colorimetric_gamma">())};
        const auto s_knee{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"softclip_knee">())};
        const auto s_desat{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"softclip_desat">())};
        const auto lut3d_size_i{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"lut3d_size_i">())};
        const auto lut3d_size_c{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"lut3d_size_c">())};
        const auto lut3d_size_h{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"lut3d_size_h">())};
        const auto lut3d_tricubic{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"lut3d_tricubic">())};
        const auto g_expansion{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"gamut_expansion">())};

        if (g_mapping || p_deadzone || p_strength || c_gamma || s_knee || s_desat || lut3d_size_i || lut3d_size_c || lut3d_size_h ||
            lut3d_tricubic || g_expansion)
        {
            if (g_mapping)
            {
                color_map_params->gamut_mapping = pl_find_gamut_map_function(*g_mapping);
                if (!color_map_params->gamut_mapping)
                    return avs_err_val(env, std::format("libplacebo_Render: wrong gamut_mapping '{}'.", std::string(*g_mapping)));
            }

            auto& gamut_constants{color_map_params->gamut_constants};
            if (!update_param(p_deadzone, gamut_constants.perceptual_deadzone, "perceptual_deadzone", msg, 0.0f, 1.0f))
                return avs_err_val(env, msg);
            if (!update_param(p_strength, gamut_constants.perceptual_strength, "perceptual_strength", msg, 0.0f, 1.0f))
                return avs_err_val(env, msg);
            if (!update_param(c_gamma, gamut_constants.colorimetric_gamma, "colorimetric_gamma", msg, 0.0f, 10.0f))
                return avs_err_val(env, msg);
            if (!update_param(s_knee, gamut_constants.softclip_knee, "softclip_knee", msg, 0.0f, 1.0f))
                return avs_err_val(env, msg);
            if (!update_param(s_desat, gamut_constants.softclip_desat, "softclip_desat", msg, 0.0f, 1.0f))
                return avs_err_val(env, msg);
            if (!update_param(lut3d_size_i, color_map_params->lut3d_size[0], "lut3d_size_i", msg, 0, 1024))
                return avs_err_val(env, msg);
            if (!update_param(lut3d_size_c, color_map_params->lut3d_size[1], "lut3d_size_c", msg, 0, 1024))
                return avs_err_val(env, msg);
            if (!update_param(lut3d_size_h, color_map_params->lut3d_size[2], "lut3d_size_h", msg, 0, 1024))
                return avs_err_val(env, msg);
            update_param(lut3d_tricubic, color_map_params->lut3d_tricubic);
            update_param(g_expansion, color_map_params->gamut_expansion);
        }
    }

    // --- Dither ---
    {
        const auto dither{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"dither">())};
        const auto dither_m{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"dither_method">())};
        const auto dither_lut_s{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"dither_lut_size">())};
        const auto dither_temp{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"dither_temporal">())};

        if (dither && !*dither)
        {
            render_data->dither_params = nullptr;
            render_data->error_diffusion = nullptr;
        }
        else if ((dither && *dither) || dither_m || dither_lut_s || dither_temp)
        {
            if (dither_m)
            {
                const auto parsed{parse_dither_m.find(*dither_m)};
                if (parsed)
                {
                    params->dither_data = std::make_unique<pl_dither_params>(
                        (!render_data->dither_params) ? pl_dither_default_params : *render_data->dither_params);
                    params->dither_data->method = static_cast<pl_dither_method>(*parsed);
                }
                else if (!iequals(*dither_m, "error_diffusion"))
                {
                    return avs_err_val(env, std::format("libplacebo_Render: Invalid dither_method '{}'.", *dither_m));
                }
                else
                {
                    if (const auto s{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"error_diffusion_k">())})
                    {
                        const auto parsed{parse_error_diffusion_k.find(*s)};
                        if (parsed)
                            render_data->error_diffusion = pl_find_error_diffusion_kernel((*parsed).data());
                        else
                            return avs_err_val(env, std::format("libplacebo_Render: Invalid error_diffusion_k '{}'.", *s));
                    }
                    else
                    {
                        render_data->error_diffusion = pl_find_error_diffusion_kernel("floyd-steinberg");
                    }
                }
            }

            if (!render_data->error_diffusion)
            {
                auto& dither_data{params->dither_data};
                if (!dither_data)
                    dither_data = std::make_unique<pl_dither_params>(
                        (!render_data->dither_params) ? pl_dither_default_params : *render_data->dither_params);

                if (!update_param(dither_lut_s, dither_data->lut_size, "dither_lut_size", msg, 1, 8))
                    return avs_err_val(env, msg);

                update_param(dither_temp, dither_data->temporal);

                render_data->dither_params = dither_data.get();
            }
        }
    }

    // --- Custom Lut File ---
    const auto opt_lut{avs_helpers::get_opt_arg<const char*>(env, args, get_param_idx<"lut">())};
    if (opt_lut)
    {
        auto content{load_file_content(*opt_lut, msg)};
        if (!content)
            return avs_err_val(env, std::format("libplacebo_Render: {}", msg));

        params->lut_ptr.reset(pl_lut_parse_cube(nullptr, content->c_str(), content->size()));
        if (!params->lut_ptr)
            return avs_new_value_error("libplacebo_Render: failed lut parsing.");

        render_data->lut = params->lut_ptr.get();

        if (process_param(avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"lut_type">()), parse_lut_type,
                render_data->lut_type, "lut_type");
            !msg.empty())
            return avs_err_val(env, msg);
    }

    // --- Tone Mapping Function ---
    {
        const auto tone_mapping_f{avs_helpers::get_opt_arg<const char*>(env, args, get_param_idx<"tone_mapping_function">())};
        const auto tone_constants{avs_helpers::get_opt_array_as_vector<std::string_view>(env, args, get_param_idx<"tone_constants">())};
        const auto inverse_tone_mapping{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"inverse_tone_mapping">())};
        const auto tone_lut_size{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"tone_lut_size">())};
        const auto contrast_recovery{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"contrast_recovery">())};
        const auto contrast_smoothness{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"contrast_smoothness">())};

        const auto peak_detect{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"peak_detect">())};
        const auto peak_detection_preset{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"peak_detection_preset">())};
        const auto peak_smoothing_period{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"peak_smoothing_period">())};
        const auto scene_threshold_low{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"scene_threshold_low">())};
        const auto scene_threshold_high{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"scene_threshold_high">())};
        const auto peak_percentile{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"peak_percentile">())};
        const auto black_cutoff{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"black_cutoff">())};

        const auto src_max{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_max">())};
        const auto src_min{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"src_min">())};
        const auto dst_max{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"dst_max">())};
        const auto dst_min{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"dst_min">())};

        const auto tone_map_metadata{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"tone_map_metadata">())};
        const auto dovi_metadata{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"dovi_metadata">())};

        if (tone_mapping_f || !tone_constants.empty() || inverse_tone_mapping || tone_lut_size || contrast_recovery ||
            contrast_smoothness || peak_detect || peak_detection_preset || peak_smoothing_period || scene_threshold_low ||
            scene_threshold_high || peak_percentile || black_cutoff || src_max || src_min || dst_max || dst_min || tone_map_metadata ||
            dovi_metadata)
        {
            if (dovi_metadata || src_sys == PL_COLOR_SYSTEM_DOLBYVISION)
            {
                if (dovi_metadata && *dovi_metadata && src_sys != PL_COLOR_SYSTEM_DOLBYVISION)
                    return avs_new_value_error("libplacebo_Render: dovi_metadata requires dovi matrix");

                params->dovi_meta = std::make_unique<pl_dovi_metadata>();
            }

            if (!color_map_params)
                color_map_params = std::make_unique<pl_color_map_params>(*color_map_base);

            if (tone_mapping_f)
            {
                color_map_params->tone_mapping_function = pl_find_tone_map_function(*tone_mapping_f);
                if (!color_map_params->tone_mapping_function)
                    return avs_new_value_error("libplacebo_Render: wrong tone_mapping_function.");
            }

            if (!tone_constants.empty())
            {
                struct tone_const_def
                {
                    std::string_view name;
                    float pl_tone_map_constants::* member;
                    float min;
                    float max;
                };

                static constexpr std::array constants_map{
                    tone_const_def{"knee_adaptation", &pl_tone_map_constants::knee_adaptation, 0.0f, 1.0f},
                    tone_const_def{"knee_minimum", &pl_tone_map_constants::knee_minimum, 0.0f, 0.5f},
                    tone_const_def{"knee_maximum", &pl_tone_map_constants::knee_maximum, 0.5f, 1.0f},
                    tone_const_def{"knee_default", &pl_tone_map_constants::knee_default, 0.0f, 1.0f},
                    tone_const_def{"knee_offset", &pl_tone_map_constants::knee_offset, 0.5f, 2.0f},
                    tone_const_def{"slope_tuning", &pl_tone_map_constants::slope_tuning, 0.0f, 10.0f},
                    tone_const_def{"slope_offset", &pl_tone_map_constants::slope_offset, 0.0f, 1.0f},
                    tone_const_def{"spline_contrast", &pl_tone_map_constants::spline_contrast, 0.0f, 1.5f},
                    tone_const_def{"reinhard_contrast", &pl_tone_map_constants::reinhard_contrast, 0.0f, 1.0f},
                    tone_const_def{"linear_knee", &pl_tone_map_constants::linear_knee, 0.0f, 1.0f},
                    tone_const_def{"exposure", &pl_tone_map_constants::exposure, 0.0f, 10.0f},
                };

                if (tone_constants.size() > 11)
                    return avs_new_value_error("libplacebo_Render: tone_constants must be equal to or less than 11.");

                for (int i{0}; i < tone_constants.size(); ++i)
                {
                    const auto& entry{tone_constants[i]};
                    const size_t eq_pos = entry.find('=');
                    if (eq_pos == std::string_view::npos)
                        return avs_new_value_error("libplacebo_Render: tone_constants must use 'key=value' format.");

                    std::string_view key{entry.substr(0, eq_pos)};
                    std::string_view val_str{entry.substr(eq_pos + 1)};
                    const auto it{std::ranges::find_if(constants_map, [&](const tone_const_def& item) { return iequals(key, item.name); })};
                    if (it == constants_map.end())
                        return avs_err_val(env, std::format("libplacebo_Render: unknown tone_constant '{}'.", key));

                    float constant_value;
                    const auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), constant_value);
                    if (ec != std::errc())
                        return avs_new_value_error("libplacebo_Render: invalid numeric value.");

                    if (constant_value < it->min || constant_value > it->max)
                        return avs_err_val(env, std::format("{} must be between {}..{}.", it->name, it->min, it->max));

                    color_map_params->tone_constants.*(it->member) = constant_value;
                }
            }

            update_param(inverse_tone_mapping, color_map_params->inverse_tone_mapping);
            if (!update_param(tone_lut_size, color_map_params->lut_size, "tone_lut_size", msg, 0, 4096))
                return avs_err_val(env, msg);
            if (!update_param(contrast_recovery, color_map_params->contrast_recovery, "contrast_recovery", msg, 0.0f, 2.0f))
                return avs_err_val(env, msg);
            if (!update_param(contrast_smoothness, color_map_params->contrast_smoothness, "contrast_smoothness", msg, 1.0f, 32.0f))
                return avs_err_val(env, msg);

            {
                if (peak_detect && !*peak_detect)
                {
                    render_data->peak_detect_params = nullptr;
                }
                else if (peak_detect || peak_detection_preset || peak_smoothing_period || scene_threshold_low || scene_threshold_high ||
                         peak_percentile || black_cutoff)
                {
                    auto& peak_detect_params{params->peak_detect_params};
                    if (peak_detection_preset)
                    {
                        if (iequals(*peak_detection_preset, "high_quality"))
                            peak_detect_params = std::make_unique<pl_peak_detect_params>(pl_peak_detect_high_quality_params);
                        else if (iequals(*peak_detection_preset, "default"))
                            peak_detect_params = std::make_unique<pl_peak_detect_params>(pl_peak_detect_default_params);
                        else
                            return avs_err_val(
                                env, std::format("libplacebo_Render: invalid peak_detection_preset '{}'.", *peak_detection_preset));
                    }
                    else
                    {
                        peak_detect_params = std::make_unique<pl_peak_detect_params>(
                            (!render_data->peak_detect_params) ? pl_peak_detect_high_quality_params : *render_data->peak_detect_params);
                    }

                    if (!update_param(
                            peak_smoothing_period, peak_detect_params->smoothing_period, "peak_smoothing_period", msg, 0.0f, 1000.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(
                            scene_threshold_low, peak_detect_params->scene_threshold_low, "scene_threshold_low", msg, 0.0f, 100.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(
                            scene_threshold_high, peak_detect_params->scene_threshold_high, "scene_threshold_high", msg, 0.0f, 100.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(peak_percentile, peak_detect_params->percentile, "peak_percentile", msg, 0.0f, 100.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(black_cutoff, peak_detect_params->black_cutoff, "black_cutoff", msg, 0.0f, 100.0f))
                        return avs_err_val(env, msg);

                    render_data->peak_detect_params = peak_detect_params.get();
                }
            }

            if (process_param(tone_map_metadata, parse_hdr_metadata, color_map_params->metadata, "tone_map_metadata"); !msg.empty())
                return avs_err_val(env, msg);

            params->is_src_hdr_max_luma_def = src_max.has_value();
            update_param(src_max, src_frame.color.hdr.max_luma);

            params->is_src_hdr_min_luma_def = src_min.has_value();
            if (!update_param(src_min, src_frame.color.hdr.min_luma, "src_min", msg, 0.0f))
                return avs_err_val(env, msg);

            update_param(dst_max, dst_frame.color.hdr.max_luma);
            if (!update_param(dst_min, dst_frame.color.hdr.min_luma, "dst_min", msg, 0.0f))
                return avs_err_val(env, msg);

            if (src_frame.color.hdr.max_luma < src_frame.color.hdr.min_luma)
                return avs_err_val(env, std::format("libplacebo_Render: src_max ({}) must be not less than src_min ({}).",
                                            src_frame.color.hdr.max_luma, src_frame.color.hdr.min_luma));
            if (dst_frame.color.hdr.max_luma < dst_frame.color.hdr.min_luma)
                return avs_err_val(env, std::format("libplacebo_Render: dst_max ({}) must be not less than dst_min ({}).",
                                            dst_frame.color.hdr.max_luma, dst_frame.color.hdr.min_luma));
        }
    }

    // --- Color Adjustment ---
    {
        const auto opt_bright{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"brightness">())};
        const auto opt_cont{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"contrast">())};
        const auto opt_sat{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"saturation">())};
        const auto opt_hue{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"hue">())};
        const auto opt_gamma{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"gamma">())};
        const auto opt_temp{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"temperature">())};

        if (opt_bright || opt_cont || opt_sat || opt_hue || opt_gamma || opt_temp)
        {
            params->color_adjustment_params = std::make_unique<pl_color_adjustment>(
                (!render_data->color_adjustment) ? pl_color_adjustment_neutral : *render_data->color_adjustment);

            if (!update_param(opt_bright, params->color_adjustment_params->brightness, "brightness", msg, -1.0f, 1.0f))
                return avs_err_val(env, msg);
            if (!update_param(opt_cont, params->color_adjustment_params->contrast, "contrast", msg, 0.0f, 100.0f))
                return avs_err_val(env, msg);
            if (!update_param(opt_sat, params->color_adjustment_params->saturation, "saturation", msg, 0.0f, 100.0f))
                return avs_err_val(env, msg);
            update_param(opt_hue, params->color_adjustment_params->hue);
            if (!update_param(opt_gamma, params->color_adjustment_params->gamma, "gamma", msg, 0.0f, 100.0f))
                return avs_err_val(env, msg);
            if (!update_param(opt_temp, params->color_adjustment_params->temperature, "temperature", msg, -1.143f, 5.286f))
                return avs_err_val(env, msg);

            render_data->color_adjustment = params->color_adjustment_params.get();
        }
    }

    // --- Deinterlace ---
    {
        const auto field{avs_helpers::get_opt_arg<int>(env, args, get_param_idx<"field">())};
        const auto deint_algo{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"deinterlace_algo">())};
        const auto spatial_check{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"spatial_check">())};

        if (field || deint_algo || spatial_check)
        {
            if (!update_param(field, params->field, "field", msg, -2, 3))
                return avs_err_val(env, msg);
            if (!field)
                params->field = -1;

            if (params->field == -2 || params->field > 1)
            {
                vi.num_frames <<= 1;
                vi.fps_numerator <<= 1;
            }

            params->deinterlace_data = std::make_unique<pl_deinterlace_params>(
                (!render_data->deinterlace_params) ? pl_deinterlace_default_params : *render_data->deinterlace_params);
            const auto& deinterlace_data{params->deinterlace_data};

            if (const auto specified{process_param(deint_algo, parse_deint_algo, deinterlace_data->algo, "deinterlace_algo")}; !msg.empty())
                return avs_err_val(env, msg);
            else if (!specified)
                deinterlace_data->algo = PL_DEINTERLACE_YADIF;

            update_param(spatial_check, params->deinterlace_data->skip_spatial_check, [](bool val) { return !val; });

            render_data->deinterlace_params = params->deinterlace_data.get();
        }
        else
        {
            params->field = -1;
        }
    }

    // --- Debug ---
    {
        const auto vis_lut{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"visualize_lut">())};
        const auto vis_x0{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_lut_x0">())};
        const auto vis_y0{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_lut_y0">())};
        const auto vis_x1{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_lut_x1">())};
        const auto vis_y1{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_lut_y1">())};
        const auto vis_hue{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_hue">())};
        const auto vis_theta{avs_helpers::get_opt_arg<float>(env, args, get_param_idx<"visualize_theta">())};
        const auto show_clipping{avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"show_clipping">())};

        if (vis_lut || vis_x0 || vis_y0 || vis_x1 || vis_y1 || vis_hue || vis_theta || show_clipping)
        {
            if (!color_map_params)
                color_map_params = std::make_unique<pl_color_map_params>(*color_map_base);

            if (vis_lut)
            {
                color_map_params->visualize_lut = *vis_lut;

                if (*vis_lut)
                {
                    if (!update_param(vis_x0, color_map_params->visualize_rect.x0, "visualize_lut_x0", msg, 0.0f, 1.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(vis_y0, color_map_params->visualize_rect.y0, "visualize_lut_y0", msg, 0.0f, 1.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(vis_x1, color_map_params->visualize_rect.x1, "visualize_lut_x1", msg, 0.0f, 1.0f))
                        return avs_err_val(env, msg);
                    if (!update_param(vis_y1, color_map_params->visualize_rect.y1, "visualize_lut_y1", msg, 0.0f, 1.0f))
                        return avs_err_val(env, msg);
                    update_param(vis_hue, color_map_params->visualize_hue);
                    update_param(vis_theta, color_map_params->visualize_theta);
                }
            }

            update_param(show_clipping, color_map_params->show_clipping);
        }
    }

    // --- Global Render Params ---
    if (!update_param(avs_helpers::get_opt_arg<bool>(env, args, get_param_idx<"corner_rounding">()), render_data->corner_rounding,
            "corner_rounding", msg, 0.0f, 1.0f))
        return avs_err_val(env, msg);

    if (color_map_params)
        render_data->color_map_params = color_map_params.get();

    // --- Output format ---
    if (const auto out_fmt{avs_helpers::get_opt_arg<std::string>(env, args, get_param_idx<"out_fmt">())}; out_fmt || dst_csp)
    {
        if (out_fmt && dst_csp)
            return avs_new_value_error("libplacebo_Render: out_fmt and dst_csp cannot be specified at the same time.");

        bool is_rgb{};
        if (!parse_out_fmt((dst_csp) ? dst_csp_fmt : *out_fmt, &vi, is_rgb))
            return avs_err_val(env, std::format("libplacebo_Render: Invalid out_fmt '{}'.", *out_fmt));

        if (is_rgb != (dst_frame.repr.sys == PL_COLOR_SYSTEM_RGB))
        {
            if (dst_matrix)
                return avs_err_val(env, std::format("libplacebo_Render: invalid dst_matrix '{}'.", *dst_matrix));
            else
                dst_frame.repr.sys = (is_rgb) ? PL_COLOR_SYSTEM_RGB : PL_COLOR_SYSTEM_BT_709;

            if (!dst_levels)
                dst_frame.repr.levels =
                    (is_rgb || (g_avs_api->avs_component_size(&vi) == 4)) ? PL_COLOR_LEVELS_FULL : PL_COLOR_LEVELS_LIMITED;
        }

        params->dst_num_planes = g_avs_api->avs_num_components(&vi);
        if (render_data->error_diffusion && params->dst_num_planes > 1)
            return avs_new_value_error("libplacebo_Render: error_diffusion support only one plane formats.");

        if (dst_alpha)
        {
            if ((dst_frame.repr.alpha == PL_ALPHA_NONE && params->dst_num_planes > 3) ||
                (dst_frame.repr.alpha != PL_ALPHA_NONE && params->dst_num_planes < 4))
                return avs_err_val(env, std::format("libplacebo_Render: invalid dst_alpha '{}'.", *dst_alpha));
        }
        else
        {
            dst_frame.repr.alpha = (params->dst_num_planes > 3) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE;
        }
    }
    else
    {
        params->dst_num_planes = src_num_comp;

        if (is_src_rgb != (dst_frame.repr.sys == PL_COLOR_SYSTEM_RGB))
            return avs_new_value_error("libplacebo_Render: out_fmt is not known.");
    }

    // --- Libplacebo ---
    params->src_planes = is_src_rgb ? decltype(params->src_planes){AVS_PLANAR_R, AVS_PLANAR_G, AVS_PLANAR_B, AVS_PLANAR_A}
                                    : decltype(params->src_planes){AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V, AVS_PLANAR_A};

    params->dst_planes = avs_is_rgb(&vi) ? decltype(params->dst_planes){AVS_PLANAR_R, AVS_PLANAR_G, AVS_PLANAR_B, AVS_PLANAR_A}
                                         : decltype(params->dst_planes){AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V, AVS_PLANAR_A};

    src_frame.repr.bits.color_depth = src_bit_depth;
    const int src_sample_depth{params->src_comp_size * 8};
    const pl_fmt_caps src_caps{(src_sample_depth == 32)
                                   ? static_cast<pl_fmt_caps>(PL_FMT_CAP_SAMPLEABLE | PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_BLITTABLE)
                                   : PL_FMT_CAP_SAMPLEABLE};

    pl_fmt src_fmt{
        pl_find_fmt(gpu, (src_sample_depth < 32) ? PL_FMT_UNORM : PL_FMT_FLOAT, 1, src_sample_depth, src_sample_depth, src_caps)};
    if (!src_fmt)
        src_fmt = pl_find_fmt(gpu, PL_FMT_UNORM, 1, 16, 16, src_caps);
    if (!src_fmt)
        return avs_new_value_error("libplacebo_Render: couldn't find src_fmt.");

    params->src_fmt_type = src_fmt->type;
    src_frame.repr.bits.sample_depth = src_fmt->component_depth[0];

    for (int i{0}; i < src_num_comp; ++i)
    {
        src_frame.planes[i].components = 1;
        src_frame.planes[i].component_mapping[0] = i;
    }

    if (params->deinterlace_data && (params->field > -1))
        src_frame.first_field = (params->field == 1 || params->field == 3) ? PL_FIELD_TOP : PL_FIELD_BOTTOM;

    const int dst_sample_depth{g_avs_api->avs_component_size(&vi) * 8};
    pl_fmt_caps dst_caps{(dst_sample_depth == 32 || src_bit_depth == 32)
                             ? static_cast<pl_fmt_caps>(PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE | PL_FMT_CAP_SAMPLEABLE)
                             : static_cast<pl_fmt_caps>(PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE)};
    if (render_data->error_diffusion)
        dst_caps = static_cast<pl_fmt_caps>(dst_caps | PL_FMT_CAP_STORABLE);

    const pl_fmt dst_fmt{
        pl_find_fmt(gpu, (dst_sample_depth < 32) ? PL_FMT_UNORM : PL_FMT_FLOAT, 1, dst_sample_depth, dst_sample_depth, dst_caps)};
    if (!dst_fmt)
        return avs_new_value_error("libplacebo_Render: couldn't find dst_fmt.");

    dst_frame.repr.bits.color_depth = g_avs_api->avs_bits_per_component(&vi);
    dst_frame.repr.bits.sample_depth = dst_fmt->component_depth[0];

    dst_frame.num_planes = params->dst_num_planes;

    const bool dst_props{(dst_frame.repr.sys == PL_COLOR_SYSTEM_RGB) || (g_avs_api->avs_num_components(&vi) == 1)};
    const int dst_sub_w{(dst_props) ? 0 : g_avs_api->avs_get_plane_width_subsampling(&vi, AVS_PLANAR_U)};
    const int dst_sub_h{(dst_props) ? 0 : g_avs_api->avs_get_plane_height_subsampling(&vi, AVS_PLANAR_U)};

    for (int i{0}; i < params->dst_num_planes; ++i)
    {
        const pl_tex_params t_r{
            .w = (i) ? (vi.width >> dst_sub_w) : vi.width,
            .h = (i) ? (vi.height >> dst_sub_h) : vi.height,
            .format = dst_fmt,
            .sampleable = (dst_sample_depth == 32 || src_bit_depth == 32),
            .renderable = true,
            .host_readable = true,
        };
        auto& tex_out{params->vf->tex_out};

        if (!pl_tex_recreate(gpu, &tex_out[i], &t_r))
            return avs_new_value_error("libplacebo_Render: cannot allocate out texture.");

        dst_frame.planes[i].texture = tex_out[i];
        dst_frame.planes[i].components = 1;
        dst_frame.planes[i].component_mapping[0] = i;
    }

    AVS_Value v;
    g_avs_api->avs_set_to_clip(&v, clip);

    fi->user_data = params.release();
    fi->get_frame = render_get_frame;
    fi->set_cache_hints = render_set_cache_hints;
    fi->get_parity = render_get_parity;
    fi->free_filter = free_render;

    return v;
}
