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

struct igt_color_tf {
    float g, a,b,c,d,e,f;
};

const struct igt_color_tf srgb_eotf = {2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0, 0};

typedef struct igt_pixel {
	float r;
	float g;
	float b;
} igt_pixel_t;

bool igt_cmp_fb_component(uint16_t comp1, uint16_t comp2, uint8_t up, uint8_t down);
bool igt_cmp_fb_pixels(igt_fb_t *fb1, igt_fb_t *fb2, uint8_t up, uint8_t down);

void igt_dump_fb(igt_display_t *display, igt_fb_t *fb, const char *path_name, const char *file_name);

/* TODO also allow 64-bit pixels, or other weird sizes */
typedef void (*igt_pixel_transform)(igt_pixel_t *pixel);

int igt_color_transform_pixels(igt_fb_t *fb, igt_pixel_transform transforms[], int num_transforms);

void igt_color_srgb_inv_eotf(igt_pixel_t *pixel);
void igt_color_srgb_eotf(igt_pixel_t *pixel);

#endif