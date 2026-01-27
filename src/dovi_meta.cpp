#include <algorithm>

#include "libplacebo_render.h"

std::unique_ptr<pl_dovi_metadata> create_dovi_meta(DoviRpuOpaque* rpu, const DoviRpuDataHeader& hdr)
{
    std::unique_ptr<pl_dovi_metadata> dovi_meta{std::make_unique<pl_dovi_metadata>()}; // persist state

    if (hdr.use_prev_vdr_rpu_flag)
        return dovi_meta;

    if (dovi_ptr<const DoviRpuDataMapping> mapping{dovi_rpu_get_data_mapping(rpu)})
    {
        const uint64_t bits{hdr.bl_bit_depth_minus8 + 8};
        const float scale{1.0f / (1 << hdr.coefficient_log2_denom)};

        for (int c{0}; c < 3; ++c)
        {
            const auto& curve{mapping->curves[c]};
            auto& cmp{dovi_meta->comp[c]};

            cmp.num_pivots = curve.pivots.len;
            std::ranges::fill(cmp.method, curve.mapping_idc);

            uint16_t pivot{0};
            for (int pivot_idx{0}; pivot_idx < cmp.num_pivots; ++pivot_idx)
            {
                pivot += curve.pivots.data[pivot_idx];
                cmp.pivots[pivot_idx] = static_cast<float>(pivot) / ((1 << bits) - 1);
            }

            for (int i{0}; i < cmp.num_pivots - 1; ++i)
            {
                std::fill(std::begin(cmp.poly_coeffs[i]), std::end(cmp.poly_coeffs[i]), 0.0f);

                if (curve.polynomial)
                {
                    for (int k{0}; k <= curve.polynomial->poly_order_minus1.data[i] + 1; ++k)
                        cmp.poly_coeffs[i][k] =
                            curve.polynomial->poly_coef_int.list[i]->data[k] + scale * curve.polynomial->poly_coef.list[i]->data[k];
                }
                else if (curve.mmr)
                {
                    cmp.mmr_constant[i] = curve.mmr->mmr_constant_int.data[i] + scale * curve.mmr->mmr_constant.data[i];
                    cmp.mmr_order[i] = curve.mmr->mmr_order_minus1.data[i] + 1;

                    for (int j{0}; j < cmp.mmr_order[i]; ++j)
                    {
                        for (int k{0}; k < 7; ++k)
                            cmp.mmr_coeffs[i][j][k] =
                                curve.mmr->mmr_coef_int.list[i]->list[j]->data[k] + scale * curve.mmr->mmr_coef.list[i]->list[j]->data[k];
                    }
                }
            }
        }
    }

    if (hdr.vdr_dm_metadata_present_flag)
    {
        if (dovi_ptr<const DoviVdrDmData> dm_data{dovi_rpu_get_vdr_dm_data(rpu)})
        {

            const auto& off{&dm_data->ycc_to_rgb_offset0};
            for (int i{0}; i < 3; ++i)
                dovi_meta->nonlinear_offset[i] = static_cast<float>(off[i]) / (1 << 28);

            const auto& ycc_to_rgb{&dm_data->ycc_to_rgb_coef0};
            auto dst_nonlinear{std::span{dovi_meta->nonlinear.m[0], 9}};
            for (int i{0}; i < 9; ++i)
                dst_nonlinear[i] = ycc_to_rgb[i] / 8192.0f;

            const auto& rgb_to_lms{&dm_data->rgb_to_lms_coef0};
            auto dst_linear{std::span{dovi_meta->linear.m[0], 9}};
            for (int i{0}; i < 9; ++i)
                dst_linear[i] = rgb_to_lms[i] / 16384.0f;
        }
    }

    return dovi_meta;
}
