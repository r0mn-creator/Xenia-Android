/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_GPU_FLAGS_H_
#define XENIA_GPU_GPU_FLAGS_H_
#include "xenia/base/cvar.h"

DECLARE_path(trace_gpu_prefix);
DECLARE_bool(trace_gpu_stream);

DECLARE_path(dump_shaders);

DECLARE_bool(vsync);

DECLARE_bool(gpu_allow_invalid_fetch_constants);

DECLARE_bool(non_seamless_cube_map);

DECLARE_bool(half_pixel_offset);

DECLARE_int32(query_occlusion_fake_sample_count);

// DIAGNOSTIC bisection toggles for the ~28s ART-heap smash. Each one early-outs
// of a stage of the Vulkan render path so we can isolate which stage corrupts
// the host heap (set via the xe_<cvar> intent extra; no rebuild per combo).
DECLARE_bool(diag_skip_draw);
DECLARE_bool(diag_skip_copy);
DECLARE_bool(diag_skip_memexport);

#define XE_GPU_FINE_GRAINED_DRAW_SCOPES 1

#endif  // XENIA_GPU_GPU_FLAGS_H_
