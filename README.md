## Description

An AviSynth+ plugin interface to [libplacebo](https://code.videolan.org/videolan/libplacebo) -
a reusable library for GPU-accelerated image/video processing primitives and shaders.

### Requirements:

- Vulkan device

- AviSynth+ r3688 or later ([1](https://github.com/AviSynth/AviSynthPlus/releases) / [2](https://forum.doom9.org/showthread.php?t=181351) / [3](https://gitlab.com/uvz/AviSynthPlus-Builds))

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases))

[Usage](#usage)<br>

[Parameters](#parameters)<br>
[Core & Geometry](#core--geometry)<br>
[Scaling](#scaling)<br>
[Image Enhancement](#image-enhancement)<br>
[Color Space Definitions](#color-space-definitions)<br>
[Tone & Gamut Mapping](#tone-gamut-mapping)<br>
[HDR & Metadata](#hdr--metadata)<br>
[Color Adjustments](#color-adjustments)<br>
[Deinterlacing](#deinterlacing)<br>
[Dithering](#dithering)<br>
[Advanced & System](#advanced--system)<br>

[Building](#building)<br>

#### Usage:

```
libplacebo_Render(clip clip,
string "preset",
int "width",
int "height",
float "src_left",
float "src_top",
float "src_width",
float "src_height",
string "custom_shader_path",
string "custom_shader_param",
string "upscaler",
string "upscaler_kernel",
string "upscaler_window",
float "upscaler_radius",
float "upscaler_clamp",
float "upscaler_taper",
float "upscaler_blur",
float "upscaler_antiring",
float "upscaler_param1",
float "upscaler_param2",
float "upscaler_wparam1",
float "upscaler_wparam2",
bool "upscaler_polar",
string "downscaler",
string "downscaler_kernel",
string "downscaler_window",
float "downscaler_radius",
float "downscaler_clamp",
float "downscaler_taper",
float "downscaler_blur",
float "downscaler_antiring",
float "downscaler_param1",
float "downscaler_param2",
float "downscaler_wparam1",
float "downscaler_wparam2",
bool "downscaler_polar",
string "plane_upscaler",
string "plane_upscaler_kernel",
string "plane_upscaler_window",
float "plane_upscaler_radius",
float "plane_upscaler_clamp",
float "plane_upscaler_taper",
float "plane_upscaler_blur",
float "plane_upscaler_antiring",
float "plane_upscaler_param1",
float "plane_upscaler_param2",
float "plane_upscaler_wparam1",
float "plane_upscaler_wparam2",
bool "plane_upscaler_polar",
string "plane_downscaler",
string "plane_downscaler_kernel",
string "plane_downscaler_window",
float "plane_downscaler_radius",
float "plane_downscaler_clamp",
float "plane_downscaler_taper",
float "plane_downscaler_blur",
float "plane_downscaler_antiring",
float "plane_downscaler_param1",
float "plane_downscaler_param2",
float "plane_downscaler_wparam1",
float "plane_downscaler_wparam2",
bool "plane_downscaler_polar",
float "antiringing_strength",
bool "linear_scaling",
bool "sigmoid",
float "sigmoid_center",
float "sigmoid_slope",
bool "deband",
int "deband_iterations",
float "deband_threshold",
float "deband_radius",
float "deband_grain",
string "src_csp",
string "src_matrix",
string "src_trc",
string "src_prim",
string "src_levels",
string "src_alpha",
string "src_cplace",
string "dst_csp",
string "dst_matrix",
string "dst_trc",
string "dst_prim",
string "dst_levels",
string "dst_alpha",
string "dst_cplace",
string "color_map_preset",
string "gamut_mapping",
float "perceptual_deadzone",
float "perceptual_strength",
float "colorimetric_gamma",
float "softclip_knee",
float "softclip_desat",
int "lut3d_size_i",
int "lut3d_size_c",
int "lut3d_size_h",
bool "lut3d_tricubic",
bool "gamut_expansion",
string "tone_mapping_function",
string[] "tone_constants",
bool "inverse_tone_mapping",
int "tone_lut_size",
float "contrast_recovery",
float "contrast_smoothness",
bool "peak_detect",
string "peak_detection_preset",
float "peak_smoothing_period",
float "scene_threshold_low",
float "scene_threshold_high",
float "peak_percentile",
float "black_cutoff",
float "src_max",
float "src_min",
float "dst_max",
float "dst_min",
string "tone_map_metadata",
bool "dovi_metadata",
float "brightness",
float "contrast",
float "saturation",
float "hue",
float "gamma",
float "temperature",
string "deinterlace_algo",
int "field",
bool "spatial_check",
bool "dither",
string "dither_method",
int "dither_lut_size",
bool "dither_temporal",
string "error_diffusion_k",
bool "visualize_lut",
float "visualize_lut_x0",
float "visualize_lut_y0",
float "visualize_lut_x1",
float "visualize_lut_y1",
float "visualize_hue",
float "visualize_theta",
bool "show_clipping",
string "out_fmt",
string "lut",
string "lut_type",
float "corner_rounding",
int "device",
bool "list_device",
string "cache_path")
```

[Back to top](#description)

#### Parameters:

##### ***`clip`***
A clip to process.<br>
It must be in planar format.

##### ***Core & Geometry***

##### ***`preset`***
Override all options from all sections by the values from the given preset.<br>
* `"fast"` - Disable all advanced rendering. It sets color_map settings to `pl_color_map_default_params`.
* `"default"` - `"fast"` and the following:
    * upscaler=lanczos
    * downscaler=hermite
    * sigmoid enabled
    * dither enabled
    * peak_detect=`default`
* `"high_quality"`:
    * upscaler=ewa_lanczossharp
    * downscaler=hermite
    * sigmoid enabled
    * peak_detect=`high_quality`
    * color_map=`high_quality`
    * dither enabled
    * deband enabled
Default: not specified (all advanced rendering disabled, color_map settings - `pl_color_map_high_quality_params`).

##### ***`width` / `height`***
The width / height of the output.<br>
Default: not specified.

##### ***`src_left` / `src_top`***
Cropping of the left / top edge.<br>
Default: `0.0`.

##### ***`src_width` / `src_height`***
If `> 0.0` it sets the width / height of the clip before resizing.<br>
If `<= 0.0` it sets the cropping of the right / bottom edge before resizing.<br>
Default: Source width / height.

##### ***`out_fmt`***
Explicitly set the output pixel format (e.g., `"YV12"`, `"RGBPS"`, `"YUV420P10"`).<br>
Default: not specified.

[Back to top](#description)

##### ***Scaling***

##### ***`upscaler`/ `downscaler` / `plane_upscaler` / `plane_downscaler`***
* `"spline16"` (2 taps)
* `"spline36"` (3 taps)
* `"spline64"` (4 taps)
* `"nearest"`
* `"bilinear"` (resizable)
* `"triangle"` (bilinear alias)
* `"gaussian"` (resizable)
  Sinc family (all configured to 3 taps):
* `"sinc"` (unwindowed) (resizable)
* `"lanczos"` (sinc-sinc) (resizable)
* `"ginseng"` (sinc-jinc) (resizable)
* `"ewa_jinc"` (unwindowed) (resizable)
* `"ewa_lanczos"` (jinc-jinc) (resizable)
* `"ewa_lanczossharp"` (jinc-jinc) (resizable)
* `"ewa_lanczos4sharpest"` (jinc-jinc) (resizable)
* `"ewa_ginseng"` (jinc-sinc) (resizable)
* `"ewa_hann"` (jinc-hann) (resizable)
* `"ewa_hanning"` (ewa_hann alias)
  Spline family:
* `"bicubic"`
* `"triangle"` (bicubic alias)
* `"hermite"`
* `"catmull_rom"`
* `"mitchell"`
* `"robidoux"`
* `"robidouxsharp"`
* `"ewa_robidoux"`
* `"ewa_robidouxsharp"`

plane_upscaler / plane_downscaler is used for chroma scaling when internally converting to RGB.

Default: if `preset` is not specified:<br>
* upscaler=ewa_lanczossharp
* downscaler=catmull_rom
* plane_upscaler=spline36
* plane_downscaler=spline36

##### ***`upscaler_kernel` / `downscaler_kernel` / `plane_upscaler_kernel` / `plane_downscaler_kernel`***

##### ***`upscaler_window` / `downscaler_window` / `plane_upscaler_kernel` / `plane_downscaler_window`***
Override the scaler kernel and window function, rspectively.<br>
Default: not specified.

##### ***`upscaler_radius` / `downscaler_radius` / `plane_upscaler_radius` / `plane_downscaler_radius`***
Override the filter kernel radius. Has no effect if the filter kernel is not resizeable.<br>
Must be between `0.0..16.0`.<br>
Default: `0.0` (no override).

##### ***`upscaler_clamp` / `downscaler_clamp` / `plane_upscaler_clamp` / `plane_downscaler_clamp`***
Represents an extra weighting/clamping coefficient for negative weights.<br>
A value of `0.0` represents no clamping.<br>
A value of `1.0` represents full clamping, i.e. all negative lobes will be removed.<br>
Defaults to `0.0`.

##### ***`upscaler_taper` / `downscaler_taper` / `plane_upscaler_taper` / `plane_downscaler_taper`***
Additional taper coefficient.<br>
This essentially flattens the function's center. The values within `[-taper, taper]` will return `1.0`,
with the actual function being squished into the remainder of `[taper, radius]`.<br>
Must be between `0.0..1.0`.<br>
Default: `0.0`.

##### ***`upscaler_blur` / `downscaler_blur` / `plane_upscaler_blur` / `plane_downscaler_blur`***
Additional blur coefficient.<br>
This effectively stretches the kernel, without changing the effective radius of the filter radius.
Setting this to a value of `0.0` is equivalent to disabling it.<br>
Values significantly below `1.0` may seriously degrade the visual output, and should be used with care.<br>
Must be between `0.0..100.0`.<br>
Default: `0.0`.

##### ***`upscaler_antiring` / `downscaler_antiring` / `plane_upscaler_antiring` / `plane_downscaler_antiring`***
Antiringing override for filter.<br>
Must be between `0.0..1.0`.<br>
Default: `0.0`, which infers the value from `antiringing_strength`.

##### ***`upscaler_param1` / `downscaler_param1` / `plane_upscaler_param1` / `plane_downscaler_param1`***

##### ***`upscaler_param2` / `downscaler_param2` / `plane_upscaler_param2` / `plane_downscaler_param2`***

##### ***`upscaler_wparam1` / `downscaler_wparam1` / `plane_upscaler_wparam1` / `plane_downscaler_wparam1`***

##### ***`upscaler_wparam2` / `downscaler_wparam2` / `plane_upscaler_wparam2` / `plane_downscaler_wparam2`***
Parameters for the respective filter function.<br>
Ignored if not tunable.<br>
Default: `0.0`.

##### ***`upscaler_polar` / `downscaler_polar` / `plane_upscaler_polar` / `plane_downscaler_polar`***
If true, this filter is a polar/2D filter (EWA), instead of a separable/1D (orthogonal) filter.<br>
Default: `false`.

##### ***`antiringing_strength`***
Antiringing strength to use for all filters.<br>
A value of `0.0` disables antiringing, and a value of `1.0` enables full-strength antiringing.<br>
Specific filter presets may override this option.<br>
Default: `0.0`.

##### ***`linear_scaling`***
If enabled, scaling is performed in linear light. This reduces "ringing" artifacts and
improves color accuracy during resizing.<br>
Default: `true`.

[Back to top](#description)

##### ***Image Enhancement***

##### ***`sigmoid`***
Enables sigmoidization when scaling in linear light. This further reduces the prominence of
ringing artifacts around sharp edges.<br>
Default: if `preset` is not specified, `false`.

##### ***`sigmoid_center` / `sigmoid_slope`***
The center (bias) and slope (steepness) of the sigmoid curve.<br>
`sigmoid_center` must be between `0.0..1.0`. Default: `0.75`.<br>
`sigmoid_slope` must be between `1.0..20.0`. Default: `6.5`.<br>
If any of these is specified, `sigmoid` is always `true`.

##### ***`deband`***
Enables the debanding filter.<br>
Default: if `preset` is not specified, `false`.

##### ***`deband_iterations`***
The number of debanding steps to perform. High numbers (>4) are usually unnecessary.<br>
If this is set to `0`, the debanding filter acts as pure grain generator.<br>
Must be between `0..16`.<br>
Default: `1`.<br>
If specified, `deband` is always `true`.

##### ***`deband_threshold`***
The filter's cut-off threshold. Higher numbers increase debanding strength but may lose detail.<br>
Must be between `0.0..1000.0`.<br>
Default: `3.0`.<br>
If specified, `deband` is always `true`.

##### ***`deband_radius`***
The initial radius for the debanding filter.<br>
Must be between `0.0..1000.0`.<br>
Default: `16.0`.<br>
If specified, `deband` is always `true`.

##### ***`deband_grain`***
Adds extra noise to help cover up remaining banding.<br>
Must be between `0.0..1000.0`.<br>
Default: `4.0`.<br>
If specified, `deband` is always `true`.

[Back to top](#description)

##### ***Color Space Definitions***

##### ***`src_csp` / `dst_csp`***
Presets for the source and destination color spaces. Setting this automatically configures matrix,
transfer, primaries, and levels.<br>
* `"sdr"`, `"709"`, `"bt709"`, `"rec709"`
* `"601_525"`, "ntsc"`
* `"601_625"`, `"pal"`
* `"hdr10"`, `"pq"`, `"2020"`, `"bt2020"`
* `"hlg"`, `"arib-std-b67"`
* `"dovi"`, `"dolbyvision"`
* `"srgb"`
* `"jpeg"`
* `"adobe"`, `"adobergb"`
* `"p3"`, `"dci-p3"`
* `"p3-d65"`, `"display-p3"`
Default: not specified.

##### ***`src_matrix` / `dst_matrix`***
Color system coefficients.<br>
* `"rgb"`
* `"709"`, `"bt709"`
* `"601"`, `"bt601"`, `"smpte170m"`
* `"240m"`, `"smpte240m"`
* `"2020nc"`, `"2020"`, `""bt2020"`, `"bt2020nc"`
* `"2020c"`, `"bt2020c"`,
* `"2100pq"`, `"bt2100pq"`, `"ictcp-pq"`
* `"2100hlg"`, `"bt2100hlg"`, `"ictcp-hlg"`
* `"dovi"`, `"dolbyvision"`
* `"ycgco"`
* `"ycgco-re"`
* `"ycgco-ro"`
* `"xyz"`.
Default: not specified.

##### ***`src_trc` / `dst_trc`***
Transfer characteristics (Gamma/EETF).<br>
* `"1886"`, `"bt1886"`, `"709"`, `"bt709"`
* `"srgb"`
* `"linear"`
* `"gamma1.8"`
*` "gamma2.0"`
*` "gamma2.2"`
* `"gamma2.4"`
* `"gamma2.6"`
* `"gamma2.8"`
* `"prophoto"`
* `"st428"`
* `"pq"`, `"st2084"`
* `"hlg"`, `"arib-std-b67"`
* `"vlog"`, `"v-log"`
* `"slog1"`, `"s-log1"`
* `"slog2"`, `"s-log2"`
Default: not specified.

##### ***`src_prim` / `dst_prim`***
Color primaries.<br>
* `"709"`, `"bt709"`, `"srgb"`
* `"ntsc"`, `"601-525"`, `"bt601-525"`, `"smpte-c"`
* `"pal"`, `"secam"`, `"601-625"`, `"bt601-625"`
* `"470m"`, `"bt470m"`
* `"ebu3213"`
* `"2020"`, `"bt2020"`
* `"apple"`
* `"adobe"`
* `"prophoto"`
* `"dci-p3"`, `"p3"`
* `"display-p3"`, `"p3-d65"`
* `"v-gamut"`, `"vgamut"`
* `"s-gamut"`, `"sgamut"`
* `"film"`, `"film-c"`
* `"aces"`, `"ap0"`, `"aces-ap0"`
* `"ap1"`, `"aces-ap1"`
Default: not specified.

##### ***`src_levels` / `dst_levels`***
Color range.<br>
* `"limited"`, `"tv"`
* `"full"`, `"pc"`
Default: YUV < 32-bit - `"tv"`, otherwise "pc".

##### ***`src_alpha` / `dst_alpha`***
Alpha channel interpretation.<br>
* `"none"`
* `"independent"`
* `"premultiplied"`
Default: not specified.

##### ***`src_cplace` / `dst_cplace`***
Chroma sample location.<br>
* `"left"`
* `"center"`
* `"top_left"`
* `"top_center"`
* `"bottom_left"`
* `"bottom_center"`
Default: `"left"`.

[Back to top](#description)

##### ***Tone & Gamut Mapping***

##### ***`color_map_preset`***
Preset for the color mapping (tone/gamut) engine.<br>
* `"default"`
    * gamut_mapping=perceptual
    * tone_mapping_function=spline
    * gamut_constants - colorimetric_gamma=1.80f, softclip_knee=0.70f, softclip_desat=0.35f,
        perceptual_deadzone=0.30f, perceptual_strength=0.80f
    * tone_constants - knee_adaptation=0.4f, knee_minimum=0.1f, knee_maximum=0.8f,
        knee_default=0.4f, knee_offset=1.0f, slope_tuning=1.5f, slope_offset=0.2f,
        spline_contrast=0.5f, reinhard_contrast=0.5f, linear_knee=0.3f, exposure=1.0f
    * metadata=ANY
    * lut3d_size={i48, c32, h256}
    * lut_size=256
    * visualize_rect={0, 0, 1, 1}
    * contrast_smoothness=3.5f
* `"high_quality"` - `"default"` + `contrast_recovery=0.30f`.
Default: if `preset` is not specified, `"high_quality`.

##### ***`gamut_mapping`***
Algorithm used to handle out-of-gamut colors.<br>
* `"clip"`: Performs no gamut-mapping, just hard clips out-of-range colors
    per-channel.
* `"perceptual"`: Performs a perceptually balanced (saturation) gamut mapping,
    using a soft knee function to preserve in-gamut colors, followed by a final
    softclip operation. This works bidirectionally, meaning it can both compress
    and expand the gamut. Behaves similar to a blend of `saturation` and
    `softclip`.
* `"softclip"`: Performs a perceptually balanced gamut mapping using a soft knee
    function to roll-off clipped regions, and a hue shifting function to preserve
    saturation.
* `"relative"`: Performs relative colorimetric clipping, while maintaining an
    exponential relationship between brightness and chromaticity.
* `"saturation"`: Performs simple RGB->RGB saturation mapping. The input R/G/B
    channels are mapped directly onto the output R/G/B channels. Will never clip,
    but will distort all hues and/or result in a faded look.
* `"absolute"`: Performs absolute colorimetric clipping. Like `relative`, but
    does not adapt the white point.
* `"desaturate"`: Performs constant-luminance colorimetric clipping, desaturing
    colors towards white until they're in-range.
* `"darken"`: Uniformly darkens the input slightly to prevent clipping on
    blown-out highlights, then clamps colorimetrically to the input gamut
    boundary, biased slightly to preserve chromaticity over luminance.
* `"highlight"`: Performs no gamut mapping, but simply highlights out-of-gamut
    pixels.
* `"linear"`: Linearly/uniformly desaturates the image in order to bring the
    entire image into the target gamut.
Default: `"perceptual"`.

##### ***`perceptual_deadzone` / `perceptual_strength`***
(Relative) chromaticity protection zone / Strength for the `perceptual` gamut mapping algorithm.<br>
Must be between `0.0..1.0`.<br>
Defaults: `deadzone=0.30`, `strength=0.80`.

##### ***`colorimetric_gamma`***
Gamma used for colorimetric clipping (`relative`, `absolute` and `darken`). <br>
Must be between `0.0..10.0`.<br>
Default: `1.80`.

##### ***`softclip_knee`***
Knee point to use for soft-clipping methods (`perceptual`, `softclip`).<br>
Must be between `0.0..1.0`.<br>
Default: `0.70`.

#####  `softclip_desat`<br>
Desaturation strength for `softclip`.<br>
Must be between `0.0..1.0`.<br>
Default: `0.35`.

##### ***`lut3d_size_i` / `lut3d_size_c` / `lut3d_size_h`***
Dimensions of the 3D LUT used for gamut mapping (Intensity, Chromaticity, Hue).<br>
Must be between `0..1024`.<br>
Defaults: `48`, `32`, `256`.

##### ***`lut3d_tricubic`***
Use tricubic interpolation for the 3D LUT.<br>
Default: `false`.

##### ***`gamut_expansion`***
Allow the gamut mapping function to expand the gamut if the target is wider than the source.<br>
Default: `false`.

##### ***`tone_mapping_function`***
Algorithm for adapting between different luminance ranges.<br>
* `"clip"`: Performs no tone-mapping, just clips out-of-range colors. Retains
    perfect color accuracy for in-range colors but completely destroys
    out-of-range information. Does not perform any black point adaptation.
* `"spline"`: Simple spline consisting of two polynomials, joined by a single
    pivot point, which is tuned based on the source scene average brightness
    (taking into account dynamic metadata if available). This function can be
    used for both forward and inverse tone mapping.
* `"st2094-40"`: EETF from SMPTE ST 2094-40 Annex B, which uses the provided OOTF
    based on Bezier curves to perform tone-mapping. The OOTF used is adjusted
    based on the ratio between the targeted and actual display peak luminances.
    In the absence of HDR10+ metadata, falls back to a simple constant bezier
    curve.
* `"st2094-10"`: EETF from SMPTE ST 2094-10 Annex B.2, which takes into account
    the input signal average luminance in addition to the maximum/minimum.
    !!! warning
    This does *not* currently include the subjective gain/offset/gamma controls
    defined in Annex B.3. (Open an issue with a valid sample file if you want
    such parameters to be respected.)
* `"bt2390"`: EETF from the ITU-R Report BT.2390, a hermite spline roll-off with
    linear segment.
* `"bt2446a"`":EETF from ITU-R Report BT.2446, method A. Can be used for both
    forward and inverse tone mapping.
* `"reinhard"`: Very simple non-linear curve. Named after Erik Reinhard.
* `"mobius"`: Generalization of the `reinhard` tone mapping algorithm to support
    an additional linear slope near black. The name is derived from its function
    shape `(ax+b)/(cx+d)`, which is known as a Möbius transformation. This
    function is considered legacy/low-quality, and should not be used.
* `"hable"`: Piece-wise, filmic tone-mapping algorithm developed by John Hable
    for use in Uncharted 2, inspired by a similar tone-mapping algorithm used by
    Kodak. Popularized by its use in video games with HDR rendering. Preserves
    both dark and bright details very well, but comes with the drawback of
    changing the average brightness quite significantly. This is sort of similar
    to `reinhard` with `reinhard_contrast=0.24`. This function is considered
    legacy/low-quality, and should not be used.
* `"gamma"`: Fits a gamma (power) function to transfer between the source and
    target color spaces, effectively resulting in a perceptual hard-knee joining
    two roughly linear sections. This preserves details at all scales, but can
    result in an image with a muted or dull appearance. This function
    is considered legacy/low-quality and should not be used.
* `"linear"`: Linearly stretches the input range to the output range, in PQ
    space. This will preserve all details accurately, but results in a
    significantly different average brightness. Can be used for inverse
    tone-mapping in addition to regular tone-mapping.
* `"linearlight"`: Like `linear`, but in linear light (instead of PQ). Works well
    for small range adjustments but may cause severe darkening when
    downconverting from e.g. 10k nits to SDR.
Default: `"spline"`.

##### ***`tone_constants`***
Fine-tuning parameters for tone mapping functions in `key=value` format.<br>
* `"knee_adaptation"`: Configures the knee point, as a ratio between the source average
    and target average (in PQ space).<br>
    An adaptation of `1.0` always adapts the source scene average brightness to
    the (scaled) target average, while a value of `0.0` never modifies scene brightness.<br>
    Must be between `0.0..1.0`.<br>
    Affects all methods that use the ST2094 knee point determination
    (currently `ST2094-40`, `ST2094-10` and `spline`).<br>
    Default: `0.4`.
* `"knee_minimum"`, `knee_maximum`: Configures the knee point minimum and maximum, respectively,
    as a percentage of the PQ luminance range.<br>
    Provides a hard limit on the knee point chosen by `knee_adaptation`.<br>
    `knee_minimum` must be between `0.0..0.5`.<br>
    `knee_maximum` must be between `0.5..1.0`.<br>
    Default: `knee_minimum` `0.1`; `knee_maximum` `0.8`.
* `"knee_default"`: Default knee point to use in the absence of source scene average metadata.<br>
    Normally, this is ignored in favor of picking the knee point as the (relative) source scene
    average brightness level.<br>
    Must be between `knee_minimum` and `knee_maximum`.<br>
    Default: `0.4`.
* `"knee_offset"`: Knee point offset (for `BT.2390` only).<br>
    Note that a value of `0.5` is the spec-defined default behavior, which differs from
    the libplacebo default of `1.0`.<br>
    Must be between `0.5..2.0`.
* `"slope_tuning"`, `slope_offset`: For the single-pivot polynomial (spline) function,
    this controls the coefficients used to tune the slope of the curve.<br>
    This tuning is designed to make the slope closer to `1.0` when the difference in peaks is low,
    and closer to linear when the difference between peaks is high.<br>
    `slope_tuning` must be between `0.0..10.0`.<br>
    `slope_offset` must be between `0.0..1.0`.<br>
    Default: `slope_tuning` 1.5; `slope_offset` 0.2.
* `"spline_contrast"`: Contrast setting for the spline function.<br>
    Higher values make the curve steeper (closer to `clip`), preserving midtones at the cost of
    losing shadow/highlight details, while lower values make the curve shallowed (closer to `linear`),
    preserving highlights at the cost of losing midtone contrast.<br>
    Values above `1.0` are possible, resulting in an output with more contrast than the input.<br>
    Must be between `0.0..1.5`.<br>
    Default: `0.5`.
* `"reinhard_contrast"`: For the reinhard function, this specifies
    the local contrast coefficient at the display peak.<br>
    Essentially, a value of `0.5` implies that the reference white will be
    about half as bright as when clipping. (0,1).
    Must be between `0.0..1.0`.<br>
    Default: `0.5`.
* `"linear_knee"`: For legacy functions (`mobius`, `gamma`) which operate on linear light,
    this directly sets the corresponding knee point.<br>
    Must be between `0.0..1.0`<br>
    Default: `0.3`.
* `"exposure"`: For linear methods (`linear`, `linearlight`), this controls
    the linear exposure/gain applied to the image.<br>
    Must be between `0.0..10.0`.<br>
    Default: `1.0`.

##### ***`inverse_tone_mapping`***
Enables inverse tone mapping to expand SDR content to HDR.<br>
Default: `false`.

##### ***`tone_lut_size`***
Size of the 1D LUT used for tone mapping.<br>
Must be between `0..4096`.<br>
Default: `256`.

##### ***`contrast_recovery`***
Strength of HDR contrast recovery (adds high-frequency detail back after tone mapping).<br>
Must be between `0.0..2.0`.<br>
Default: if `preset` or `color_map_preset` is not specified, `0.0`.

##### ***`contrast_smoothness`***
Lowpass kernel size for contrast recovery.<br>
Must be between `1.0..32.0`.<br>
Default: `3.5`.

[Back to top](#description)

##### ***HDR & Metadata***

##### ***`peak_detect`***
Enables dynamic HDR peak detection.<br>
Default: if `preset` is not specified, `false`.

##### ***`peak_detection_preset`***
Preset for peak detection.<br>
* `"default"`:
    * smoothing_period=20.0f
    * scene_threshold_low=1.0f
    * scene_threshold_high=3.0f
    * percentile= 100.0f
    * black_cutoff=1.0f
* `"high_quality"` - `"default"` + `percentile=99.995f`.
Default: if `preset` is not specified, `"high_quality"`.<br>
If this is specified, `peak_detect` is always `true`.

##### ***`peak_smoothing_period`***
Time constant for the peak detection low-pass filter (in frames).<br>
This helps block out annoying "sparkling" or "flickering" due to small variations
in frame-to-frame brightness.<br>
If left as `0.0`, this smoothing is completely disabled.<br>
Must be between `0.0..1000.0`.<br>
Default: `20.0`.<br>
If this is specified, `peak_detect` is always `true`.

##### ***`scene_threshold_low` / `scene_threshold_high`***
Thresholds to detect scene changes and bypass smoothing.<br>
Setting either one of these to `0.0` disables this logic.<br>
Must be between `0.0..100.0`.<br>
Defaults: `1.0`, `3.0`.<br>
If any of these is specified, `peak_detect` is always `true`.

##### ***`peak_percentile`***
Percentile of the histogram to consider as the peak.<br>
Must be between `0.0..100.0`.<br>
Default: if `preset` or `peak_detection_preset` is not specified, `100.0`.<br>
If this is specified, `peak_detect` is always `true`.

##### ***`black_cutoff`***
Cutoff threshold to prevent shadow shimmer.<br>
Must be between `0.0..100.0`.<br>
Default: `1.0`.<br>
If this is specified, `peak_detect` is always `true`.

##### ***`src_max` / `src_min` / `dst_max` / `dst_min`***
Manual overrides for source and destination luminance limits (in nits).
Default: not specified.

##### ***`tone_map_metadata`***
Metadata source preference.<br>
* `"any"`
* `"none"`
* `"hdr10"`
* `"hdr10plus"`
* `"cie_y"`
Default: `"any"`.

##### ***`dovi_metadata`***
Enable parsing and application of Dolby Vision RPU metadata.<br>
Default: `false`.

[Back to top](#description)

##### ***Color Adjustments***

##### ***`brightness`***
Brightness boost. Adds a constant bias onto the source luminance signal.<br>
* `0.0` = neutral
* `1.0` = solid white
* `-1.0` = solid black
Default: `0.0`.

##### ***`contrast`***
Contrast gain. Multiplies the source luminance signal by a constant factor.<br>
* `1.0` = neutral
* `0.0` = solid black
Must be between `0.0..100.0`.<br>
Default: `1.0`.

##### ***`saturation`***
Saturation gain. Multiplies the source chromaticity signal by a constant factor.<br>
* `1.0` = neutral
* `0.0` = grayscale
Must be between `0.0..100.0`.<br>
Default: `1.0`.

##### ***`hue`***
Hue shift. Corresponds to a rotation of the UV subvector around the neutral axis.<br>
Specified in radians.<br>
Default: `0.0` (neutral).

##### ***`gamma`***
Gamma lift. Subjectively brightnes or darkens the scene while preserving overall contrast.<br>
* `1.0` = neutral
* `0.0` = solid black
Must be between `0.0..100.0`.<br>
Default: `1.0`.

##### ***`temperature`***
Color temperature shift.<br>
Relative to 6500 K, a value of `0.0` gives you 6500 K (no change), a value of `-1.0` gives you 3000 K,
and a value of `1.0` gives you 10000 K.<br>
Must be between `-1.143..5.286`.<br>
Default: `0.0`.

[Back to top](#description)

##### ***Deinterlacing***

##### ***`deinterlace_algo`***
Algorithm for deinterlacing.<br>
* `"weave"`: No-op deinterlacing, just sample the weaved frame un-touched.
* `"bob"`: Naive bob deinterlacing. Doubles the field lines vertically.
* `"yadif"`: "Yet another deinterlacing filter". Deinterlacer with temporal and
    spatial information. Based on FFmpeg's Yadif filter algorithm, but adapted
    slightly for the GPU.
* `"bwdif"`: "Bob weaver deinterlacing filter". Motion-adaptive deinterlacer
    based on yadif, with the use of w3fdif and cubic interpolation algorithms.
Default: `"yadif"`.<br>
If this is specified, deinterlacing is enabled.

##### ***`field`***
Field order and deinterlacing mode.<br>
* `-2`: Double rate, auto parity.
* `-1`: Single rate, auto parity.
* `0`: Single rate, Bottom Field First.
* `1`: Single rate, Top Field First.
* `2`: Double rate, Bottom Field First.
* `3`: Double rate, Top Field First.
Default: `-1`.<br>
If this is specified, deinterlacing is enabled.

##### ***`spatial_check`***
Enable spatial interlacing check for `yadif`.<br>
Default: `true`.<br>
If this is specified, deinterlacing is enabled.

[Back to top](#description)

##### ***Dithering***

##### ***`dither`***
Enables dithering to the output bit depth.<br>
Default: if `preset` is not specified, `false`.

##### ***`dither_method`***
* `"blue"`: Dither with blue noise. Very high quality, but requires the use of a
    LUT.
    !!! warning
    Computing a blue noise texture with a large size can be very slow, however
    this only needs to be performed once. Even so, using this with a
    `dither_lut_size` greater than `6` is generally ill-advised.
* `"ordered_lut"`: Dither with an ordered (bayer) dither matrix, using a LUT. Low
    quality, and since this also uses a LUT, there's generally no advantage to
    picking this instead of `blue`. It's mainly there for testing.
* `"ordered"`: The same as `ordered`, but uses fixed function math instead of a
    LUT. This is faster, but only supports a fixed dither matrix size of 16x16
    (equivalent to `dither_lut_size=4`).
* `"white"`: Dither with white noise. This does not require a LUT and is fairly
    cheap to compute. Unlike the other modes it doesn't show any repeating
    patterns either spatially or temporally, but the downside is that this is
    visually fairly jarring due to the presence of low frequencies in the noise
    spectrum.
* `"error_diffusion"`: Only formats with one plane are supported. This is a very slow and memory
    intensive method of dithering without the use of a fixed dither pattern. It's
    highly recommended to use this only for still images, not moving video.
Default: `"blue"`.<br>
If this is specified, `dither` is always `true`.

##### ***`dither_lut_size`***
For the dither methods which require the use of a LUT (`blue`, `ordered_lut`),
this controls the size of the LUT (base 2).<br>
Default: `6`.<br>
If this is specified, `dither` is always `true`.

##### ***`dither_temporal`***
Changes the dither pattern every frame.<br>
!!! warning
This can cause nasty aliasing artifacts on some LCD screens.<br>
Default: `false`.<br>
If this is specified, `dither` is always `true`.

##### ***`error_diffusion_k`***
Kernel to use if `dither_method` is set to error diffusion.<br>
* `"simple"`: Simple error diffusion (fast)
* `"false-fs"`: False Floyd-Steinberg kernel (fast)
* `"sierra-lite"`: Sierra Lite kernel (slow)
* `"floyd-steinberg"` / `"fs"`: Floyd-Steinberg kernel (slow)
* `"atkinson"`: Atkinson kernel (slow)
* `"jarvis-judice-ninke"` / `"jjn"`: Jarvis, Judice & Ninke kernel (very slow)
* `"stucki"`: Stucki kernel (very slow)
* `"burkes"`: Burkes kernel (very slow)
* `"sierra-2"`: Two-row Sierra (very slow)
* `"sierra-3"`: Three-row Sierra (very slow)
Default: `"floyd-steinberg"`.

[Back to top](#description)

##### ***Advanced & System***

##### ***`custom_shader_path`***
Path to custom shader file.<br>
Default: not specified.

##### ***`custom_shader_param`***
This changes shader's parameter set by `#define XXXX YYYY` on the fly.<br>
Format is: `param=value`.<br>
The parameter is case sensitive and must be the same as in the shader file.<br>
If more than one parameter is specified, the parameters must be separated by space.
  Usage example: if the shader has the following parameters:
* #define INTENSITY_SIGMA 0.1 //Intensity window size, higher is stronger denoise, must be a positive real number
* #define SPATIAL_SIGMA 1.0 //Spatial window size, higher is stronger denoise, must be a positive real number
  `shader_param="INTENSITY_SIGMA=0.15 SPATIAL_SIGMA=1.1"`

Default: not specified.

##### ***`visualize_lut`***
Enables a graphical overlay to visualize tone mapping and gamut mapping LUTs.<br>
Default: `false`.

##### ***`visualize_lut_x0` / `visualize_lut_y0` / `visualize_lut_x1` / `visualize_lut_y1`***
Coordinates for the visualization overlay.<br>
Defaults: `0.0, 0.0, 1.0, 1.0`.

##### ***`visualize_hue` / `visualize_theta`***
Controls the rotation of the gamut 3DLUT visualization. The `hue` parameter
rotates the gamut through hue space (around the `I` axis), while the `theta`
parameter vertically rotates the cross section (around the `C` axis), in
radians.<br>
Default: `0, 0`.

##### ***`show_clipping`***
Highlight pixels that are clipped by the current tone mapping settings.<br>
Default: `false`.

##### ***`lut`***
Path to a `.cube` LUT file to apply.<br>
Default: not specified.

##### ***`lut_type`***
How the LUT should be applied.<br>
* `"unknown"`: Unknown LUT type, try and guess from metadata
* `"native"`: LUT is applied to raw image contents
* `"normalized"`: LUT is applied to normalized (HDR) RGB values
* `"conversion"`: LUT fully replaces color conversion step
Default: `"unknown"`.

##### ***`corner_rounding`***
If set to a value above `0.0`, the output will be rendered with rounded
corners, as if an alpha transparency mask had been applied. The value indicates
the relative fraction of the side length to round - a value of `1.0` rounds the
corners as much as possible.<br>
Default: `0.0`.

##### ***`device`***
The index of the Vulkan device to use.<br>
Default: `-1` (Auto).

##### ***`list_devices`***
If true, prints the list of available Vulkan devices onto the video frame.<br>
Default: `false`.

##### ***`cache_path`***
Path to save/load the compiled Vulkan shader cache to speed up subsequent initializations.<br>
Default: not specified.

[Back to top](#description)

### Building:

```
Requirements:
    - CMake
    - Ninja
    - Vulkan SDK (https://vulkan.lunarg.com/sdk)
    - Clang-cl (https://github.com/llvm/llvm-project/releases) (Windows)
```

```
Steps:
    Install Vulkan SDk.

    Clone the repo:
        git clone --recurse-submodules --depth 1 --shallow-submodules https://github.com/Asd-g/libplacebo_Render

    Set prefix:
        cd libplacebo_Render
        set prefix="%cd%\deps" (Windows)
        prefix="$(pwd)/deps" (Linux)

    Build dolby_vision:
        cd dovi_tool/dolby_vision
        cargo install cargo-c
        cargo cinstall --release --prefix %prefix% (Windows)
        cargo cinstall --release --prefix $prefix (Linux)

    Building libplacebo:
        cd ../../libplacebo
        set LIB=%LIB%;C:\VulkanSDK\1.3.268.0\Lib (Windows)
        meson setup build -Dvulkan-registry=C:\VulkanSDK\1.3.283.0\share\vulkan\registry\vk.xml --default-library=static --buildtype=release -Ddemos=false -Dopengl=disabled -Dd3d11=disabled --prefix=%prefix% (Windows)
        meson setup build --default-library=static --buildtype=release -Ddemos=false -Dopengl=disabled -Dd3d11=disabled --prefix=$prefix (Linux)
        ninja -C build
        ninja -C build install

    Building plugin:
        # Options:
        # - USE_SYSTEM_AVS_HELPER: Use an installed version of avs_c_api_loader, default OFF
        # - USE_STATIC_LIBPLACEBO: Link libplacebo statically, default ON
        # - USE_STATIC_DOVI: Link dovi statically, default ON
        # USE_STATIC_SHADERC does work only for MSVC. MINGW is always statically linked.
        # - USE_STATIC_SHADERC: Link shaderc statically (shaderc_combined) instead of shared, default ON
        cd ../
        cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=%prefix% (Windows)
        cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=$prefix (Linux)
        ninja -C build
```

[Back to top](#description)
