/*
 * Copyright © 2007 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "igt.h"
#include <limits.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>

/**
 * TEST: core getclient
 * Description: Tests the DRM_IOCTL_GET_CLIENT ioctl.
 * Category: Core
 * Mega feature: General Core features
 * Sub-category: DRM
 * Functionality: permission management for clients
 * Feature: core
 * Test category: GEM_Legacy
 *
 * SUBTEST:
 * Description: Tests the DRM_IOCTL_GET_CLIENT ioctl.
 */

IGT_TEST_DESCRIPTION("Tests the DRM_IOCTL_GET_CLIENT ioctl.");

igt_simple_main
{
	int fd, ret;
	drm_client_t client;

	fd = drm_open_driver(DRIVER_ANY);

	/* Look for client index 0.  This should exist whether we're operating
	 * on an otherwise unused drm device, or the X Server is running on
	 * the device.
	 */
	client.idx = 0;
	ret = ioctl(fd, DRM_IOCTL_GET_CLIENT, &client);
	igt_assert(ret == 0);

	/* Look for some absurd client index and make sure it's invalid.
	 * The DRM drivers currently always return data, so the user has
	 * no real way to detect when the list has terminated.  That's bad,
	 * and this test is XFAIL as a result.
	 */
	client.idx = 0x7fffffff;
	ret = ioctl(fd, DRM_IOCTL_GET_CLIENT, &client);
	igt_assert(ret == -1 && errno == EINVAL);

	drm_close_driver(fd);
}
