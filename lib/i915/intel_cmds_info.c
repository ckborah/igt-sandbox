// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#include <stdint.h>
#include <stddef.h>

#include "intel_chipset.h"
#include "i915/intel_cmds_info.h"

#define BLT_INFO(_cmd, _tiling)  { \
		.blt_cmd_type = _cmd, \
		.supported_tiling = _tiling \
	}

#define BLT_INFO_EXT(_cmd, _tiling, _flags)  { \
		.blt_cmd_type = _cmd, \
		.supported_tiling = _tiling, \
		.flags = _flags, \
	}

static const struct blt_cmd_info src_copy = BLT_INFO(SRC_COPY, BIT(T_LINEAR));
static const struct blt_cmd_info
		pre_gen6_xy_src_copy = BLT_INFO(XY_SRC_COPY,
						BIT(T_LINEAR) |
						BIT(T_XMAJOR));
static const struct blt_cmd_info
		gen6_xy_src_copy = BLT_INFO(XY_SRC_COPY,
					    BIT(T_LINEAR) |
					    BIT(T_XMAJOR) |
					    BIT(T_YMAJOR));
static const struct blt_cmd_info
		gen11_xy_fast_copy = BLT_INFO(XY_FAST_COPY,
					      BIT(T_LINEAR)  |
					      BIT(T_YMAJOR)  |
					      BIT(T_YFMAJOR) |
					      BIT(T_TILE64));
static const struct blt_cmd_info
		gen12_xy_fast_copy = BLT_INFO(XY_FAST_COPY,
					      BIT(T_LINEAR) |
					      BIT(T_YMAJOR) |
					      BIT(T_TILE4)  |
					      BIT(T_TILE64));
static const struct blt_cmd_info
		dg2_xy_fast_copy = BLT_INFO(XY_FAST_COPY,
					    BIT(T_LINEAR) |
					    BIT(T_XMAJOR) |
					    BIT(T_TILE4)  |
					    BIT(T_TILE64));
static const struct blt_cmd_info
		gen12_xy_block_copy = BLT_INFO(XY_BLOCK_COPY,
					       BIT(T_LINEAR) |
					       BIT(T_YMAJOR));
static const struct blt_cmd_info
		dg2_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY,
						 BIT(T_LINEAR) |
						 BIT(T_XMAJOR) |
						 BIT(T_TILE4)  |
						 BIT(T_TILE64),
						 BLT_CMD_EXTENDED |
						 BLT_CMD_SUPPORTS_COMPRESSION);

static const struct blt_cmd_info
		mtl_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY,
						 BIT(T_LINEAR) |
						 BIT(T_XMAJOR) |
						 BIT(T_TILE4)  |
						 BIT(T_TILE64),
						 BLT_CMD_EXTENDED);

const struct intel_cmds_info pre_gen6_cmds_info = {
	.blt_cmds = {
		[SRC_COPY] = &src_copy,
		[XY_SRC_COPY] = &pre_gen6_xy_src_copy
	}
};

const struct intel_cmds_info gen6_cmds_info =  {
	.blt_cmds = {
		[SRC_COPY] = &src_copy,
		[XY_SRC_COPY] = &gen6_xy_src_copy
	}

};

const struct intel_cmds_info gen8_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
	}
};

const struct intel_cmds_info gen11_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &gen11_xy_fast_copy,
	}
};

const struct intel_cmds_info gen12_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &gen12_xy_fast_copy,
		[XY_BLOCK_COPY] = &gen12_xy_block_copy,
	}
};

const struct intel_cmds_info gen12_dg2_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &dg2_xy_fast_copy,
		[XY_BLOCK_COPY] = &dg2_xy_block_copy,
	}
};

const struct intel_cmds_info gen12_mtl_cmds_info = {
	.blt_cmds = {
		[XY_FAST_COPY] = &dg2_xy_fast_copy,
		[XY_BLOCK_COPY] = &mtl_xy_block_copy,
	}
};

const struct blt_cmd_info *blt_get_cmd_info(const struct intel_cmds_info *cmds_info,
					    enum blt_cmd_type cmd)
{
	if (!cmds_info)
		return NULL;

	return cmds_info->blt_cmds[cmd];
}
