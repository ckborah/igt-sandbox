#ifndef GEN9_RENDER_H
#define GEN9_RENDER_H

#include "gen8_render.h"

#define GEN9_3DSTATE_COMPONENT_PACKING		GEN4_3D(3, 0, 0x55)

#define GEN9_SBE_ACTIVE_COMPONENT_NONE		0
#define GEN9_SBE_ACTIVE_COMPONENT_XY		1
#define GEN9_SBE_ACTIVE_COMPONENT_XYZ		2
#define GEN9_SBE_ACTIVE_COMPONENT_XYZW		3

#define GEN9_PIPELINE_SELECTION_MASK		(3 << 8)
#define GEN9_PIPELINE_SELECT			(GEN4_3D(1, 1, 4) | (3 << 8))

#define GEN9_3DSTATE_MULTISAMPLE_NUMSAMPLES_16	(4 << 1)

/* Shamelessly ripped from mesa */
struct gen9_surface_state {
	struct {
		uint32_t cube_pos_z:1;
		uint32_t cube_neg_z:1;
		uint32_t cube_pos_y:1;
		uint32_t cube_neg_y:1;
		uint32_t cube_pos_x:1;
		uint32_t cube_neg_x:1;
		uint32_t media_boundary_pixel_mode:2;
		uint32_t render_cache_read_write:1;
		uint32_t smapler_l2_bypass:1;
		uint32_t vert_line_stride_ofs:1;
		uint32_t vert_line_stride:1;
		uint32_t tiled_mode:2;
		uint32_t horizontal_alignment:2;
		uint32_t vertical_alignment:2;
		uint32_t surface_format:9;     /**< BRW_SURFACEFORMAT_x */
		uint32_t pad0:1;
		uint32_t is_array:1;
		uint32_t surface_type:3;       /**< BRW_SURFACE_1D/2D/3D/CUBE */
	} ss0;

	struct {
		uint32_t qpitch:15;
		uint32_t pad1:4;
		uint32_t base_mip_level:5;
		uint32_t pxp:1;
		uint32_t mocs_index:6;
		uint32_t pad0:1;
	} ss1;

	struct {
		uint32_t width:14;
		uint32_t pad1:2;
		uint32_t height:14;
		uint32_t pad0:2;
	} ss2;

	struct {
		uint32_t pitch:18;
		uint32_t pad:3;
		uint32_t depth:11;
	} ss3;

	union {
		struct {
			uint32_t minimum_array_element:27;
			uint32_t pad0:5;
		} skl;
		struct {
			uint32_t decompress_in_l3:1;
			uint32_t pad0:31;
		} dg2;
	} ss4;

	struct {
		uint32_t mip_count:4;
		uint32_t min_lod:4;
		uint32_t mip_tail_start_lod:4;
		uint32_t pad3:2;
		uint32_t coherency_type:1;
		uint32_t pad2:3;
		uint32_t trmode:2;
		uint32_t ewa_disable_for_cube:1;
		uint32_t y_offset:3;
		uint32_t pad0:1;
		uint32_t x_offset:7;
	} ss5;

	struct {
		uint32_t aux_mode:3;
		uint32_t aux_pitch:9;
		uint32_t pad0:4;
		uint32_t aux_qpitch:15;
		uint32_t pad1:1;
	} ss6;

	union {
		struct {
			uint32_t resource_min_lod:12;

			/* Only on Haswell */
			uint32_t pad0:4;
			uint32_t shader_chanel_select_a:3;
			uint32_t shader_chanel_select_b:3;
			uint32_t shader_chanel_select_g:3;
			uint32_t shader_chanel_select_r:3;

			uint32_t alpha_clear_color:1;
			uint32_t blue_clear_color:1;
			uint32_t green_clear_color:1;
			uint32_t red_clear_color:1;
		} skl;
		struct {
			uint32_t resource_min_lod:12;

			/* Only on Haswell */
			uint32_t pad0:4;
			uint32_t shader_chanel_select_a:3;
			uint32_t shader_chanel_select_b:3;
			uint32_t shader_chanel_select_g:3;
			uint32_t shader_chanel_select_r:3;

			uint32_t pad1:2;
			uint32_t media_compression:1;
			uint32_t pad2:1;
		} tgl;

		struct {
			uint32_t pad0:14;
			uint32_t disable_support_for_multi_gpu_partial_writes:1;
			uint32_t disable_support_for_multi_gpu_atomics:1;
			uint32_t pad1:14;
			uint32_t memory_compression_enable:1;
			uint32_t memory_compression_type:1;
		} dg2;
	} ss7;

	struct {
		uint32_t base_addr;
	} ss8;

	struct {
		uint32_t base_addr_hi;
	} ss9;

	struct {
		uint32_t quilt_width:5;
		uint32_t quilt_height:5;
		uint32_t clearvalue_addr_enable:1;
		uint32_t procedual_texture:1; /* Only on TGL+ */
		uint32_t aux_base_addr:20;
	} ss10;

	struct {
		uint32_t aux_base_addr_hi;
	} ss11;

	union {
		struct {
			/*
			 * compression_format is used only dg2 onward.
			 * prior to dg2 full ss12 is used for the address
			 * but due to alignments bits 0..6 will be zero
			 * and asserted in code to be so
			 */
			uint32_t compression_format:5;
			uint32_t pad0:1;
			uint32_t clear_address:26;
		} dg2;

		struct {
			/*
			 * On Xe2+ compression format is 4-bit long.
			 */
			uint32_t compression_format:4;
			uint32_t mip_region_depth_in_log:4;
			uint32_t pad0:24;
		} lnl;
	} ss12;

	struct {
		uint32_t clear_address_hi:16;
		uint32_t pad0:16;

	} ss13;

	struct {
		uint32_t reserved;
	} ss14;

	struct {
		uint32_t reserved;
	} ss15;
};

#endif
