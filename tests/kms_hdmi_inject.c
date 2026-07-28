/*
 * Copyright © 2017 Intel Corporation
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
 * TEST: kms hdmi inject
 * Category: Display
 * Description: Test that in-kernel EDID parsing is producing expected results
 *              by forcing a HDMI connector with a known EDID and checking that
 *              the metadata exposed to user space matches.
 * Driver requirement: i915, xe
 * Mega feature: General Display Features
 */

#include "config.h"

#include <dirent.h>

#include "igt.h"
#include "igt_edid.h"
#include "igt_eld.h"
#include "xe/xe_query.h"

/**
 * SUBTEST: inject-4k
 * Description: Make sure that 4K modes exposed by DRM match the forced EDID and
 *              modesetting using it succeed.
 *
 * SUBTEST: inject-audio
 * Description: Make sure that audio information exposed by ALSA match the forced
 *              EDID.
 *
 * SUBTEST: audio-bw-prune-reduced-blanking
 * Description: Force a reduced-blanking 2560x1080 mode whose hblank cannot carry
 *              high audio sample rates, and check that the kernel prunes the
 *              affected rates from the ELD and drops SADs that lose all rates.
 *
 * SUBTEST: audio-bw-no-prune-standard-blanking
 * Description: Force a standard-blanking 2560x1080 mode with ample hblank, and
 *              check that no SAD or sample rate is pruned from the ELD.
 */

#define HDISPLAY_4K	3840
#define VDISPLAY_4K	2160

IGT_TEST_DESCRIPTION("Test that in-kernel EDID parsing is producing "
		     "expected results by forcing a HDMI connector "
		     "with a known EDID and checking that the metadata "
		     "exposed to user space matches.");

/**
 * This collection of tests performs EDID and status injection tests. Injection
 * forces a given EDID and status on a connector. The kernel will parse the
 * forced EDID and we will check whether correct metadata is exposed to
 * userspace.
 *
 * Currently, this can be used to test:
 *
 * - 4K modes exposed via KMS
 * - Audio capabilities of the monitor exposed via ALSA. EDID-Like Data (ELD)
 *   entries in /proc/asound are verified.
 * - HDMI TMDS audio-bandwidth pruning of the ELD Short Audio Descriptors: for
 *   a reduced-blanking mode the kernel drops the sample rates (and whole SADs)
 *   that do not fit in the horizontal blanking.
 */

/** get_connector: get the first disconnected HDMI connector */
static drmModeConnector *
get_connector(int drm_fd, drmModeRes *res)
{
	int i;
	drmModeConnector *connector = NULL;

	for (i = 0; i < res->count_connectors; i++) {

		connector =
			drmModeGetConnectorCurrent(drm_fd, res->connectors[i]);

		if (connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
		    connector->connector_type == DRM_MODE_CONNECTOR_HDMIB)
			break;

		drmModeFreeConnector(connector);
		connector = NULL;
	}

	return connector;
}

static void
hdmi_inject_4k(int drm_fd, drmModeConnector *connector)
{
	const struct edid *edid;
	struct kmstest_connector_config config;
	int ret, cid, i, crtc_mask = -1;
	int fb_id;
	struct igt_fb fb;
	uint8_t found_4k_mode = 0;

	if (is_intel_device(drm_fd)) {
		uint32_t devid = intel_get_drm_devid(drm_fd);

		/* 4K requires at least HSW */
		igt_require(IS_HASWELL(devid) || intel_display_ver(devid) >= 8);
	}

	edid = igt_kms_get_4k_edid();
	kmstest_force_edid(drm_fd, connector, edid);

	igt_skip_on_f(!kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_ON),
		      "Could not force connector on\n");

	cid = connector->connector_id;

	connector = drmModeGetConnectorCurrent(drm_fd, cid);

	for (i = 0; i < connector->count_modes; i++) {
		if (connector->modes[i].hdisplay == HDISPLAY_4K &&
		    connector->modes[i].vdisplay == VDISPLAY_4K) {
			found_4k_mode++;
			break;
		}
	}

	igt_assert(found_4k_mode);

	/* create a configuration */
	ret = kmstest_get_connector_config(drm_fd, cid, crtc_mask, &config);
	igt_assert(ret);

	igt_info("  ");
	kmstest_dump_mode(&connector->modes[i]);

	/* create framebuffer */
	fb_id = igt_create_fb(drm_fd, connector->modes[i].hdisplay,
			      connector->modes[i].vdisplay,
			      DRM_FORMAT_XRGB8888,
			      DRM_FORMAT_MOD_LINEAR, &fb);

	ret = drmModeSetCrtc(drm_fd, config.crtc->crtc_id, fb_id, 0, 0,
			     &connector->connector_id, 1,
			     &connector->modes[i]);

	igt_assert_eq(ret, 0);

	igt_remove_fb(drm_fd, &fb);

	kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_UNSPECIFIED);
	kmstest_force_edid(drm_fd, connector, NULL);
}

static void
hdmi_inject_audio(int drm_fd, drmModeConnector *connector)
{
	const struct edid *edid;
	int fb_id, cid, ret, crtc_mask = -1;
	struct igt_fb fb;
	struct kmstest_connector_config config;

	igt_require(eld_is_supported());

	edid = igt_kms_get_hdmi_audio_edid();
	kmstest_force_edid(drm_fd, connector, edid);

	igt_skip_on_f(!kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_ON),
		      "Could not force connector on\n");

	cid = connector->connector_id;
	connector = drmModeGetConnectorCurrent(drm_fd, cid);

	/* create a configuration */
	ret = kmstest_get_connector_config(drm_fd, cid, crtc_mask, &config);
	igt_assert(ret);

	/*
	 * Create a framebuffer as to allow the kernel to enable the pipe and
	 * enable the audio encoder.
	 */
	fb_id = igt_create_fb(drm_fd, connector->modes[0].hdisplay,
			      connector->modes[0].vdisplay,
			      DRM_FORMAT_XRGB8888,
			      DRM_FORMAT_MOD_LINEAR, &fb);

	ret = drmModeSetCrtc(drm_fd, config.crtc->crtc_id, fb_id, 0, 0,
			     &connector->connector_id, 1,
			     &connector->modes[0]);


	igt_assert_eq(ret, 0);

	/*
	 * Test if we have /proc/asound/HDMI/eld#0.0 and is its contents are
	 * valid.
	 */
	if (!eld_has_igt()) {
		igt_info("HDMI audio enabled but ELD not visible yet; retrying\n");
		usleep(200 * 1000);

		igt_assert_f(eld_has_igt(),
			     "HDMI ELD missing after retry\n");
	}

	igt_remove_fb(drm_fd, &fb);

	igt_info("  ");
	kmstest_dump_mode(&connector->modes[0]);

	kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_UNSPECIFIED);
	kmstest_force_edid(drm_fd, connector, NULL);
}

/*
 * HDMI TMDS audio-bandwidth pruning of the ELD Short Audio Descriptors.
 *
 * Two timings, same 2560x1080 resolution and default 8bpc RGB output:
 *
 *   reduced blanking : hblank=160  -> TB_blank=160 TMDS cycles/line. At >2ch a
 *                      packet needs 32*ceil(...)+74 cycles; only 32/44.1/48 kHz
 *                      fit, so 88.2 kHz and up are pruned.
 *   standard blanking: hblank=864  -> TB_blank=864, everything fits.
 *
 * See the audio-BW algorithm in intel_audio.c (BSpec 68944).
 */
static drmModeModeInfo mode_reduced = {
	.clock = 119000,
	.hdisplay = 2560, .hsync_start = 2608, .hsync_end = 2640, .htotal = 2720,
	.vdisplay = 1080, .vsync_start = 1083, .vsync_end = 1088, .vtotal = 1101,
	.vrefresh = 40,
	.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
	.name = "2560x1080_reduced",
};

static drmModeModeInfo mode_standard = {
	.clock = 119000,
	.hdisplay = 2560, .hsync_start = 2656, .hsync_end = 2720, .htotal = 3360,
	.vdisplay = 1080, .vsync_start = 1083, .vsync_end = 1088, .vtotal = 1180,
	.vrefresh = 30,
	.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
	.name = "2560x1080_standard",
};

/* Only rates that igt_eld's parser recognises: 32/44.1/48/96/192 kHz. */
#define R32  CEA_SAD_SAMPLING_RATE_32KHZ
#define R44  CEA_SAD_SAMPLING_RATE_44KHZ
#define R48  CEA_SAD_SAMPLING_RATE_48KHZ
#define R96  CEA_SAD_SAMPLING_RATE_96KHZ
#define R192 CEA_SAD_SAMPLING_RATE_192KHZ

/*
 * SADs are LPCM (igt_eld only parses LPCM) with distinct channel counts so each
 * can be matched unambiguously in the read-back ELD.
 */
struct sad_desc {
	int channels;
	uint8_t rates_in;	/* rates advertised in the EDID */
	uint8_t rates_reduced;	/* expected rates after pruning on mode_reduced */
};

static const struct sad_desc sad_descs[] = {
	{ .channels = 2, .rates_in = R32 | R48 | R96 | R192, .rates_reduced = R32 | R48 | R96 | R192 },
	{ .channels = 8, .rates_in = R48 | R96 | R192,       .rates_reduced = R48 },
	{ .channels = 6, .rates_in = R96 | R192,             .rates_reduced = 0 /* whole SAD removed */ },
	{ .channels = 4, .rates_in = R32 | R44 | R48,        .rates_reduced = R32 | R44 | R48 },
};

#define NUM_SADS (sizeof(sad_descs) / sizeof(sad_descs[0]))
#define AUDIO_BW_EDID_SIZE (2 * EDID_BLOCK_SIZE)

static const struct edid *
build_audio_bw_edid(unsigned char raw[static AUDIO_BW_EDID_SIZE],
		    drmModeModeInfo *mode)
{
	struct edid *edid = (struct edid *)raw;
	struct edid_ext *ext;
	struct edid_cea *cea;
	char *cea_data;
	struct edid_cea_data_block *block;
	const struct cea_vsdb *vsdb;
	struct cea_sad sads[NUM_SADS];
	struct cea_speaker_alloc speaker_alloc = { .speakers = CEA_SPEAKER_FRONT_LEFT_RIGHT };
	size_t size = 0, vsdb_size;
	unsigned int i;

	for (i = 0; i < NUM_SADS; i++)
		cea_sad_init_pcm(&sads[i], sad_descs[i].channels,
				 sad_descs[i].rates_in,
				 CEA_SAD_SAMPLE_SIZE_16 | CEA_SAD_SAMPLE_SIZE_20 |
				 CEA_SAD_SAMPLE_SIZE_24);

	/* Base EDID carries the forced mode as its preferred timing. */
	edid_init_with_mode(edid, mode);
	edid->extensions_len = 1;
	ext = &edid->extensions[0];
	cea = &ext->data.cea;
	cea_data = cea->data;

	/* Short Audio Descriptor block */
	block = (struct edid_cea_data_block *)&cea_data[size];
	size += edid_cea_data_block_set_sad(block, sads, NUM_SADS);

	/* HDMI Vendor Specific Data Block (required for HDMI audio).
	 * Force 8bpc by clearing the deep-color support flags (DC_30BIT, DC_36BIT).
	 */
	block = (struct edid_cea_data_block *)&cea_data[size];
	vsdb = cea_vsdb_get_hdmi_default(&vsdb_size);
	/* Modify the VSDB to clear deep-color flags: DC_30BIT (0x10) and DC_36BIT (0x20) */
	((struct hdmi_vsdb *)&vsdb->data.hdmi)->flags1 &= ~(HDMI_VSDB_DC_30BIT | HDMI_VSDB_DC_36BIT);
	size += edid_cea_data_block_set_vsdb(block, vsdb, vsdb_size);

	/* Speaker Allocation Data Block */
	block = (struct edid_cea_data_block *)&cea_data[size];
	size += edid_cea_data_block_set_speaker_alloc(block, &speaker_alloc);

	igt_assert(size <= sizeof(cea->data));
	edid_ext_set_cea(ext, size, 0, EDID_CEA_BASIC_AUDIO);
	edid_update_checksum(edid);

	return edid;
}

/* Find the forced mode in the connector's mode list. */
static int find_mode(drmModeConnector *connector, const drmModeModeInfo *want)
{
	int i;

	for (i = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *m = &connector->modes[i];

		if (m->hdisplay == want->hdisplay &&
		    m->vdisplay == want->vdisplay &&
		    m->htotal == want->htotal &&
		    m->clock == want->clock)
			return i;
	}

	return -1;
}

static const struct eld_sad *find_sad(const struct eld_entry *eld, int channels)
{
	size_t i;

	for (i = 0; i < eld->sads_len; i++)
		if (eld->sads[i].channels == channels)
			return &eld->sads[i];

	return NULL;
}

static const char *rate_mask_str(uint8_t mask, char *buf, size_t sz)
{
	const char *rates[] = { "32k", "44.1k", "48k", "88.2k", "96k", "176.4k", "192k" };
	size_t pos = 0;
	int i;

	for (i = 0; i < 7; i++) {
		if (mask & (1 << i)) {
			if (pos > 0)
				pos += snprintf(buf + pos, sz - pos, "|");
			pos += snprintf(buf + pos, sz - pos, "%s", rates[i]);
		}
	}
	if (pos == 0)
		snprintf(buf, sz, "(empty)");
	return buf;
}

/*
 * Force the EDID + mode, modeset, and return the ELD ALSA exposes. The pipe and
 * audio encoder must be enabled for the ELD to appear, so a framebuffer is set.
 */
static void get_eld_for_mode(int drm_fd, drmModeConnector *connector,
			     drmModeModeInfo *mode, struct eld_entry *eld)
{
	static unsigned char raw_edid[AUDIO_BW_EDID_SIZE];
	const struct edid *edid;
	struct kmstest_connector_config config;
	struct igt_fb fb;
	drmModeConnector *fresh_connector;
	int cid, mode_idx, fb_id, ret;

	edid = build_audio_bw_edid(raw_edid, mode);
	kmstest_force_edid(drm_fd, connector, edid);

	igt_skip_on_f(!kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_ON),
		      "Could not force connector on\n");

	cid = connector->connector_id;
	fresh_connector = drmModeGetConnectorCurrent(drm_fd, cid);

	mode_idx = find_mode(fresh_connector, mode);
	igt_assert_f(mode_idx >= 0, "Forced mode %s not exposed by the kernel\n",
		     mode->name);

	ret = kmstest_get_connector_config(drm_fd, cid, -1, &config);
	igt_assert(ret);

	fb_id = igt_create_fb(drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR, &fb);

	ret = drmModeSetCrtc(drm_fd, config.crtc->crtc_id, fb_id, 0, 0,
			     &fresh_connector->connector_id, 1,
			     &fresh_connector->modes[mode_idx]);
	igt_assert_eq(ret, 0);

	/* ELD may appear slightly after the modeset; retry once. */
	if (!eld_has_igt()) {
		usleep(200 * 1000);
		igt_assert_f(eld_has_igt(), "HDMI ELD missing after modeset\n");
	}

	igt_assert_f(eld_get_igt(eld), "Could not read back the IGT ELD\n");

	igt_remove_fb(drm_fd, &fb);
	kmstest_free_connector_config(&config);
	drmModeFreeConnector(fresh_connector);
}

static void audio_bw_restore(int drm_fd, drmModeConnector *connector)
{
	kmstest_force_connector(drm_fd, connector, FORCE_CONNECTOR_UNSPECIFIED);
	kmstest_force_edid(drm_fd, connector, NULL);
}

static void hdmi_audio_bw_prune(int drm_fd, drmModeConnector *connector)
{
	struct eld_entry eld = {};
	const struct eld_sad *sad;
	unsigned int i, expect_len = 0;
	char buf_expected[64], buf_received[64];

	igt_require(is_intel_device(drm_fd));
	igt_require(eld_is_supported());

	get_eld_for_mode(drm_fd, connector, &mode_reduced, &eld);

	igt_info("ELD SAD count: received %u\n", (unsigned int)eld.sads_len);

	for (i = 0; i < NUM_SADS; i++)
		if (sad_descs[i].rates_reduced)
			expect_len++;

	igt_info("  expected %u (after pruning)\n", expect_len);

	for (i = 0; i < NUM_SADS; i++) {
		int ch = sad_descs[i].channels;

		igt_info("SAD[%d] %dch: ", i, ch);

		if (sad_descs[i].rates_reduced == 0) {
			igt_info("expected removal\n");
			sad = find_sad(&eld, ch);
			igt_assert_f(!sad,
				     "%dch SAD should have been removed but is present (rates: %s)\n",
				     ch, rate_mask_str(sad ? sad->rates : 0, buf_received, sizeof(buf_received)));
			continue;
		}

		sad = find_sad(&eld, ch);
		igt_info("expected: %s, ",
			 rate_mask_str(sad_descs[i].rates_reduced, buf_expected, sizeof(buf_expected)));

		igt_assert_f(sad, "missing from ELD\n");

		igt_info("received: %s\n",
			 rate_mask_str(sad->rates, buf_received, sizeof(buf_received)));

		igt_assert_f(sad->rates == sad_descs[i].rates_reduced,
			     "mismatch: 0x%x vs 0x%x\n",
			     sad->rates, sad_descs[i].rates_reduced);
	}

	audio_bw_restore(drm_fd, connector);
}

static void hdmi_audio_bw_no_prune(int drm_fd, drmModeConnector *connector)
{
	struct eld_entry eld = {};
	const struct eld_sad *sad;
	unsigned int i;
	char buf_expected[64], buf_received[64];

	igt_require(is_intel_device(drm_fd));
	igt_require(eld_is_supported());

	get_eld_for_mode(drm_fd, connector, &mode_standard, &eld);

	igt_info("ELD SAD count: received %u, expected %u (no pruning)\n",
		 (unsigned int)eld.sads_len, NUM_SADS);

	igt_assert_eq(eld.sads_len, NUM_SADS);

	for (i = 0; i < NUM_SADS; i++) {
		int ch = sad_descs[i].channels;

		sad = find_sad(&eld, ch);
		igt_info("SAD[%d] %dch: expected %s, ",
			 i, ch,
			 rate_mask_str(sad_descs[i].rates_in, buf_expected, sizeof(buf_expected)));

		igt_assert_f(sad, "missing from ELD\n");

		igt_info("received %s\n",
			 rate_mask_str(sad->rates, buf_received, sizeof(buf_received)));

		igt_assert_f(sad->rates == sad_descs[i].rates_in,
			     "mismatch: 0x%x vs 0x%x\n",
			     sad->rates, sad_descs[i].rates_in);
	}

	audio_bw_restore(drm_fd, connector);
}

int igt_main()
{
	int drm_fd;
	drmModeRes *res;
	drmModeConnector *connector;

	igt_fixture() {
		drm_fd = drm_open_driver_master(DRIVER_ANY);

		res = drmModeGetResources(drm_fd);
		igt_require(res);

		connector = get_connector(drm_fd, res);
		igt_require(connector);

		kmstest_unset_all_crtcs(drm_fd, res);
	}

	igt_describe("Make sure that 4K modes exposed by DRM match the "
                     "forced EDID and modesetting using it succeed.");
	igt_subtest("inject-4k")
		hdmi_inject_4k(drm_fd, connector);

	igt_describe("Make sure that audio information exposed by ALSA "
		     "match the forced EDID.");
	igt_subtest("inject-audio")
		hdmi_inject_audio(drm_fd, connector);

	igt_describe("Reduced-blanking mode: high audio sample rates and "
		     "rate-empty SADs are pruned from the ELD.");
	igt_subtest("audio-bw-prune-reduced-blanking")
		hdmi_audio_bw_prune(drm_fd, connector);

	igt_describe("Standard-blanking mode: nothing is pruned from the ELD.");
	igt_subtest("audio-bw-no-prune-standard-blanking")
		hdmi_audio_bw_no_prune(drm_fd, connector);

	igt_fixture() {
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		drm_close_driver(drm_fd);
	}
}
