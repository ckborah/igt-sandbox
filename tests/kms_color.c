/*
 * Copyright © 2015 Intel Corporation
 *
  * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

/**
 * TEST: kms color
 * Category: Display
 * Description: Test Color Features at Pipe level
 * Driver requirement: i915, xe
 * Functionality: colorspace
 * Mega feature: Color Management
 * Test category: functionality test
 */

#include "kms_color_helper.h"
#include "kms_colorop.h"

#define MAX_COLOROPS	5

/**
 * SUBTEST: degamma
 * Description: Verify that degamma LUT transformation works correctly
 *
 * SUBTEST: gamma
 * Description: Verify that gamma LUT transformation works correctly
 *
 * SUBTEST: legacy-gamma
 * Description: Verify that legacy gamma LUT transformation works correctly
 *
 * SUBTEST: legacy-gamma-reset
 * Description: Verify that setting the legacy gamma LUT resets the gamma LUT
 *              set through GAMMA_LUT property
 *
 * SUBTEST: ctm-%s
 * Description: Check the color transformation %arg[1]
 *
 * arg[1]:
 *
 * @0-25:           for 0.25 transparency
 * @0-50:           for 0.50 transparency
 * @0-75:           for 0.75 transparency
 * @blue-to-red:    from blue to red
 * @green-to-red:   from green to red
 * @max:            for maximum transparency
 * @negative:       for negative transparency
 * @red-to-blue:    from red to blue
 * @signed:         for correct signed handling
 */

/**
 * SUBTEST: deep-color
 * Description: Verify that deep color works correctly
 *
 * SUBTEST: invalid-%s-sizes
 * Description: Negative check for %arg[1] sizes
 *
 * arg[1]:
 *
 * @ctm-matrix:         Color transformation matrix
 * @degamma-lut:        Degamma LUT
 * @gamma-lut:          Gamma LUT
 */

/**
 * SUBTEST: plane-%s
 * Description: Test plane color pipeline with colorops: %arg[1].
 *
 * arg[1]:
 *
 * @lut1d:		1D LUT
 * @ctm3x3:		3X3 CTM
 * @lut1d-ctm3x3:	1D LUT --> 3X3 CTM
 * @ctm3x3-lut1d:	3X3 CTM --> 1D LUT
 * @lut1d-lut1d:	1D LUT --> 1D LUT
 * @lut1d-ctm3x3-lut1d: 1D LUT --> 3X3 CTM --> 1D LUT
 */

IGT_TEST_DESCRIPTION("Test Color Features at Pipe level");

static bool test_pipe_degamma(data_t *data,
			      igt_plane_t *primary)
{
	igt_output_t *output = data->output;
	igt_display_t *display = &data->display;
	gamma_lut_t *degamma_linear, *degamma_full;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	drmModeModeInfo *mode = data->mode;
	struct igt_fb fb_modeset, fb;
	igt_crc_t crc_fullgamma, crc_fullcolors;
	int fb_id, fb_modeset_id;
	bool ret;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT));
	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	degamma_full = generate_table_max(data->degamma_lut_size);

	igt_output_set_pipe(output, primary->pipe->pipe);
	igt_output_override_mode(output, mode);

	/* Create a framebuffer at the size of the output. */
	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay,
			      mode->vdisplay,
			      data->drm_format,
			      DRM_FORMAT_MOD_LINEAR,
			      &fb);
	igt_assert(fb_id);

	fb_modeset_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      data->drm_format,
				      DRM_FORMAT_MOD_LINEAR,
				      &fb_modeset);
	igt_assert(fb_modeset_id);

	igt_plane_set_fb(primary, &fb_modeset);
	disable_ctm(primary->pipe);
	disable_gamma(primary->pipe);
	set_degamma(data, primary->pipe, degamma_linear);
	igt_display_commit(&data->display);

	/* Draw solid colors with linear degamma transformation. */
	paint_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

	/*
	 * Draw a gradient with degamma LUT to remap all
	 * values to max red/green/blue.
	 */
	paint_gradient_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);
	set_degamma(data, primary->pipe, degamma_full);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

	/*
	 * Verify that the CRC of the software computed output is
	 * equal to the CRC of the degamma LUT transformation output.
	 */
	ret = igt_skip_crc_compare || igt_check_crc_equal(&crc_fullgamma, &crc_fullcolors);

	disable_degamma(primary->pipe);
	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit(&data->display);
	igt_remove_fb(data->drm_fd, &fb);
	igt_remove_fb(data->drm_fd, &fb_modeset);

	free_lut(degamma_linear);
	free_lut(degamma_full);

	return ret;
}

/*
 * Draw 3 gradient rectangles in red, green and blue, with a maxed out gamma
 * LUT and verify we have the same CRC as drawing solid color rectangles.
 */
static bool test_pipe_gamma(data_t *data,
			    igt_plane_t *primary)
{
	igt_output_t *output = data->output;
	igt_display_t *display = &data->display;
	gamma_lut_t *gamma_full;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	drmModeModeInfo *mode = data->mode;
	struct igt_fb fb_modeset, fb;
	igt_crc_t crc_fullgamma, crc_fullcolors;
	int fb_id, fb_modeset_id;
	bool ret;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	gamma_full = generate_table_max(data->gamma_lut_size);

	igt_output_set_pipe(output, primary->pipe->pipe);
	igt_output_override_mode(output, mode);

	/* Create a framebuffer at the size of the output. */
	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay,
			      mode->vdisplay,
			      data->drm_format,
			      DRM_FORMAT_MOD_LINEAR,
			      &fb);
	igt_assert(fb_id);

	fb_modeset_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      data->drm_format,
				      DRM_FORMAT_MOD_LINEAR,
				      &fb_modeset);
	igt_assert(fb_modeset_id);

	igt_plane_set_fb(primary, &fb_modeset);
	disable_ctm(primary->pipe);
	disable_degamma(primary->pipe);
	set_gamma(data, primary->pipe, gamma_full);
	igt_display_commit(&data->display);

	/* Draw solid colors with no gamma transformation. */
	paint_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

	/*
	 * Draw a gradient with gamma LUT to remap all values
	 * to max red/green/blue.
	 */
	paint_gradient_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

	/*
	 * Verify that the CRC of the software computed output is
	 * equal to the CRC of the gamma LUT transformation output.
	 */
	ret = igt_skip_crc_compare || igt_check_crc_equal(&crc_fullgamma, &crc_fullcolors);

	disable_gamma(primary->pipe);
	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit(&data->display);
	igt_remove_fb(data->drm_fd, &fb);
	igt_remove_fb(data->drm_fd, &fb_modeset);

	free_lut(gamma_full);

	return ret;
}

/*
 * Draw 3 gradient rectangles in red, green and blue, with a maxed out legacy
 * gamma LUT and verify we have the same CRC as drawing solid color rectangles
 * with linear legacy gamma LUT.
 */
static bool test_pipe_legacy_gamma(data_t *data,
				   igt_plane_t *primary)
{
	igt_output_t *output = data->output;
	igt_display_t *display = &data->display;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	drmModeCrtc *kms_crtc;
	uint32_t i, legacy_lut_size;
	uint16_t *red_lut, *green_lut, *blue_lut;
	drmModeModeInfo *mode = data->mode;
	struct igt_fb fb_modeset, fb;
	igt_crc_t crc_fullgamma, crc_fullcolors;
	int fb_id, fb_modeset_id;
	bool ret;

	kms_crtc = drmModeGetCrtc(data->drm_fd, primary->pipe->crtc_id);
	legacy_lut_size = kms_crtc->gamma_size;
	drmModeFreeCrtc(kms_crtc);

	igt_require(legacy_lut_size > 0);

	red_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	green_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	blue_lut = malloc(sizeof(uint16_t) * legacy_lut_size);

	igt_output_set_pipe(output, primary->pipe->pipe);
	igt_output_override_mode(output, mode);

	/* Create a framebuffer at the size of the output. */
	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay,
			      mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      DRM_FORMAT_MOD_LINEAR,
			      &fb);
	igt_assert(fb_id);

	fb_modeset_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      DRM_FORMAT_MOD_LINEAR,
				      &fb_modeset);
	igt_assert(fb_modeset_id);

	igt_plane_set_fb(primary, &fb_modeset);
	disable_degamma(primary->pipe);
	disable_gamma(primary->pipe);
	disable_ctm(primary->pipe);
	igt_display_commit(&data->display);

	/* Draw solid colors with no gamma transformation. */
	paint_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

	/*
	 * Draw a gradient with gamma LUT to remap all values
	 * to max red/green/blue.
	 */
	paint_gradient_rectangles(data, mode, red_green_blue, &fb);
	igt_plane_set_fb(primary, &fb);

	red_lut[0] = green_lut[0] = blue_lut[0] = 0;
	for (i = 1; i < legacy_lut_size; i++)
		red_lut[i] = green_lut[i] = blue_lut[i] = 0xffff;
	igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd, primary->pipe->crtc_id,
					  legacy_lut_size, red_lut, green_lut, blue_lut), 0);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

	/*
	 * Verify that the CRC of the software computed output is
	 * equal to the CRC of the gamma LUT transformation output.
	 */
	ret = igt_skip_crc_compare || igt_check_crc_equal(&crc_fullgamma, &crc_fullcolors);

	/* Reset output. */
	for (i = 1; i < legacy_lut_size; i++)
		red_lut[i] = green_lut[i] = blue_lut[i] = i << 8;

	igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd, primary->pipe->crtc_id,
					  legacy_lut_size, red_lut, green_lut, blue_lut), 0);
	igt_display_commit(&data->display);

	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit(&data->display);
	igt_remove_fb(data->drm_fd, &fb);
	igt_remove_fb(data->drm_fd, &fb_modeset);

	free(red_lut);
	free(green_lut);
	free(blue_lut);

	return ret;
}

/*
 * Verify that setting the legacy gamma LUT resets the gamma LUT set
 * through the GAMMA_LUT property.
 */
static bool test_pipe_legacy_gamma_reset(data_t *data,
					 igt_plane_t *primary)
{
	static const double ctm_identity[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0,
	};
	drmModeCrtc *kms_crtc;
	gamma_lut_t *degamma_linear = NULL, *gamma_zero;
	uint32_t i, legacy_lut_size;
	uint16_t *red_lut, *green_lut, *blue_lut;
	struct drm_color_lut *lut;
	drmModePropertyBlobPtr blob;
	igt_output_t *output = data->output;
	bool ret = true;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
		degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	gamma_zero = generate_table_zero(data->gamma_lut_size);

	igt_output_set_pipe(output, primary->pipe->pipe);

	/* Ensure we have a clean state to start with. */
	disable_degamma(primary->pipe);
	disable_ctm(primary->pipe);
	disable_gamma(primary->pipe);
	igt_display_commit(&data->display);

	/*
	 * Set a degama & gamma LUT and a CTM using the
	 * properties and verify the content of the
	 * properties.
	 */
	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
		set_degamma(data, primary->pipe, degamma_linear);
	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM))
		set_ctm(primary->pipe, ctm_identity);
	set_gamma(data, primary->pipe, gamma_zero);
	igt_display_commit(&data->display);

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT)) {
		blob = get_blob(data, primary->pipe, IGT_CRTC_DEGAMMA_LUT);
		igt_assert(blob &&
			   blob->length == (sizeof(struct drm_color_lut) *
					    data->degamma_lut_size));
		drmModeFreePropertyBlob(blob);
	}

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM)) {
		blob = get_blob(data, primary->pipe, IGT_CRTC_CTM);
		igt_assert(blob &&
			   blob->length == sizeof(struct drm_color_ctm));
		drmModeFreePropertyBlob(blob);
	}

	blob = get_blob(data, primary->pipe, IGT_CRTC_GAMMA_LUT);
	igt_assert(blob &&
		   blob->length == (sizeof(struct drm_color_lut) *
				    data->gamma_lut_size));
	lut = (struct drm_color_lut *) blob->data;
	for (i = 0; i < data->gamma_lut_size; i++)
		ret &=(lut[i].red == 0 &&
			   lut[i].green == 0 &&
			   lut[i].blue == 0);
	drmModeFreePropertyBlob(blob);
	if(!ret)
		goto end;

	/*
	 * Set a gamma LUT using the legacy ioctl and verify
	 * the content of the GAMMA_LUT property is changed
	 * and that CTM and DEGAMMA_LUT are empty.
	 */
	kms_crtc = drmModeGetCrtc(data->drm_fd, primary->pipe->crtc_id);
	legacy_lut_size = kms_crtc->gamma_size;
	drmModeFreeCrtc(kms_crtc);

	red_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	igt_assert(red_lut);

	green_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	igt_assert(green_lut);

	blue_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	igt_assert(blue_lut);

	for (i = 0; i < legacy_lut_size; i++)
		red_lut[i] = green_lut[i] = blue_lut[i] = 0xffff;

	igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd,
					  primary->pipe->crtc_id,
					  legacy_lut_size,
					  red_lut, green_lut, blue_lut), 0);
	igt_display_commit(&data->display);

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
		igt_assert(get_blob(data, primary->pipe,
				    IGT_CRTC_DEGAMMA_LUT) == NULL);

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM))
		igt_assert(get_blob(data, primary->pipe, IGT_CRTC_CTM) == NULL);

	blob = get_blob(data, primary->pipe, IGT_CRTC_GAMMA_LUT);
	igt_assert(blob &&
		   blob->length == (sizeof(struct drm_color_lut) *
				    legacy_lut_size));
	lut = (struct drm_color_lut *) blob->data;
	for (i = 0; i < legacy_lut_size; i++)
		ret &= (lut[i].red == 0xffff &&
			   lut[i].green == 0xffff &&
			   lut[i].blue == 0xffff);
	drmModeFreePropertyBlob(blob);

	free(red_lut);
	free(green_lut);
	free(blue_lut);

end:
	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit(&data->display);

	free_lut(degamma_linear);
	free_lut(gamma_zero);
	return ret;
}

/*
 * Draw 3 rectangles using before colors with the ctm matrix apply and verify
 * the CRC is equal to using after colors with an identify ctm matrix.
 */
static bool test_pipe_ctm(data_t *data,
			  igt_plane_t *primary,
			  const color_t *before,
			  const color_t *after,
			  const double *ctm_matrix)
{
	static const double ctm_identity[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0,
	};
	gamma_lut_t *degamma_linear = NULL, *gamma_linear = NULL;
	igt_output_t *output = data->output;
	bool ret = true;
	igt_display_t *display = &data->display;
	drmModeModeInfo *mode = data->mode;
	struct igt_fb fb_modeset, fb;
	igt_crc_t crc_software, crc_hardware;
	int fb_id, fb_modeset_id;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM));

	igt_output_set_pipe(output, primary->pipe->pipe);
	igt_output_override_mode(output, mode);

	/* Create a framebuffer at the size of the output. */
	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay,
			      mode->vdisplay,
			      data->drm_format,
			      DRM_FORMAT_MOD_LINEAR,
			      &fb);
	igt_assert(fb_id);

	fb_modeset_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      data->drm_format,
				      DRM_FORMAT_MOD_LINEAR,
				      &fb_modeset);
	igt_assert(fb_modeset_id);
	igt_plane_set_fb(primary, &fb_modeset);

	disable_degamma(primary->pipe);
	disable_gamma(primary->pipe);

	/*
	 * Only program LUT's for intel, but not for max CTM as limitation of
	 * representing intermediate values between 0 and 1.0 causes
	 * rounding issues and inaccuracies leading to crc mismatch.
	 */
	if (is_intel_device(data->drm_fd) && memcmp(before, after, sizeof(color_t))) {
		igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

		gamma_linear = generate_table(256, 1.0);

		set_gamma(data, primary->pipe, gamma_linear);
	}

	igt_debug("color before[0] %f,%f,%f\n", before[0].r, before[0].g, before[0].b);
	igt_debug("color before[1] %f,%f,%f\n", before[1].r, before[1].g, before[1].b);
	igt_debug("color before[2] %f,%f,%f\n", before[2].r, before[2].g, before[2].b);

	igt_debug("color after[0] %f,%f,%f\n", after[0].r, after[0].g, after[0].b);
	igt_debug("color after[1] %f,%f,%f\n", after[1].r, after[1].g, after[1].b);
	igt_debug("color after[2] %f,%f,%f\n", after[2].r, after[2].g, after[2].b);

	disable_ctm(primary->pipe);
	igt_display_commit(&data->display);

	paint_rectangles(data, mode, after, &fb);
	igt_plane_set_fb(primary, &fb);
	set_ctm(primary->pipe, ctm_identity);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_software);

	/* With CTM transformation. */
	paint_rectangles(data, mode, before, &fb);
	igt_plane_set_fb(primary, &fb);
	set_ctm(primary->pipe, ctm_matrix);
	igt_display_commit(&data->display);
	igt_wait_for_vblank(data->drm_fd,
			    display->pipes[primary->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_hardware);

	/*
	 * Verify that the CRC of the software computed output is
	 * equal to the CRC of the CTM matrix transformation output.
	 */
	ret &= igt_skip_crc_compare || igt_check_crc_equal(&crc_software, &crc_hardware);

	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit(&data->display);
	igt_remove_fb(data->drm_fd, &fb);
	igt_remove_fb(data->drm_fd, &fb_modeset);

	free_lut(degamma_linear);
	free_lut(gamma_linear);

	return ret;
}

/*
 * Hardware computes CRC based on the number of bits it is working with (8,
 * 10, 12, 16 bits), meaning with a framebuffer of 8bits per color will
 * usually leave the remaining lower bits at 0.
 *
 * We're programming the gamma LUT in order to get rid of those lower bits so
 * we can compare the CRC of a framebuffer without any transformation to a CRC
 * with transformation applied and verify the CRCs match.
 *
 * This test is currently disabled as the CRC computed on Intel hardware seems
 * to include data on the lower bits, this is preventing us to CRC checks.
 */
#if 0
static void test_pipe_limited_range_ctm(data_t *data,
					igt_plane_t *primary)
{
	double limited_result = 235.0 / 255.0;
	static const color_t red_green_blue_limited[] = {
		{ limited_result, 0.0, 0.0 },
		{ 0.0, limited_result, 0.0 },
		{ 0.0, 0.0, limited_result },
	};
	static const color_t red_green_blue_full[] = {
		{ 0.5, 0.0, 0.0 },
		{ 0.0, 0.5, 0.0 },
		{ 0.0, 0.0, 0.5 },
	};
	static const double ctm[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0,
	};
	gamma_lut_t *degamma_linear, *gamma_linear;
	igt_output_t *output;
	bool has_broadcast_rgb_output = false;
	igt_display_t *display = &data->display;

	degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	gamma_linear = generate_table(data->gamma_lut_size, 1.0);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_full, crc_limited;
		int fb_id, fb_modeset_id;

		if (!igt_output_has_prop(output, IGT_CONNECTOR_BROADCAST_RGB))
			continue;

		has_broadcast_rgb_output = true;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      DRM_FORMAT_MOD_LINEAR,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      DRM_FORMAT_MOD_LINEAR,
					      &fb_modeset);
		igt_assert(fb_modeset_id);
		igt_plane_set_fb(primary, &fb_modeset);

		set_degamma(data, primary->pipe, degamma_linear);
		set_gamma(data, primary->pipe, gamma_linear);
		set_ctm(primary->pipe, ctm);

		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_FULL);
		paint_rectangles(data, mode, red_green_blue_limited, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd,
				display->pipes[primary->pipe->pipe].crtc_offset);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_full);

		/* Set the output into limited range. */
		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_16_235);
		paint_rectangles(data, mode, red_green_blue_full, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd,
				display->pipes[primary->pipe->pipe].crtc_offset);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_limited);

		/* And reset.. */
		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_FULL);
		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the CTM matrix transformation output.
		 */
		igt_assert_crc_equal(&crc_full, &crc_limited);

		igt_remove_fb(data->drm_fd, &fb);
		igt_remove_fb(data->drm_fd, &fb_modeset);
	}

	free_lut(gamma_linear);
	free_lut(degamma_linear);

	igt_require(has_broadcast_rgb_output);
}
#endif

static void clear_lut_data(kms_colorop_t *colorops[])
{
	int i;

	for (i = 0; colorops[i]; i++) {
		if (colorops[i]->type != KMS_COLOROP_CUSTOM_LUT1D)
			continue;

		if (colorops[i]->custom_lut1d_info.lut)
			free(colorops[i]->custom_lut1d_info.lut);
	}
}

static void prepare_lut_data(data_t *data, kms_colorop_t *colorops[])
{
	int i;

	for (i = 0; colorops[i]; i++) {
		uint64_t hwlut_caps = 0;
		segment_data_t *segment_info;

		if (colorops[i]->type != KMS_COLOROP_CUSTOM_LUT1D_MULTSEG)
			continue;

		hwlut_caps = igt_colorop_get_prop(&data->display, colorops[i]->colorop, IGT_COLOROP_HW_CAPS);
		segment_info = get_segment_data(data->drm_fd, hwlut_caps);

		igt_info("Lut size (%s): %d\n", colorops[i]->name, segment_info->entries_count);

		colorops[i]->custom_lut1d_info.lut_size = segment_info->entries_count;
		colorops[i]->custom_lut1d_info.lut =
			malloc(sizeof(struct drm_color_lut_32) * colorops[i]->custom_lut1d_info.lut_size);
		igt_assert(colorops[i]->custom_lut1d_info.lut);

		switch (colorops[i]->custom_lut1d_info.lut_type) {
			case KMS_COLOROP_CUSTOM_LUT1D_ZERO:
				create_zero_lut(segment_info, colorops[i]->custom_lut1d_info.lut);
				break;
			case KMS_COLOROP_CUSTOM_LUT1D_LINEAR:
				create_unity_lut(segment_info, colorops[i]->custom_lut1d_info.lut);
				break;
			case KMS_COLOROP_CUSTOM_LUT1D_MAX:
			default:
				create_max_lut(segment_info, colorops[i]->custom_lut1d_info.lut);
		}

		clear_segment_data(segment_info);
	}
}

static bool ctm_colorop_only(kms_colorop_t *colorops[])
{
	int i;

	for (i = 0; colorops[i]; i++)
		if (colorops[i]->type != KMS_COLOROP_CTM_3X3)
			return false;
	return true;
}

static bool can_use_colorop(igt_display_t *display, igt_colorop_t *colorop, kms_colorop_t *desired)
{

        switch (desired->type) {
        case KMS_COLOROP_ENUMERATED_LUT1D:
                if (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_1D_CURVE &&
                    igt_colorop_try_prop_enum(colorop, IGT_COLOROP_CURVE_1D_TYPE, kms_colorop_lut1d_tf_names[desired->enumerated_lut1d_info.tf]))
                        return true;
                return false;
        case KMS_COLOROP_CTM_3X3:
                return (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_CTM_3X3);
        case KMS_COLOROP_CTM_3X4:
                return (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_CTM_3X4);
        case KMS_COLOROP_CUSTOM_LUT1D:
                if (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_1D_LUT)
                        return true;
                return false;
        case KMS_COLOROP_CUSTOM_LUT1D_MULTSEG:
	{
                return (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_1D_LUT_MULTSEG);
	}
        case KMS_COLOROP_MULTIPLIER:
                return (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_MULTIPLIER);
        case KMS_COLOROP_LUT3D:
                return (igt_colorop_get_prop(display, colorop, IGT_COLOROP_TYPE) == DRM_COLOROP_3D_LUT);
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

        current_op = colorops[i];
        i++;
        igt_require(current_op);

        while (next) {
                if (can_use_colorop(display, next, current_op)) {
                        current_op->colorop = next;
                        current_op = colorops[i];
                        i++;
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
		igt_critical("color pipelines id %d\n", plane->color_pipelines[i]->id);
                if (map_to_pipeline(display, plane->color_pipelines[i], colorops)) {
                        colorop = plane->color_pipelines[i];
                        break;
                }
        }

        return colorop;
}


static void set_color_pipeline_bypass(igt_plane_t *plane)
{
        igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_PIPELINE, "Bypass");
}

static void fill_custom_1dlut(igt_display_t *display, kms_colorop_t *colorop)
{
        uint64_t lut_size = igt_colorop_get_prop(display, colorop->colorop, IGT_COLOROP_SIZE);
        igt_pixel_t pixel;
        float index;
        int i;

        for (i = 0; i < lut_size; i++) {
                index = i / (float) lut_size;

                pixel.r = index;
                pixel.g = index;
                pixel.b = index;

                colorop->transform(&pixel);

                colorop->lut1d->lut[i].red = pixel.r * 0xffff;
                colorop->lut1d->lut[i].green = pixel.g * 0xffff;
                colorop->lut1d->lut[i].blue = pixel.b * 0xffff;
        }
}

static void configure_3dlut(igt_display_t *display, kms_colorop_t *colorop,
                                  struct drm_mode_3dlut_mode *modes,
                                  uint64_t num_mode) {
        uint64_t lut_size = 0;
        uint64_t i;
        igt_3dlut_norm_t *igt_3dlut;

        /* Convert 3DLUT floating points to u16 required by colorop API */
        lut_size = colorop->lut3d_mode.lut_size * colorop->lut3d_mode.lut_size * colorop->lut3d_mode.lut_size;
        igt_3dlut = (igt_3dlut_norm_t *) malloc(sizeof(struct drm_color_lut) * lut_size);
        for (i = 0; i < lut_size; i++) {
                struct igt_color_lut_float *lut_f = &colorop->lut3d->lut[i];
                igt_3dlut->lut[i].red = round (lut_f->red * 0xFFFF);
                igt_3dlut->lut[i].green = round (lut_f->green * 0xFFFF);
                igt_3dlut->lut[i].blue = round (lut_f->blue * 0xFFFF);
        }

        /* find the exact lut mode supported by a kms_colorop_3dlut_* test */
        for (i = 0; i < num_mode; i++)
                if (!memcmp(&colorop->lut3d_mode, &modes[i], sizeof(struct drm_mode_3dlut_mode)))
                        break;

        igt_skip_on_f(i == num_mode, "no matching 3dlut mode\n");
        igt_colorop_set_prop_value(colorop->colorop, IGT_COLOROP_3DLUT_MODE_INDEX, i);

        lut_size = modes[i].lut_stride[0] * modes[i].lut_stride[1] * modes[i].lut_stride[2];
        igt_colorop_set_3dlut(display, colorop->colorop, igt_3dlut, lut_size * sizeof(struct drm_color_lut));
        free(igt_3dlut);
}

static void set_colorop(igt_display_t *display,
                        kms_colorop_t *colorop)
{
        drmModePropertyBlobPtr blob = NULL;
        struct drm_mode_3dlut_mode *modes;
        uint64_t lut_size = 0;
        uint64_t mult = 1;
        uint64_t blob_id;

        igt_assert(colorop->colorop);
        igt_colorop_set_prop_value(colorop->colorop, IGT_COLOROP_BYPASS, 0);

        switch (colorop->type) {
        case KMS_COLOROP_ENUMERATED_LUT1D:
                igt_colorop_set_prop_enum(colorop->colorop, IGT_COLOROP_CURVE_1D_TYPE, kms_colorop_lut1d_tf_names[colorop->enumerated_lut1d_info.tf]);
                break;
        case KMS_COLOROP_CTM_3X3:
                igt_colorop_set_ctm_3x3(display, colorop->colorop, colorop->matrix_3x3);
                break;
        case KMS_COLOROP_CTM_3X4:
                igt_colorop_set_ctm_3x4(display, colorop->colorop, colorop->matrix_3x4);
                break;
        case KMS_COLOROP_CUSTOM_LUT1D:
                fill_custom_1dlut(display, colorop);
                lut_size = igt_colorop_get_prop(display, colorop->colorop, IGT_COLOROP_SIZE);
                igt_colorop_set_custom_1dlut(display, colorop->colorop, colorop->lut1d, lut_size * sizeof(struct drm_color_lut));
                break;
        case KMS_COLOROP_CUSTOM_LUT1D_MULTSEG:
                igt_colorop_set_custom_lut_1d_multseg(display, colorop->colorop, colorop->custom_lut1d_info);
                break;
        case KMS_COLOROP_MULTIPLIER:
                mult = colorop->multiplier * (mult << 32);      /* convert double to fixed number */
                igt_colorop_set_prop_value(colorop->colorop, IGT_COLOROP_MULTIPLIER, mult);
                break;
        case KMS_COLOROP_LUT3D:
                blob_id = igt_colorop_get_prop(display, colorop->colorop, IGT_COLOROP_3DLUT_MODES);
                igt_assert(blob_id);
                blob = drmModeGetPropertyBlob(display->drm_fd, blob_id);
                igt_assert(blob);
                modes = (struct drm_mode_3dlut_mode *) blob->data;
                configure_3dlut(display, colorop, modes, blob->length / sizeof(struct drm_mode_3dlut_mode));
                break;
        default:
                igt_fail(IGT_EXIT_FAILURE);
        }
}

static void clear_colorop(igt_display_t *display, kms_colorop_t *colorop)
{
	igt_assert(colorop->colorop);
	igt_colorop_set_prop_value(colorop->colorop, IGT_COLOROP_BYPASS, 1);

	switch (colorop->type) {
	case KMS_COLOROP_CTM_3X3:
	case KMS_COLOROP_CTM_3X4:
	case KMS_COLOROP_CUSTOM_LUT1D:
		igt_colorop_replace_prop_blob(colorop->colorop, IGT_COLOROP_DATA, NULL, 0);
		break;
	case KMS_COLOROP_ENUMERATED_LUT1D:
	case KMS_COLOROP_LUT3D:
	default:
		return;
	}
}

static void clear_color_pipeline(igt_display_t *display,
			  igt_plane_t *plane,
			  kms_colorop_t *colorops[],
			  igt_colorop_t *color_pipeline)
{
	int i;

	for(i = 0; colorops[i]; i++)
		clear_colorop(display, colorops[i]);
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

	/* Set everything to bypass */
        next = color_pipeline;

	while (next) {
		igt_colorop_set_prop_value(next, IGT_COLOROP_BYPASS, 1);

		prop_val = igt_colorop_get_prop(display, next,
				IGT_COLOROP_NEXT);
		next = igt_find_colorop(display, prop_val);
	}

        for(i = 0; colorops[i]; i++)
                set_colorop(display, colorops[i]);
}

static bool test_plane_colorops(data_t *data,
				const color_t *fb_colors,
				const color_t *exp_colors,
				kms_colorop_t *colorops[])
{
	igt_plane_t *plane = data->primary;
	igt_output_t *output = data->output;
	igt_display_t *display = &data->display;
	drmModeModeInfo *mode = data->mode;
	struct igt_fb fb;
	bool ret;
	igt_colorop_t *color_pipeline = get_color_pipeline(display, plane, colorops);
	igt_crc_t crc_gamma, crc_fullcolors;

	igt_output_set_pipe(output, plane->pipe->pipe);
	mode = igt_output_get_mode(output);

	/* Create a framebuffer at the size of the output. */
	igt_assert(igt_create_fb(data->drm_fd,
			      mode->hdisplay,
			      mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      DRM_FORMAT_MOD_LINEAR,
			      &fb));
	igt_plane_set_fb(plane, &fb);

	/* Disable Pipe color props. */
	disable_ctm(plane->pipe);
	disable_degamma(plane->pipe);
	disable_gamma(plane->pipe);
	igt_display_commit2(display, COMMIT_ATOMIC);

	set_color_pipeline_bypass(plane);
	paint_rectangles(data, mode, exp_colors, &fb);
	igt_plane_set_fb(plane, &fb);
	igt_display_commit2(display, COMMIT_ATOMIC);
	igt_wait_for_vblank(data->drm_fd,
		display->pipes[plane->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

	/*
	 * Draw gradient colors with LUT to remap all
	 * values to max red/green/blue.
	 */
	prepare_lut_data(data, colorops);
	set_color_pipeline(display, plane, colorops, color_pipeline);
	if (ctm_colorop_only(colorops))
		paint_rectangles(data, mode, fb_colors, &fb);
	else
		paint_gradient_rectangles(data, mode, fb_colors, &fb);
	igt_plane_set_fb(plane, &fb);
	igt_display_commit2(display, COMMIT_ATOMIC);
	igt_wait_for_vblank(data->drm_fd,
			display->pipes[plane->pipe->pipe].crtc_offset);
	igt_pipe_crc_collect_crc(data->pipe_crc, &crc_gamma);

	/*
	 * Verify that the CRC of the software computed output is
	 * equal to the CRC of the gamma LUT transformation output.
	 */
	ret = igt_check_crc_equal(&crc_gamma, &crc_fullcolors);

	clear_lut_data(colorops);
	clear_color_pipeline(display, plane, colorops, color_pipeline);
	igt_plane_set_fb(plane, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit2(display, COMMIT_ATOMIC);

	return ret;
}

static void
prep_pipe(data_t *data, enum pipe p)
{
	igt_require_pipe(&data->display, p);

	if (igt_pipe_obj_has_prop(&data->display.pipes[p], IGT_CRTC_DEGAMMA_LUT_SIZE)) {
		data->degamma_lut_size =
			igt_pipe_obj_get_prop(&data->display.pipes[p],
					      IGT_CRTC_DEGAMMA_LUT_SIZE);
		igt_assert_lt(0, data->degamma_lut_size);
	}

	if (igt_pipe_obj_has_prop(&data->display.pipes[p], IGT_CRTC_GAMMA_LUT_SIZE)) {
		data->gamma_lut_size =
			igt_pipe_obj_get_prop(&data->display.pipes[p],
					      IGT_CRTC_GAMMA_LUT_SIZE);
		igt_assert_lt(0, data->gamma_lut_size);
	}
}

static void test_setup(data_t *data, enum pipe p)
{
	igt_pipe_t *pipe;

	prep_pipe(data, p);
	igt_require_pipe_crc(data->drm_fd);

	pipe = &data->display.pipes[p];
	igt_require(pipe->n_planes >= 0);

	data->primary = igt_pipe_get_plane_type(pipe, DRM_PLANE_TYPE_PRIMARY);
	data->pipe_crc = igt_pipe_crc_new(data->drm_fd,
					  data->primary->pipe->pipe,
					  IGT_PIPE_CRC_SOURCE_AUTO);

	igt_display_require_output_on_pipe(&data->display, p);
	data->output = igt_get_single_output_for_pipe(&data->display, p);
	igt_require(data->output);

	igt_display_reset(&data->display);
}

static void test_cleanup(data_t *data)
{
	igt_pipe_crc_free(data->pipe_crc);
	data->pipe_crc = NULL;
}

static void
run_gamma_degamma_tests_for_pipe(data_t *data, enum pipe p,
				 bool (*test_t)(data_t*, igt_plane_t*))
{
	test_setup(data, p);

	/*
	 * We assume an 8bits depth per color for degamma/gamma LUTs
	 * for CRC checks with framebuffer references.
	 */
	data->color_depth = 8;
	data->drm_format = DRM_FORMAT_XRGB8888;
	data->mode = igt_output_get_mode(data->output);

	if (!pipe_output_combo_valid(data, p))
		goto out;

	igt_dynamic_f("pipe-%s-%s", kmstest_pipe_name(p), igt_output_name(data->output))
		igt_assert(test_t(data, data->primary));
out:
	test_cleanup(data);
}

static void transform_color(color_t *color, const double *ctm, double offset)
{
	color_t tmp = *color;

	color->r = ctm[0] * tmp.r + ctm[1] * tmp.g + ctm[2] * tmp.b + offset;
	color->g = ctm[3] * tmp.r + ctm[4] * tmp.g + ctm[5] * tmp.b + offset;
	color->b = ctm[6] * tmp.r + ctm[7] * tmp.g + ctm[8] * tmp.b + offset;
}

static void
run_ctm_tests_for_pipe(data_t *data, enum pipe p,
		       const color_t *fb_colors,
		       const double *ctm,
		       int iter)
{
	bool success = false;
	double delta;
	int i;

	test_setup(data, p);

	/*
	 * We assume an 8bits depth per color for degamma/gamma LUTs
	 * for CRC checks with framebuffer references.
	 */
	data->color_depth = 8;
	delta = 1.0 / (1 << data->color_depth);
	data->drm_format = DRM_FORMAT_XRGB8888;
	data->mode = igt_output_get_mode(data->output);

	if (!pipe_output_combo_valid(data, p))
		goto out;

	if (!iter)
		iter = 1;

	igt_dynamic_f("pipe-%s-%s", kmstest_pipe_name(p), igt_output_name(data->output)) {
		/*
		 * We tests a few values around the expected result because
		 * it depends on the hardware we're dealing with, we can either
		 * get clamped or rounded values and we also need to account
		 * for odd number of items in the LUTs.
		 */
		for (i = 0; i < iter; i++) {
			color_t expected_colors[3] = {
				fb_colors[0],
				fb_colors[1],
				fb_colors[2],
			};

			transform_color(&expected_colors[0], ctm, delta * (i - (iter / 2)));
			transform_color(&expected_colors[1], ctm, delta * (i - (iter / 2)));
			transform_color(&expected_colors[2], ctm, delta * (i - (iter / 2)));

			if (test_pipe_ctm(data, data->primary, fb_colors,
					  expected_colors, ctm)) {
				success = true;
				break;
			}
		}
		igt_assert(success);
	}

out:
	test_cleanup(data);
}

static void run_plane_color_tests(data_t *data,
				  const color_t *fb_colors,
				  const color_t *exp_colors,
				  kms_colorop_t *colorops[])
{
	enum pipe pipe;

	data->color_depth = 8;
	data->drm_format = DRM_FORMAT_XRGB8888;

	for_each_pipe(&data->display, pipe) {
		test_setup(data, pipe);

		data->mode = igt_output_get_mode(data->output);

		if (!pipe_output_combo_valid(data, pipe)){
			test_cleanup(data);
			continue;
		}

		/*
		 * TODO: Extend the test to multiple planes?
		 * Since, Intel planes (HDR & SDR) have different capabilities.
		 */
		if (!igt_plane_has_prop(data->primary, IGT_PLANE_COLOR_PIPELINE))
			continue;

		igt_dynamic_f("pipe-%s-%s",
			      kmstest_pipe_name(pipe),
			      igt_output_name(data->output))
			igt_assert(test_plane_colorops(data, fb_colors, exp_colors, colorops));
	}

	test_cleanup(data);
}

static void
run_deep_color_tests_for_pipe(data_t *data, enum pipe p)
{
	igt_output_t *output;
	static const color_t blue_green_blue[] = {
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	static const color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	static const double ctm[] = {
		0.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		1.0, 0.0, 1.0,
	};

	if (is_intel_device(data->drm_fd))
		igt_require_f((intel_display_ver(data->devid) >= 11),
				"At least GEN 11 is required to validate Deep-color.\n");

	test_setup(data, p);

	for_each_valid_output_on_pipe(&data->display, p, output) {
		uint64_t max_bpc = get_max_bpc(output);
		bool ret;

		if (!max_bpc)
			continue;

		if (!panel_supports_deep_color(data->drm_fd, output->name))
			continue;
		/*
		 * In intel driver, for MST streams pipe_bpp is
		 * restricted to 8bpc. So, deep-color >= 10bpc
		 * will never work for DP-MST even if the panel
		 * supports 10bpc. Once KMD FIXME, is resolved
		 * this MST constraint can be removed.
		 */
		if (is_intel_device(data->drm_fd) && igt_check_output_is_dp_mst(output))
			continue;

		igt_display_reset(&data->display);
		igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, 10);
		igt_output_set_pipe(output, p);

		if (is_intel_device(data->drm_fd) &&
		    !igt_max_bpc_constraint(&data->display, p, output, 10))
			continue;

		data->color_depth = 10;
		data->drm_format = DRM_FORMAT_XRGB2101010;
		data->output = output;

		data->mode = malloc(sizeof(drmModeModeInfo));
		igt_assert(data->mode);
		memcpy(data->mode, igt_output_get_mode(data->output), sizeof(drmModeModeInfo));

		igt_dynamic_f("pipe-%s-%s-gamma", kmstest_pipe_name(p), output->name) {
			igt_display_reset(&data->display);
			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, 10);

			ret = test_pipe_gamma(data, data->primary);

			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, max_bpc);
			igt_assert(ret);
		}

		igt_dynamic_f("pipe-%s-%s-degamma", kmstest_pipe_name(p), output->name) {
			igt_display_reset(&data->display);
			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, 10);

			ret = test_pipe_degamma(data, data->primary);

			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, max_bpc);
			igt_assert(ret);
		}

		igt_dynamic_f("pipe-%s-%s-ctm", kmstest_pipe_name(p), output->name) {
			igt_display_reset(&data->display);
			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, 10);

			ret = test_pipe_ctm(data, data->primary,
					    red_green_blue,
					    blue_green_blue, ctm);

			igt_output_set_prop_value(output, IGT_CONNECTOR_MAX_BPC, max_bpc);
			igt_assert(ret);
		}

		free(data->mode);

		break;
	}

	test_cleanup(data);
}

static void
run_invalid_tests_for_pipe(data_t *data)
{
	enum pipe pipe;
	struct {
		const char *name;
		void (*test_t) (data_t *data, enum pipe pipe);
		const char *desc;
	} tests[] = {
		{ "invalid-gamma-lut-sizes", invalid_gamma_lut_sizes,
			"Negative check for invalid gamma lut sizes" },

		{ "invalid-degamma-lut-sizes", invalid_degamma_lut_sizes,
			"Negative check for invalid degamma lut sizes" },

		{ "invalid-ctm-matrix-sizes", invalid_ctm_matrix_sizes,
			"Negative check for color tranformation matrix sizes" },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		igt_describe_f("%s", tests[i].desc);
		igt_subtest_with_dynamic_f("%s", tests[i].name) {
			for_each_pipe(&data->display, pipe) {
				igt_dynamic_f("pipe-%s", kmstest_pipe_name(pipe)) {
					prep_pipe(data, pipe);
					tests[i].test_t(data, pipe);
				}
			}
		}
	}
}

static void
run_tests_for_pipe(data_t *data)
{
	enum pipe pipe;
	static const struct {
		const char *name;
		bool (*test_t)(data_t*, igt_plane_t*);
		const char *desc;
	} gamma_degamma_tests[] = {
		{ .name = "degamma",
		  .test_t = test_pipe_degamma,
		  .desc = "Verify that degamma LUT transformation works correctly",
		},
		{ .name = "gamma",
		  .test_t = test_pipe_gamma,
		  .desc = "Verify that gamma LUT transformation works correctly",
		},
		{ .name = "legacy-gamma",
		  .test_t = test_pipe_legacy_gamma,
		  .desc = "Verify that legacy gamma LUT transformation works correctly",
		},
		{ .name = "legacy-gamma-reset",
		  .test_t = test_pipe_legacy_gamma_reset,
		  .desc = "Verify that setting the legacy gamma LUT resets the gamma LUT set through GAMMA_LUT property",
		},
	};
	static const color_t colors_rgb[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 },
	};
	static const color_t colors_cmy[] = {
		{ 0.0, 1.0, 1.0 },
		{ 1.0, 0.0, 1.0 },
		{ 1.0, 1.0, 0.0 }
	};
	static const struct {
		const char *name;
		int iter;
		const color_t *fb_colors;
		double ctm[9];
		const char *desc;
	} ctm_tests[] = {
		{ .name = "ctm-red-to-blue",
		  .fb_colors = colors_rgb,
		  .ctm = {
			  0.0, 0.0, 0.0,
			  0.0, 1.0, 0.0,
			  1.0, 0.0, 1.0,
		  },
		  .desc = "Check the color transformation from red to blue",
		},
		{ .name = "ctm-green-to-red",
		  .fb_colors = colors_rgb,
		  .ctm = {
			  1.0, 1.0, 0.0,
			  0.0, 0.0, 0.0,
			  0.0, 0.0, 1.0,
		  },
		  .desc = "Check the color transformation from green to red",
		},
		{ .name = "ctm-blue-to-red",
		  .fb_colors = colors_rgb,
		  .ctm = {
			  1.0, 0.0, 1.0,
			  0.0, 1.0, 0.0,
			  0.0, 0.0, 0.0,
		  },
		  .desc = "Check the color transformation from blue to red",
		},
		{ .name = "ctm-max",
		  .fb_colors = colors_rgb,
		  .ctm = { 100.0, 0.0, 0.0,
			  0.0, 100.0, 0.0,
			  0.0, 0.0, 100.0,
		  },
		  .desc = "Check the color transformation for maximum transparency",
		},
		{ .name = "ctm-negative",
		  .fb_colors = colors_rgb,
		  .ctm = {
			  -1.0,  0.0,  0.0,
			   0.0, -1.0,  0.0,
			   0.0,  0.0, -1.0,
		  },
		  .desc = "Check the color transformation for negative transparency",
		},
		{ .name = "ctm-0-25",
		  .iter = 5,
		  .fb_colors = colors_rgb,
		  .ctm = {
			  0.25, 0.0,  0.0,
			  0.0,  0.25, 0.0,
			  0.0,  0.0,  0.25,
		  },
		  .desc = "Check the color transformation for 0.25 transparency",
		},
		{ .name = "ctm-0-50",
		  .iter = 5,
		  .fb_colors = colors_rgb,
		  .ctm = {
			  0.5,  0.0,  0.0,
			  0.0,  0.5,  0.0,
			  0.0,  0.0,  0.5,
		  },
		  .desc = "Check the color transformation for 0.5 transparency",
		},
		{ .name = "ctm-0-75",
		  .iter = 7,
		  .fb_colors = colors_rgb,
		  .ctm = {
			  0.75, 0.0,  0.0,
			  0.0,  0.75, 0.0,
			  0.0,  0.0,  0.75,
		  },
		  .desc = "Check the color transformation for 0.75 transparency",
		},
		{ .name = "ctm-signed",
		  .fb_colors = colors_cmy,
		  .iter = 3,
		  .ctm = {
			  -0.25,  0.75,  0.75,
			   0.75, -0.25,  0.75,
			   0.75,  0.75, -0.25,
		  },
		  .desc = "Check the color transformation for correct signed handling",
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(gamma_degamma_tests); i++) {
		igt_describe_f("%s", gamma_degamma_tests[i].desc);
		igt_subtest_with_dynamic_f("%s", gamma_degamma_tests[i].name) {
			for_each_pipe(&data->display, pipe) {
				run_gamma_degamma_tests_for_pipe(data, pipe,
								 gamma_degamma_tests[i].test_t);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(ctm_tests); i++) {
		igt_describe_f("%s", ctm_tests[i].desc);
		igt_subtest_with_dynamic_f("%s", ctm_tests[i].name) {
			for_each_pipe(&data->display, pipe) {
				run_ctm_tests_for_pipe(data, pipe,
						       ctm_tests[i].fb_colors,
						       ctm_tests[i].ctm,
						       ctm_tests[i].iter);
			}
		}
	}

	igt_subtest_group {
		static const color_t colors_red_to_blue[] = {
			{ 0.0, 0.0, 1.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, 1.0 },
		};
		const struct drm_color_ctm ctm_red_to_blue = { {
			0.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			1.0, 0.0, 1.0
		} };
		kms_colorop_t lut1d_linear = {
			.type = KMS_COLOROP_CUSTOM_LUT1D_MULTSEG,
			.name = "Pre/Post CSC GAMMA (linear LUT)",
			.custom_lut1d_info = {
				.lut_type = KMS_COLOROP_CUSTOM_LUT1D_LINEAR,
			},
		};
		kms_colorop_t lut1d_max = {
			.type = KMS_COLOROP_CUSTOM_LUT1D_MULTSEG,
			.name = "Pre/Post CSC GAMMA (max LUT)",
			.custom_lut1d_info = {
				.lut_type = KMS_COLOROP_CUSTOM_LUT1D_MAX,
			},
		};
		kms_colorop_t ctm_3x3 = {
			.type = KMS_COLOROP_CTM_3X3,
			.name = "CTM 3X3 (red to blue)",
			.matrix_3x3 = &ctm_red_to_blue,
		};
		struct {
			const char *name;
			const color_t *fb_colors;
			const color_t *exp_colors;
			kms_colorop_t *colorops[MAX_COLOROPS];
		} plane_colorops_tests[] = {
			{ .name = "lut1d",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_rgb,
			  .colorops = { &lut1d_max, NULL },
			},
			{ .name = "ctm3x3",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_red_to_blue,
			  .colorops = { &ctm_3x3, NULL },
			},
			{ .name = "lut1d-ctm3x3",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_red_to_blue,
			  .colorops = { &lut1d_max, &ctm_3x3, NULL },
			},
			{ .name = "ctm3x3-lut1d",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_red_to_blue,
			  .colorops = { &ctm_3x3, &lut1d_max, NULL },
			},
			{ .name = "lut1d-lut1d",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_rgb,
			  .colorops = { &lut1d_linear, &lut1d_max, NULL },
			},
			{ .name = "lut1d-ctm3x3-lut1d",
			  .fb_colors = colors_rgb,
			  .exp_colors = colors_red_to_blue,
			  .colorops = { &lut1d_linear, &ctm_3x3, &lut1d_max, NULL },
			},
		};

		igt_fixture
			igt_require(data->display.is_atomic);

		for (i = 0; i < ARRAY_SIZE(plane_colorops_tests); i++) {
			igt_describe_f("Test plane color pipeline with colorops: %s", plane_colorops_tests[i].name);
			igt_subtest_with_dynamic_f("plane-%s", plane_colorops_tests[i].name)
				run_plane_color_tests(data,
						      plane_colorops_tests[i].fb_colors,
						      plane_colorops_tests[i].exp_colors,
						      plane_colorops_tests[i].colorops);
		}

		igt_describe("Verify that deep color works correctly");
		igt_subtest_with_dynamic("deep-color") {
			for_each_pipe(&data->display, pipe) {
				run_deep_color_tests_for_pipe(data, pipe);
			}
		}
	}
}

igt_main
{
	data_t data = {};

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);
		if (is_intel_device(data.drm_fd))
			data.devid = intel_get_drm_devid(data.drm_fd);
		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
	}

	igt_subtest_group
		run_tests_for_pipe(&data);

	igt_subtest_group
		run_invalid_tests_for_pipe(&data);

	igt_fixture {
		igt_display_fini(&data.display);
		drm_close_driver(data.drm_fd);
	}
}
