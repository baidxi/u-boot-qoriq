// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 NXP
 */

#include <common.h>
#include <fsl_ddr_sdram.h>
#include <fsl_ddr_dimm_params.h>
#include "ddr.h"
#ifdef CONFIG_FSL_DEEP_SLEEP
#include <fsl_sleep.h>
#endif
#include <asm/arch/clock.h>

DECLARE_GLOBAL_DATA_PTR;

void fsl_ddr_board_options(memctl_options_t *popts,
			   dimm_params_t *pdimm,
			   unsigned int ctrl_num)
{
	const struct board_specific_parameters *pbsp, *pbsp_highest = NULL;
	ulong ddr_freq;

	if (ctrl_num > 1) {
		printf("Not supported controller number %d\n", ctrl_num);
		return;
	}
	if (!pdimm->n_ranks)
		return;

	if (popts->registered_dimm_en)
		pbsp = rdimms[0];
	else
		pbsp = udimms[0];

	/* Get clk_adjust, wrlvl_start, wrlvl_ctl, according to the board ddr
	 * freqency and n_banks specified in board_specific_parameters table.
	 */
	ddr_freq = get_ddr_freq(0) / 1000000;
	while (pbsp->datarate_mhz_high) {
		if (pbsp->n_ranks == pdimm->n_ranks) {
			if (ddr_freq <= pbsp->datarate_mhz_high) {
				popts->clk_adjust = pbsp->clk_adjust;
				popts->wrlvl_start = pbsp->wrlvl_start;
				popts->wrlvl_ctl_2 = pbsp->wrlvl_ctl_2;
				popts->wrlvl_ctl_3 = pbsp->wrlvl_ctl_3;
				goto found;
			}
			pbsp_highest = pbsp;
		}
		pbsp++;
	}

	if (pbsp_highest) {
		printf("Error: board specific timing not found for %lu MT/s\n",
		       ddr_freq);
		printf("Trying to use the highest speed (%u) parameters\n",
		       pbsp_highest->datarate_mhz_high);
		popts->clk_adjust = pbsp_highest->clk_adjust;
		popts->wrlvl_start = pbsp_highest->wrlvl_start;
		popts->wrlvl_ctl_2 = pbsp->wrlvl_ctl_2;
		popts->wrlvl_ctl_3 = pbsp->wrlvl_ctl_3;
	} else {
		panic("DIMM is not supported by this board");
	}
found:
	debug("Found timing match: n_ranks %d, data rate %d, rank_gb %d\n",
	      pbsp->n_ranks, pbsp->datarate_mhz_high, pbsp->rank_gb);

	popts->data_bus_width = 0;	/* 64-bit data bus */
	popts->bstopre = 0;		/* enable auto precharge */

	/*
	 * Factors to consider for half-strength driver enable:
	 *	- number of DIMMs installed
	 */
	popts->half_strength_driver_enable = 0;
	/*
	 * Write leveling override
	 */
	popts->wrlvl_override = 1;
	popts->wrlvl_sample = 0xf;

	/*
	 * Rtt and Rtt_WR override
	 */
	popts->rtt_override = 0;

	/* Enable ZQ calibration */
	popts->zq_en = 1;

	popts->ddr_cdr1 = DDR_CDR1_DHC_EN | DDR_CDR1_ODT(DDR_CDR_ODT_80ohm);
	popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_80ohm) |
			  DDR_CDR2_VREF_TRAIN_EN | DDR_CDR2_VREF_RANGE_2;

	/* optimize cpo for erratum A-009942 */
	popts->cpo_sample = 0x61;
}

static phys_size_t fixed_sdram(void)
{
        size_t ddr_size;

	fsl_ddr_cfg_regs_t ddr_cfg_regs = {
                .cs[0].bnds             = 0x000000ff,
                .cs[0].config           = 0x80010412,
                .cs[0].config_2         = 0,
                .cs[1].bnds             = 0,
                .cs[1].config           = 0,
                .cs[1].config_2         = 0,

                .timing_cfg_3           = 0x01111000,
                .timing_cfg_0           = 0xFA550018,
                .timing_cfg_1           = 0xBAB40C52,
                .timing_cfg_2           = 0x0048C11C,
                .ddr_sdram_cfg          = 0xC5040008,
		.ddr_sdram_cfg_2        = 0x00401010,
                .ddr_sdram_mode         = 0x01010210,
                .ddr_sdram_mode_2       = 0x0,

                //.ddr_sdram_md_cntl      = 0x0600001f,
                .ddr_sdram_interval     = 0x18600618,
                .ddr_data_init          = 0xdeadbeef,

                .ddr_sdram_clk_cntl     = 0x02000000,
                .ddr_init_addr          = 0,
                .ddr_init_ext_addr      = 0,

                .timing_cfg_4           = 0x00000002,
                .timing_cfg_5           = 0x03401400,
                .timing_cfg_6           = 0x0,
                .timing_cfg_7           = 0x23300000,

                .ddr_zq_cntl            = 0x8A090705,
                .ddr_wrlvl_cntl         = 0x86550607,
                //.ddr_sr_cntr            = 0,
                .ddr_sdram_rcw_1        = 0,
                .ddr_sdram_rcw_2        = 0,
                .ddr_wrlvl_cntl_2       = 0x07070708,
                .ddr_wrlvl_cntl_3       = 0x0808088,

                .ddr_sdram_mode_9       = 0x00000500,
                .ddr_sdram_mode_10      = 0x04000000,

                .timing_cfg_8           = 0x02116600,
		.ddr_sdram_cfg_3        = 0x00000001,

                .dq_map_0               = 0,
                .dq_map_1               = 0,
                .dq_map_2               = 0,
                .dq_map_3               = 0,

                .ddr_cdr1               = 0x80040000,
                .ddr_cdr2               = 0x000000C1
        };

	fsl_ddr_set_memctl_regs(&ddr_cfg_regs, 0, 0);
        ddr_size = 1ULL << 32;

	return ddr_size;
}

#ifdef CONFIG_TFABOOT
int fsl_initdram(void)
{
	gd->ram_size = tfa_get_dram_size();

	if (!gd->ram_size)
		gd->ram_size = fsl_ddr_sdram_size();

	return 0;
}
#else
int fsl_initdram(void)
{
	phys_size_t dram_size;

#if defined(CONFIG_SPL) && !defined(CONFIG_SPL_BUILD)
	gd->ram_size = 1ULL << 32;
	//gd->ram_size = fsl_ddr_sdram_size();

	return 0;
#else
	puts("Initializing DDR....using SPD\n");
	dram_size = fixed_sdram();
	//dram_size = fsl_ddr_sdram();
#endif

	erratum_a008850_post();

	gd->ram_size = dram_size;

	return 0;
}
#endif