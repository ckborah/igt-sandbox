// SPDX-License-Identifier: MIT
/*
 * Copyright © 2024 Intel Corporation
 */

#include <cairo.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "igt.h"
#include "intel_blt.h"
#include "intel_bufops.h"
#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"

/**
 * TEST: Copy memory using 3d engine
 * Category: Core
 * Mega feature: Render
 * Sub-category: 3d
 * Functionality: render_copy
 * Test category: functionality test
 *
 * SUBTEST: render-square
 * Description: Copy surface using 3d engine dividing to 2x2 squares
 *
 * SUBTEST: render-vstripes
 * Description: Copy surface using 3d engine dividing to 4x1 rectangles
 *
 * SUBTEST: render-hstripes
 * Description: Copy surface using 3d engine dividing to 1x4 rectangles
 *
 * SUBTEST: render-random
 * Description: Copy surface using 3d engine with randomized width, height and
 *              rectangles size
 *
 * SUBTEST: render-full
 * Description: Copy surface using 3d engine (1:1)
 *
 * SUBTEST: render-full-compressed
 * Description: Copy surface using 3d engine (1:1) when intermediate surface
 *              is compressed
 */
#define WIDTH	256
#define HEIGHT	256
IGT_TEST_DESCRIPTION("Exercise render-copy on xe");

static bool debug_bb;
static bool write_png;
static bool buf_info;
static uint32_t surfwidth = WIDTH;
static uint32_t surfheight = HEIGHT;

static void scratch_buf_init(struct buf_ops *bops,
			     struct intel_buf *buf,
			     int width, int height,
			     uint32_t req_tiling,
			     enum i915_compression compression)
{
	int fd = buf_ops_get_fd(bops);
	int bpp = 32;
	uint64_t region = system_memory(fd);

	if (compression && xe_has_vram(fd))
		region = vram_memory(fd, 0);

	intel_buf_init_in_region(bops, buf, width, height, bpp, 0,
				 req_tiling, compression, region);

	igt_assert(intel_buf_width(buf) == width);
	igt_assert(intel_buf_height(buf) == height);
}

#define GROUP_SIZE 4096
static int compare_detail(const uint32_t *ptr1, uint32_t *ptr2,
			  uint32_t size)
{
	int i, ok = 0, fail = 0;
	int groups = size / GROUP_SIZE;
	int *hist = calloc(GROUP_SIZE, groups);

	igt_debug("size: %d, group_size: %d, groups: %d\n",
		  size, GROUP_SIZE, groups);

	for (i = 0; i < size / sizeof(uint32_t); i++) {
		if (ptr1[i] == ptr2[i]) {
			ok++;
		} else {
			fail++;
			hist[i * sizeof(uint32_t) / GROUP_SIZE]++;
		}
	}

	for (i = 0; i < groups; i++) {
		if (hist[i])
			igt_debug("[group %4x]: %d\n", i, hist[i]);
	}
	free(hist);

	igt_debug("ok: %d, fail: %d\n", ok, fail);

	return fail;
}

static int compare_bufs(struct intel_buf *buf1, struct intel_buf *buf2,
			bool detail_compare)
{
	void *ptr1, *ptr2;
	int fd1, fd2, ret;

	/* Avoid comparison of buffers of different sizes */
	if (buf1->surface[0].size != buf2->surface[0].size)
		return 0;

	fd1 = buf_ops_get_fd(buf1->bops);
	fd2 = buf_ops_get_fd(buf2->bops);

	ptr1 = xe_bo_map(fd1, buf1->handle, buf1->surface[0].size);
	ptr2 = xe_bo_map(fd2, buf2->handle, buf2->surface[0].size);
	ret = memcmp(ptr1, ptr2, buf1->surface[0].size);
	if (detail_compare)
		ret = compare_detail(ptr1, ptr2, buf1->surface[0].size);

	munmap(ptr1, buf1->surface[0].size);
	munmap(ptr2, buf2->surface[0].size);

	return ret;
}

static bool buf_is_aux_compressed(struct buf_ops *bops, struct intel_buf *buf)
{
	int xe = buf_ops_get_fd(bops);
	unsigned int gen = intel_gen(buf_ops_get_devid(bops));
	uint32_t ccs_size;
	uint8_t *ptr;
	bool is_compressed = false;

	igt_assert_neq(buf->ccs[0].offset, 0);

	ccs_size = intel_buf_ccs_width(gen, buf) * intel_buf_ccs_height(gen, buf);
	ptr = xe_bo_map(xe, buf->handle, buf->size);
	for (int i = 0; i < ccs_size; i++)
		if (ptr[buf->ccs[0].offset + i] != 0) {
			is_compressed = true;
			break;
		}
	munmap(ptr, buf->size);

	return is_compressed;
}

static bool buf_is_compressed(struct buf_ops *bops, struct intel_buf *buf)
{
	struct drm_xe_engine_class_instance inst = {
		.engine_class = DRM_XE_ENGINE_CLASS_COPY,
	};
	int xe = buf_ops_get_fd(bops);
	struct blt_copy_object obj;
	uint64_t ahnd;
	uint32_t vm, exec_queue;
	uint32_t tiling = i915_tile_to_blt_tile(buf->tiling);
	uint32_t devid = buf_ops_get_devid(bops);
	intel_ctx_t *ctx;
	bool is_compressed;

	if (!HAS_FLATCCS(devid))
		return buf_is_aux_compressed(bops, buf);

	vm = xe_vm_create(xe, 0, 0);
	exec_queue = xe_exec_queue_create(xe, vm, &inst, 0);
	ctx = intel_ctx_xe(xe, vm, exec_queue, 0, 0, 0);
	ahnd = intel_allocator_open(xe, ctx->vm, INTEL_ALLOCATOR_RELOC);

	blt_set_object(&obj, buf->handle,
		       buf->size, buf->region, buf->mocs_index,
		       buf->pat_index, tiling,
		       buf->compression ? COMPRESSION_ENABLED : COMPRESSION_DISABLED,
		       COMPRESSION_TYPE_3D);
	blt_set_geom(&obj, buf->surface[0].stride, 0, 0, buf->width, buf->height, 0, 0);

	is_compressed = blt_surface_is_compressed(xe, ctx, NULL, ahnd, &obj);

	xe_exec_queue_destroy(xe, exec_queue);
	xe_vm_destroy(xe, vm);
	put_ahnd(ahnd);
	free(ctx);

	return is_compressed;
}

/*
 *
 * Scenarios implemented are presented below. We copy from linear to and forth
 * linear/tiled and back manipulating x,y coordinates from source and
 * destination.
 * For render randomize width and height and randomize x,y inside.
 *
 *  <linear>        <linear/x/y/4/64>
 *
 *  Square:
 *  +---+---+       +---+---+
 *  | 1 | 2 |  ==>  | 3 | 1 |
 *  +---+---+       +---+---+
 *  | 3 | 4 |  <==  | 4 | 2 |
 *  +---+---+       +---+---+
 *
 *  VStripes:
 *  +-+-+-+-+       +-+-+-+-+
 *  | | | | |  ==>  | | | | |
 *  |1|2|3|4|       |2|4|1|3|
 *  | | | | |  ==>  | | | | |
 *  +-+-+-+-+       +-+-+-+-+
 *
 *  HStripes:
 *  +-------+       +-------+
 *  |   1   |       |   2   |
 *  +-------+  ==>  +-------+
 *  |   2   |       |   4   |
 *  +-------+       +-------+
 *  |   3   |       |   1   |
 *  +-------+  <==  +-------+
 *  |   4   |       |   3   |
 *  +-------+       +-------+
 *
 *   Full:
 *  +-------+       +-------+
 *  |       |  ==>  |       |
 *  |   1   |       |   1   |
 *  |       |  <==  |       |
 *  +-------+       +-------+
 *
 *  Random:
 *  +-+-----+       +-+-----+
 *  |1|  2  |       |1|  2  |
 *  +-+-----+  ==>  +-+-----+
 *  |3|  4  |       |3|  4  |
 *  | |     |  <==  | |     |
 *  +-+-----+       +-+-----+
 */

enum render_copy_testtype {
	COPY_SQUARE,
	COPY_VSTRIPES,
	COPY_HSTRIPES,
	COPY_RANDOM,
	COPY_FULL,
	COPY_FULL_COMPRESSED,
};

static const char * const testname[] = {
	[COPY_SQUARE]	= "square",
	[COPY_VSTRIPES]	= "vstripes",
	[COPY_HSTRIPES]	= "hstripes",
	[COPY_RANDOM]	= "random",
	[COPY_FULL]	= "full",
	[COPY_FULL_COMPRESSED] = "full-compressed",
};

static int render(struct buf_ops *bops, uint32_t tiling,
		  uint32_t width, uint32_t height,
		  enum render_copy_testtype testtype)
{
	struct intel_bb *ibb;
	struct intel_buf src, dst, final, grfs;
	int xe = buf_ops_get_fd(bops);
	uint32_t fails = 0;
	uint32_t devid = intel_get_drm_devid(xe);
	igt_render_copyfunc_t render_copy = NULL;
	int compression = testtype == COPY_FULL_COMPRESSED ? I915_COMPRESSION_RENDER :
							     I915_COMPRESSION_NONE;
	bool is_compressed;
	struct posrc {
		uint32_t x0, y0;
		uint32_t x1, y1;
		uint32_t x2, y2;
		uint32_t x3, y3;
		uint32_t w, h;
	} xys[] = {
		/* square */
		{ .x0 = 0,		.y0 = 0,
		  .x1 = width/2,	.y1 = 0,
		  .x2 = width/2,	.y2 = height/2,
		  .x3 = 0,		.y3 = height/2,
		  .w = width/2,		.h = height/2 },

		/* vstripes */
		{ .x0 = 0,
		  .x1 = width/2,
		  .x2 = width/2 + width/4,
		  .x3 = width/4,
		  .w = width/4,		.h = height },

		/* hstripes */
		{ .y0 = 0,
		  .y1 = height/2,
		  .y2 = height/2 + height/4,
		  .y3 = height/4,
		  .w = width,		.h = height/4 },

		/* random - filled later */
		{ 0, }
	}, *p;

	if (testtype == COPY_RANDOM) {
		width = rand() % width + 1;
		height = rand() % height + 1;
	}

	ibb = intel_bb_create(xe, SZ_4K);

	if (debug_bb)
		intel_bb_set_debug(ibb, true);

	scratch_buf_init(bops, &src, width, height, I915_TILING_NONE,
			 I915_COMPRESSION_NONE);
	scratch_buf_init(bops, &dst, width, height, tiling,
			 compression);
	scratch_buf_init(bops, &final, width, height, I915_TILING_NONE,
			 I915_COMPRESSION_NONE);
	scratch_buf_init(bops, &grfs, 64, height * 4, I915_TILING_NONE,
			 I915_COMPRESSION_NONE);

	intel_buf_draw_pattern(bops, &src,
			       0, 0, width, height,
			       0, 0, width, height, 0);

	render_copy = igt_get_render_copyfunc(devid);
	igt_assert(render_copy);

	switch (testtype) {
	case COPY_SQUARE:
	case COPY_VSTRIPES:
	case COPY_HSTRIPES:
		p = &xys[testtype];

		/* copy to intermediate surface (dst) */
		render_copy(ibb,
			    &src, p->x0, p->y0, p->w, p->h,
			    &dst, p->x1, p->y1);
		render_copy(ibb,
			    &src, p->x1, p->y1, p->w, p->h,
			    &dst, p->x2, p->y2);
		render_copy(ibb,
			    &src, p->x2, p->y2, p->w, p->h,
			    &dst, p->x3, p->y3);
		render_copy(ibb,
			    &src, p->x3, p->y3, p->w, p->h,
			    &dst, p->x0, p->y0);

		/* copy to final */
		render_copy(ibb,
			    &dst, p->x0, p->y0, p->w, p->h,
			    &final, p->x3, p->y3);
		render_copy(ibb,
			    &dst, p->x1, p->y1, p->w, p->h,
			    &final, p->x0, p->y0);
		render_copy(ibb,
			    &dst, p->x2, p->y2, p->w, p->h,
			    &final, p->x1, p->y1);
		render_copy(ibb,
			    &dst, p->x3, p->y3, p->w, p->h,
			    &final, p->x2, p->y2);
		break;

	case COPY_RANDOM:
		p = &xys[testtype];
		p->x0 = rand() % width;
		p->y0 = rand() % height;
		igt_debug("Random <width: %u, height: %u, x0: %d, y0: %d>\n",
			  width, height, p->x0, p->y0);

		/* copy to intermediate surface (dst), split is randomized */
		render_copy(ibb,
			    &src, 0, 0, p->x0, p->y0,
			    &dst, 0, 0);
		render_copy(ibb,
			    &src, p->x0, 0, width - p->x0, p->y0,
			    &dst, p->x0, 0);
		render_copy(ibb,
			    &src, 0, p->y0, p->x0, height - p->y0,
			    &dst, 0, p->y0);
		render_copy(ibb,
			    &src, p->x0, p->y0, width - p->x0, height - p->y0,
			    &dst, p->x0, p->y0);

		render_copy(ibb,
			    &dst, 0, 0, width, height,
			    &final, 0, 0);
		break;


	case COPY_FULL:
	case COPY_FULL_COMPRESSED:
		render_copy(ibb,
			    &src, 0, 0, width, height,
			    &dst, 0, 0);

		render_copy(ibb,
			    &dst, 0, 0, width, height,
			    &final, 0, 0);
		break;
	}

	intel_bb_sync(ibb);
	intel_bb_destroy(ibb);

	if (write_png) {
		intel_buf_raw_write_to_png(&src, "render_src_tiling_%d_%dx%d.png",
					   tiling, width, height);
		intel_buf_raw_write_to_png(&dst, "render_dst_tiling_%d_%dx%d.png",
					   tiling, width, height);
		intel_buf_raw_write_to_png(&final, "render_final_tiling_%d_%dx%d.png",
					   tiling, width, height);
	}

	fails = compare_bufs(&src, &final, false);
	if (compression == I915_COMPRESSION_RENDER)
		is_compressed = buf_is_compressed(bops, &dst);

	intel_buf_close(bops, &src);
	intel_buf_close(bops, &dst);
	intel_buf_close(bops, &final);

	igt_assert_f(fails == 0, "%s: (tiling: %d) fails: %d\n",
		     __func__, tiling, fails);
	if (compression == I915_COMPRESSION_RENDER && blt_platform_has_flat_ccs_enabled(xe))
		igt_assert_f(is_compressed, "%s: (tiling: %d) buffer is not compressed\n",
			     __func__, tiling);

	return fails;
}

static int opt_handler(int opt, int opt_index, void *data)
{
	switch (opt) {
	case 'd':
		debug_bb = true;
		break;
	case 'p':
		write_png = true;
		break;
	case 'i':
		buf_info = true;
		break;
	case 'W':
		surfwidth = atoi(optarg);
		break;
	case 'H':
		surfheight = atoi(optarg);
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str =
	"  -d\tDebug bb\n"
	"  -p\tWrite surfaces to png\n"
	"  -i\tPrint buffer info\n"
	"  -W\tWidth (default 256)\n"
	"  -H\tHeight (default 256)"
	;


igt_main_args("dpiW:H:", NULL, help_str, opt_handler, NULL)
{
	int xe;
	struct buf_ops *bops;
	const char *tiling_name;
	int tiling;

	igt_fixture {
		xe = drm_open_driver(DRIVER_XE);
		bops = buf_ops_create(xe);
		srand(time(NULL));
	}

	for (int id = 0; id <= COPY_FULL_COMPRESSED; id++) {
		igt_subtest_with_dynamic_f("render-%s", testname[id]) {
			igt_require(xe_has_engine_class(xe, DRM_XE_ENGINE_CLASS_RENDER));

			for_each_tiling(tiling) {
				if (!render_supports_tiling(xe, tiling,
							    id == COPY_FULL_COMPRESSED))
					continue;

				tiling_name = blt_tiling_name(tiling);
				tiling = blt_tile_to_i915_tile(tiling);
				igt_dynamic_f("render-%s-%ux%u", tiling_name, surfwidth, surfheight)
					render(bops, tiling, surfwidth, surfheight, id);
			}
		}
	}

	igt_fixture {
		buf_ops_destroy(bops);
		drm_close_driver(xe);
	}
}
