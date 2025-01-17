// SPDX-License-Identifier: MIT
/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * This file contains code adapted from Skia, which is
 * Copyright (c) 2011 Google Inc. All rights reserved.
 */


#ifndef __IGT_COLOR_H__
#define __IGT_COLOR_H__

#include <limits.h>

#include "igt_fb.h"
#include "igt_kms.h"
#include "igt_color_lut.h"

#define MAX_COLOR_LUT_ENTRIES 4096

struct igt_color_tf {
    float g, a,b,c,d,e,f;
};

struct igt_color_tf_pq {
	float A, B, C, D, E, F, G;
};


const struct igt_color_tf srgb_eotf = {2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0, 0};
const struct igt_color_tf bt2020_inv_oetf = {(float)(1/0.45f), (float)(1/1.0993f), (float)(0.0993f/1.0993f), (float)(1/4.5f), (float)(0.081), 0, 0};

const struct igt_color_tf_pq pq_eotf = {-107/128.0f, 1.0f, 32/2523.0f, 2413/128.0f, -2392/128.0f, 8192/1305.0f };

typedef struct igt_pixel {
	float r;
	float g;
	float b;
} igt_pixel_t;

typedef struct igt_1dlut {
	struct drm_color_lut lut[MAX_COLOR_LUT_ENTRIES];
} igt_1dlut_t;

igt_1dlut_t igt_1dlut_srgb_inv_eotf = { {
} };


igt_1dlut_t igt_1dlut_srgb_eotf = { {
} };

typedef struct igt_matrix_3x4 {
	/*
	 * out   matrix          in
	 * |R|   |0  1  2  3 |   | R |
	 * |G| = |4  5  6  7 | x | G |
	 * |B|   |8  9  10 12|   | B |
	 *                       |1.0|
	 */
	float m[12];
} igt_matrix_3x4_t;

typedef struct igt_matrix_3x3 {
	/*
	 * out   matrix       in
	 * |R|   |0  1  2 |   | R |
	 * |G| = |3  4  5 | x | G |
	 * |B|   |6  7  8 |   | B |
	 */
	double m[9];
} igt_matrix_3x3_t;

const igt_matrix_3x4_t igt_matrix_3x4_50_desat = { {
	0.5, 0.25, 0.25, 0.0,
	0.25, 0.5, 0.25, 0.0,
	0.25, 0.25, 0.5, 0.0
} };

const igt_matrix_3x4_t igt_matrix_3x4_overdrive = { {
	1.5, 0.0, 0.0, 0.0,
	0.0, 1.5, 0.0, 0.0,
	0.0, 0.0, 1.5, 0.0
} };

const igt_matrix_3x4_t igt_matrix_3x4_oversaturate = { {
	1.5,   -0.25, -0.25, 0.0,
	-0.25,  1.5,  -0.25, 0.0,
	-0.25, -0.25,  1.5,  0.0
} };

#if 0
const igt_matrix_3x4_t igt_matrix_3x4_bt709_enc = { {
	 0.2126,   0.7152,  0.0722, 0.0,
	-0.1146, -0.3854,  0.5,    0.0,
	 0.5,     -0.4542, -0.0458, 0.0
} };

const igt_matrix_3x4_t igt_matrix_3x4_bt709_dec = { {
	1.0,  0.0,     1.5748, 0.0,
	1.0, -0.1873, -0.4681, 0.0,
	1.0,  1.8556,  0.0,    0.0
} };
#else
const igt_matrix_3x4_t igt_matrix_3x4_bt709_enc = { {
	 0.2126,   0.7152,   0.0722,  0.0,
	-0.09991, -0.33609,  0.436,   0.0,
	 0.615,   -0.55861, -0.05639, 0.0
} };

const igt_matrix_3x4_t igt_matrix_3x4_bt709_dec = { {
	1.0,  0.0,      1.28033, 0.0,
	1.0, -0.21482, -0.38059, 0.0,
	1.0,  2.12798,  0.0,     0.0
} };
#endif

typedef struct {
	uint32_t segment_count;
	struct drm_color_lut_range *segment_data;
	uint32_t entries_count;
} segment_data_t;

typedef enum kms_colorop_custom_lut1d_tf {
	KMS_COLOROP_CUSTOM_LUT1D_ZERO,
	KMS_COLOROP_CUSTOM_LUT1D_LINEAR,
	KMS_COLOROP_CUSTOM_LUT1D_MAX,
} kms_colorop_custom_lut1d_tf_t;

typedef struct kms_colorop_custom_lut1d_info {
	uint32_t lut_size;
	kms_colorop_custom_lut1d_tf_t lut_type;
	struct drm_color_lut_32 *lut;
} kms_colorop_custom_lut1d_info_t;

void create_zero_lut(segment_data_t *info, struct drm_color_lut_32 *lut);
void create_unity_lut(segment_data_t *info, struct drm_color_lut_32 *lut);
void create_max_lut(segment_data_t *info, struct drm_color_lut_32 *lut);
void clear_segment_data(segment_data_t *info);
segment_data_t *get_segment_data(int drm_fd, uint64_t blob_id);

bool igt_cmp_fb_component(uint16_t comp1, uint16_t comp2, uint8_t up, uint8_t down);
bool igt_cmp_fb_pixels(igt_fb_t *fb1, igt_fb_t *fb2, uint8_t up, uint8_t down);

void igt_dump_fb(igt_display_t *display, igt_fb_t *fb, const char *path_name, const char *file_name);

/* TODO also allow 64-bit pixels, or other weird sizes */
typedef void (*igt_pixel_transform)(igt_pixel_t *pixel);

int igt_color_transform_pixels(igt_fb_t *fb, igt_pixel_transform transforms[], int num_transforms);

/* colorop helpers */
void igt_colorop_set_ctm_3x3(igt_display_t *display,
			     igt_colorop_t *colorop,
			     const struct drm_color_ctm *matrix);
void igt_colorop_set_ctm_3x4(igt_display_t *display,
			     igt_colorop_t *colorop,
			     const igt_matrix_3x4_t *matrix);
void igt_colorop_set_custom_lut_1d_multseg(igt_display_t *display,
			     igt_colorop_t *colorop,
			     const kms_colorop_custom_lut1d_info_t custom_lut1d_info);

void igt_colorop_set_custom_1dlut(igt_display_t *display,
				  igt_colorop_t *colorop,
				  const igt_1dlut_t *lut1d,
				  const size_t lut_size);

void igt_colorop_set_3dlut(igt_display_t *display,
			   igt_colorop_t *colorop,
			   const igt_3dlut_norm_t *lut3d,
			   const size_t lut_size);


/* transformations */

void igt_color_srgb_inv_eotf(igt_pixel_t *pixel);
void igt_color_srgb_eotf(igt_pixel_t *pixel);

void igt_color_srgb_inv_eotf_custom_lut(igt_pixel_t *pixel);

void igt_color_pq_inv_eotf(igt_pixel_t *pixel);
void igt_color_pq_eotf(igt_pixel_t *pixel);

void igt_color_pq_125_inv_eotf(igt_pixel_t *pixel);
void igt_color_pq_125_eotf(igt_pixel_t *pixel);

void igt_color_bt2020_inv_oetf(igt_pixel_t *pixel);
void igt_color_bt2020_oetf(igt_pixel_t *pixel);

void igt_color_ctm_3x4_50_desat(igt_pixel_t *pixel);
void igt_color_ctm_3x4_overdrive(igt_pixel_t *pixel);
void igt_color_ctm_3x4_oversaturate(igt_pixel_t *pixel);
void igt_color_ctm_3x4_bt709_dec(igt_pixel_t *pixel);
void igt_color_ctm_3x4_bt709_enc(igt_pixel_t *pixel);

void igt_color_multiply_125(igt_pixel_t *pixel);
void igt_color_multiply_inv_125(igt_pixel_t *pixel);
void igt_color_3dlut_17_12_rgb(igt_pixel_t *pixel);
void igt_color_3dlut_17_12_bgr(igt_pixel_t *pixel);

#endif
