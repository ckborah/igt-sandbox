/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt.h"
#include "igt_color.h"
#include "sw_sync.h"

#include "kms_colorop.h"

/**
 * TEST: kms colorop
 * Category: Display
 * Description: Test to validate the retrieving and setting of DRM colorops
 *
 * SUBTEST: plane-%s
 * Description: Tests DRM colorop properties on a plane
 * Driver requirement: amdgpu
 * Functionality: kms_core
 * Mega feature: General Display Features
 * Test category: functionality test
 *
 * arg[1]:
 *
 * @srgb_eotf:                   sRGB EOTF
 * @srgb_inv_eotf:               sRGB Inverse EOTF
 * @srgb_eotf-srgb_inv_eotf:     sRGB EOTF -> sRGB Inverse EOTF
 *
 */

/* TODO move to lib for kms_writeback and kms_colorop (and other future) use */
static bool check_writeback_config(igt_display_t *display, igt_output_t *output,
				    drmModeModeInfo override_mode)
{
	igt_fb_t input_fb, output_fb;
	igt_plane_t *plane;
	uint32_t writeback_format = DRM_FORMAT_XRGB8888;
	uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
	int width, height, ret;

	igt_output_override_mode(output, &override_mode);

	width = override_mode.hdisplay;
	height = override_mode.vdisplay;

	ret = igt_create_fb(display->drm_fd, width, height,
			    DRM_FORMAT_XRGB8888, modifier, &input_fb);
	igt_assert(ret >= 0);

	ret = igt_create_fb(display->drm_fd, width, height,
			    writeback_format, modifier, &output_fb);
	igt_assert(ret >= 0);

	plane = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(plane, &input_fb);
	igt_output_set_writeback_fb(output, &output_fb);

	ret = igt_display_try_commit_atomic(display, DRM_MODE_ATOMIC_TEST_ONLY |
					    DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	igt_plane_set_fb(plane, NULL);
	igt_remove_fb(display->drm_fd, &input_fb);
	igt_remove_fb(display->drm_fd, &output_fb);

	return !ret;
}

/* TODO move to lib for kms_writeback and kms_colorop (and other future) use */
typedef struct {
	bool builtin_mode;
	bool custom_mode;
	bool list_modes;
	bool dump_check;
	int mode_index;
	drmModeModeInfo user_mode;
} data_t;

static data_t data;

/* TODO move to lib for kms_writeback and kms_colorop (and other future) use */
static igt_output_t *kms_writeback_get_output(igt_display_t *display)
{
	int i;
	enum pipe pipe;

	drmModeModeInfo override_mode = {
		.clock = 25175,
		.hdisplay = 640,
		.hsync_start = 656,
		.hsync_end = 752,
		.htotal = 800,
		.hskew = 0,
		.vdisplay = 480,
		.vsync_start = 490,
		.vsync_end = 492,
		.vtotal = 525,
		.vscan = 0,
		.vrefresh = 60,
		.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
		.name = {"640x480-60"},
	};

	for (i = 0; i < display->n_outputs; i++) {
		igt_output_t *output = &display->outputs[i];

		if (output->config.connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		for_each_pipe(display, pipe) {
			igt_output_set_pipe(output, pipe);

			if (data.custom_mode)
				override_mode = data.user_mode;
			if (data.builtin_mode)
				override_mode = output->config.connector->modes[data.mode_index];

			if (check_writeback_config(display, output, override_mode)) {
				igt_debug("Using connector %u:%s on pipe %d\n",
					  output->config.connector->connector_id,
					  output->name, pipe);
				return output;
			}
		}

		igt_debug("We found %u:%s, but this test will not be able to use it.\n",
			  output->config.connector->connector_id, output->name);

		/* Restore any connectors we don't use, so we don't trip on them later */
		kmstest_force_connector(display->drm_fd, output->config.connector, FORCE_CONNECTOR_UNSPECIFIED);
	}

	return NULL;
}

/* TODO move to lib for kms_writeback and kms_colorop (and other future) use */
static uint64_t get_writeback_fb_id(igt_output_t *output)
{
	return igt_output_get_prop(output, IGT_CONNECTOR_WRITEBACK_FB_ID);
}

/* TODO move to lib for kms_writeback and kms_colorop (and other future) use */
static void detach_crtc(igt_display_t *display, igt_output_t *output)
{
	if (get_writeback_fb_id(output) == 0)
		return;

	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit2(display, COMMIT_ATOMIC);
}

static void get_and_wait_out_fence(igt_output_t *output)
{
	int ret;

	igt_assert(output->writeback_out_fence_fd >= 0);

	ret = sync_fence_wait(output->writeback_out_fence_fd, 1000);
	igt_assert_f(ret == 0, "sync_fence_wait failed: %s\n", strerror(-ret));
	close(output->writeback_out_fence_fd);
	output->writeback_out_fence_fd = -1;
}

static bool can_use_colorop(igt_display_t *display, igt_colorop_t *colorop, kms_colorop_t *desired)
{
	switch (desired->type) {
	case KMS_COLOROP_ENUMERATED_LUT1D:
		if (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_1D_CURVE) {
			return true;
		}
	case KMS_COLOROP_CUSTOM_LUT1D:
	case KMS_COLOROP_CTM:
	case KMS_COLOROP_LUT3D:
	default:
		return false;
	}
}

/**
 * Iterate color pipeline that begins with colorop and try to map
 * colorops[] to it.
 */
static bool map_to_pipeline(igt_display_t *display,
			    igt_colorop_t *colorop,
			    kms_colorop_t *colorops[])
{
	igt_colorop_t *next = colorop;
	kms_colorop_t *current_op;
	int i = 0;
	int prop_val = 0;

	current_op = colorops[i++];
	igt_require(current_op);

	while (next) {
		if (can_use_colorop(display, next, current_op)) {
			current_op->colorop = next;
			current_op = colorops[i++];
			if (!current_op)
				break;
		}
		prop_val = igt_colorop_get_prop(display, next,
						IGT_COLOROP_NEXT);
		next = igt_find_colorop(display, prop_val);
	}

	if (current_op) {
		/* we failed to map the pipeline */

		/* clean up colorops[i].colorop mappings */
		for(i = 0, current_op = colorops[0]; current_op; current_op = colorops[i++])
			current_op->colorop = NULL;

		return false;
	}

	return true;
}

static igt_colorop_t *get_color_pipeline(igt_display_t *display,
					 igt_plane_t *plane,
					 kms_colorop_t *colorops[])
{
	igt_colorop_t *colorop = NULL;
	int i;

	/* go through all color pipelines */
	for (i = 0; i < plane->num_color_pipelines; ++i) {
		if (map_to_pipeline(display, plane->color_pipelines[i], colorops)) {
			colorop = plane->color_pipelines[i];
			break;
		}
	}

	return colorop;
}

static void set_colorop(igt_display_t *display,
			kms_colorop_t *colorop)
{
	igt_assert(colorop->colorop);
	igt_colorop_set_prop_value(colorop->colorop, IGT_COLOROP_BYPASS, 0);

	/* TODO set to desired value from kms_colorop_t */
	switch (colorop->type) {
	case KMS_COLOROP_ENUMERATED_LUT1D:
		switch (colorop->enumerated_lut1d_info.tf) {
		case KMS_COLOROP_LUT1D_SRGB_EOTF:
			igt_colorop_set_prop_enum(colorop->colorop, IGT_COLOROP_CURVE_1D_TYPE, "sRGB EOTF");
			break;
		case KMS_COLOROP_LUT1D_SRGB_INV_EOTF:
			igt_colorop_set_prop_enum(colorop->colorop, IGT_COLOROP_CURVE_1D_TYPE, "sRGB Inverse EOTF");
			break;
		case KMS_COLOROP_LUT1D_PQ_EOTF:
		case KMS_COLOROP_LUT1D_PQ_INV_EOTF:
		default:
			igt_fail(IGT_EXIT_FAILURE);
		}
		break;
	case KMS_COLOROP_CUSTOM_LUT1D:
	case KMS_COLOROP_CTM:
	case KMS_COLOROP_LUT3D:
	default:
		igt_fail(IGT_EXIT_FAILURE);
	}
}

static void set_color_pipeline(igt_display_t *display,
			       igt_plane_t *plane,
			       kms_colorop_t *colorops[],
			       igt_colorop_t *color_pipeline)
{
	igt_colorop_t *next;
	int prop_val = 0;
	int i;

	igt_plane_set_color_pipeline(plane, color_pipeline);

	for(i = 0; colorops[i]; i++)
		set_colorop(display, colorops[i]);

	/* set unused ops in pipeline to bypass */
	next = color_pipeline;
	i = 0;
	while (next) {
		if (!colorops[i] || colorops[i]->colorop != next)
			igt_colorop_set_prop_value(next, IGT_COLOROP_BYPASS, 1);
		else
			i++;

		prop_val = igt_colorop_get_prop(display, next,
						IGT_COLOROP_NEXT);
		next = igt_find_colorop(display, prop_val);
	}
}

static void set_color_pipeline_bypass(igt_plane_t *plane)
{
	igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_PIPELINE, "Bypass");
}

static bool compare_with_bracket(igt_fb_t *in, igt_fb_t *out)
{
	if (is_vkms_device(in->fd))
		return igt_cmp_fb_pixels(in, out, 1, 1);
	else
		/*
		 * By default we'll look for a [0, 0] bracket. We can then
		 * define it for each driver that implements support for this
		 * test. That way we can understand the precision of each
		 * driver better.
		 */
		return igt_cmp_fb_pixels(in, out, 0, 0);
}

#define DUMP_FBS 1

#define MAX_COLOROPS 3
#define NUM_COLOROP_TESTS 3
#define MAX_NAME_SIZE 256

static void apply_transforms(kms_colorop_t *colorops[], igt_fb_t *sw_transform_fb)
{
	int i;
	igt_pixel_transform transforms[MAX_COLOROPS];

	/*
	 * TODO
	 *
	 * This is wrong and loses precision since it always goes back
	 * to an 8-bpc framebuffer.
	 *
	 * Instead we need to stay as UNORM or float 16-bpc value throughout
	 * all transforms.
	 */

	for (i = 0; colorops[i]; i++)
		transforms[i] = colorops[i]->transform;

	igt_color_transform_pixels(sw_transform_fb, transforms, i);
}

static void colorop_plane_test(igt_display_t *display,
			       kms_colorop_t *colorops[])
{
	igt_colorop_t *color_pipeline = NULL;
	igt_output_t *output;
	igt_plane_t *plane;
	igt_fb_t input_fb;
	igt_fb_t sw_transform_fb;
	igt_fb_t output_fb;
	drmModeModeInfo mode;
	unsigned int fb_id;
	igt_crc_t input_crc, output_crc;

	output = kms_writeback_get_output(display);
	igt_require(output);

	if (output->use_override_mode)
		memcpy(&mode, &output->override_mode, sizeof(mode));
	else
		memcpy(&mode, &output->config.default_mode, sizeof(mode));

	/* create input fb */
	plane = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_assert(plane);

	fb_id = igt_create_color_pattern_fb(display->drm_fd,
					mode.hdisplay, mode.vdisplay,
					DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR,
					0.2, 0.2, 0.2, &input_fb);
	igt_assert(fb_id >= 0);
	igt_plane_set_fb(plane, &input_fb);
#if DUMP_FBS
	igt_dump_fb(display, &input_fb, ".", "input");
#endif

	/* create output fb */
	fb_id = igt_create_fb(display->drm_fd, mode.hdisplay, mode.vdisplay,
				DRM_FORMAT_XRGB8888,
				igt_fb_mod_to_tiling(0),
				&output_fb);
	igt_require(fb_id > 0);

	igt_fb_get_fnv1a_crc(&input_fb, &input_crc);

	igt_require(igt_plane_has_prop(plane, IGT_PLANE_COLOR_PIPELINE));

	/* reset color pipeline*/

	/* TODO do we need this? might be good sanity anyways */
	set_color_pipeline_bypass(plane);

	/* Commit */
	igt_plane_set_fb(plane, &input_fb);
	igt_output_set_writeback_fb(output, &output_fb);

	igt_display_commit_atomic(output->display,
				DRM_MODE_ATOMIC_ALLOW_MODESET,
				NULL);
	get_and_wait_out_fence(output);

	/* Compare input and output buffers. They should be equal here. */
	igt_fb_get_fnv1a_crc(&output_fb, &output_crc);

	igt_assert_crc_equal(&input_crc, &output_crc);

	/* create sw transformed buffer */

	igt_copy_fb(display->drm_fd, &input_fb, &sw_transform_fb);
	igt_assert(igt_cmp_fb_pixels(&input_fb, &sw_transform_fb, 0, 0));

	apply_transforms(colorops, &sw_transform_fb);
#if DUMP_FBS
	igt_dump_fb(display, &sw_transform_fb, ".", "sw_transform");
#endif
	/* discover and set COLOR PIPELINE */

	/* get COLOR_PIPELINE enum */
	color_pipeline = get_color_pipeline(display, plane, colorops);

	/* skip test if we can't find applicable pipeline */
	igt_skip_on(!color_pipeline);

	set_color_pipeline(display, plane, colorops, color_pipeline);

	igt_output_set_writeback_fb(output, &output_fb);

	/* commit COLOR_PIPELINE */
	igt_display_commit_atomic(display,
				DRM_MODE_ATOMIC_ALLOW_MODESET,
				NULL);
	get_and_wait_out_fence(output);
#if DUMP_FBS
	igt_dump_fb(display, &output_fb, ".", "output");
#endif

	/* compare sw transformed and KMS transformed FBs */
	igt_assert(compare_with_bracket(&sw_transform_fb, &output_fb));

	/* reset color pipeline*/
	set_color_pipeline_bypass(plane);

	/* Commit */
	igt_plane_set_fb(plane, &input_fb);
	igt_output_set_writeback_fb(output, &output_fb);

	igt_display_commit_atomic(output->display,
				DRM_MODE_ATOMIC_ALLOW_MODESET,
				NULL);
	get_and_wait_out_fence(output);

	/* cleanup */
	detach_crtc(display, output);
	igt_remove_fb(display->drm_fd, &input_fb);
	igt_remove_fb(display->drm_fd, &output_fb);
}

igt_main
{

	struct {
		kms_colorop_t *colorops[MAX_COLOROPS];
		const char *name;
	} tests[] = {
		{ { &kms_colorop_srgb_eotf, NULL }, "srgb_eotf" },
		{ { &kms_colorop_srgb_inv_eotf, NULL }, "srgb_inv_eotf" },
		{ { &kms_colorop_srgb_eotf, &kms_colorop_srgb_inv_eotf, NULL }, "srgb_eotf-srgb_inv_eotf" }
	};

	igt_display_t display;
	int i, ret;

	igt_fixture {
		display.drm_fd = drm_open_driver_master(DRIVER_ANY);

		if (drmSetClientCap(display.drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0)
			display.is_atomic = 1;

		ret = drmSetClientCap(display.drm_fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1);

		igt_require_f(!ret, "error setting DRM_CLIENT_CAP_WRITEBACK_CONNECTORS\n");

		igt_display_require(&display, display.drm_fd);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&display, display.drm_fd);

		igt_require(display.is_atomic);

	}

	for (i = 0; i < sizeof(tests) / sizeof (tests[0]); i++) {
		igt_describe("Bla bla bla");
		igt_subtest_f("plane-%s", tests[i].name)
			colorop_plane_test(&display, tests[i].colorops);
	}

	igt_describe("Tests getting and setting COLOR_PIPELINE property on plane");
#if 0
	igt_subtest("plane-srgb") {
		colorop_plane_test(&display, colorops_srgb);
	}
	igt_subtest("plane-inv_srgb") {
		colorop_plane_test(&display, colorops_srgb_inv);
	}
#endif
	igt_fixture {

		igt_display_fini(&display);
	}
}

