/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "hashtab_mname.h"
#include "hashtab.h"
#include "dpgbldir.h"

#define	DUMMY_GLD_ADD_MAP(MAP, MAPKEY_PTR, REG, WHICH_MAP)					\
MBSTART {											\
	MAP->gvkey.addr = MAPKEY_PTR;								\
	MEMCPY_LIT(MAPKEY_PTR, WHICH_MAP);							\
	MAP->reg.addr = REG;									\
	MAP->gvname_len = DUMMY_GBLDIR_MAP_GVN_SIZE(WHICH_MAP);					\
	MAP->gvkey_len =  DUMMY_GBLDIR_MAP_KEY_SIZE(WHICH_MAP) - 1;				\
	MAPKEY_PTR += MAP->gvkey_len;								\
	MAP++;											\
} MBEND

#define	GLD_REG_INIT(REG, RNAME, ADDR)		\
MBSTART {					\
	REG->rname_len = STR_LIT_LEN(RNAME);	\
	MEMCPY_LIT(REG->rname, RNAME);		\
	REG->owning_gd = ADDR;			\
} MBEND

/* C function to create a dummy gld structure in memory. This code is very similar to M code in GDE
 * so the two need to be maintained in parallel. Currently the only caller of this function is "mu_gv_cur_reg_init"
 * and so the below code is simplified to work only with that caller. As more callers need this functionality, the
 * below code can be enhanced to be more generalized.
 */
gd_addr *create_dummy_gbldir(void)
{
	gd_addr		*addr;
	gd_region	*basedb_reg, *statsdb_reg;
	gd_segment	*basedb_seg, *statsdb_seg;
	gd_binding	*map;
	char		*mapkey_ptr;
#	ifdef DEBUG
	gd_binding	*map_top;
#	endif

	/* The below code might need corresponding changes if ever the gld format changes hence the GDE_LABEL_LITERAL assert */
	GTM64_ONLY(assert(!MEMCMP_LIT(GDE_LABEL_LITERAL, "GTCGBDUNX112"));)
	NON_GTM64_ONLY(assert(!MEMCMP_LIT(GDE_LABEL_LITERAL, "GTCGBDUNX012"));)
	addr = (gd_addr *)malloc(DUMMY_GBLDIR_SIZE);
	memset(addr, 0, DUMMY_GBLDIR_SIZE);
	addr->max_rec_size = 256;
	addr->maps = (gd_binding *)((UINTPTR_T)addr + SIZEOF(gd_addr));
	addr->var_maps_len = DUMMY_GBLDIR_VAR_MAP_SIZE;
	addr->n_maps = DUMMY_GBLDIR_N_MAPS;
	addr->n_regions = DUMMY_GBLDIR_N_REGS;
	addr->n_segments = DUMMY_GBLDIR_N_REGS;
	addr->n_gblnames = 0;
	addr->link = 0;
	addr->is_dummy_gbldir = TRUE;
	addr->local_locks = 0;
	basedb_reg = (gd_region *)((char *)addr + SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE);
	statsdb_reg = basedb_reg + 1;
	addr->regions = basedb_reg;
	basedb_seg = (gd_segment *)((INTPTR_T)(basedb_reg + DUMMY_GBLDIR_N_REGS));
	statsdb_seg = basedb_seg + 1;
	addr->segments = basedb_seg;
	basedb_reg->dyn.addr = basedb_seg;
	basedb_reg->statsDB_reg_index = 1;
	basedb_reg->reservedDBFlags = RDBF_NOSTATS;	/* Keep statsdb region invisible at start by default. If opened db
							 * indicates it is enabled for statsdb, then start using statsdb reg.
							 */
	statsdb_reg->dyn.addr = statsdb_seg;
	statsdb_reg->statsDB_reg_index = 0;
	statsdb_reg->reservedDBFlags = RDBF_STATSDB_MASK;
	/* Start MAPS section initialization */
	mapkey_ptr = (char *)((UINTPTR_T)addr + SIZEOF(gd_addr) + DUMMY_GBLDIR_FIX_MAP_SIZE);
	map = (gd_binding *)((UINTPTR_T)addr + SIZEOF(gd_addr));
	DEBUG_ONLY(map_top = map + DUMMY_GBLDIR_N_MAPS);
	DUMMY_GLD_ADD_MAP(map, mapkey_ptr, basedb_reg, DUMMY_GBLDIR_FIRST_MAP);   /* add map "#)" */
	DUMMY_GLD_ADD_MAP(map, mapkey_ptr, basedb_reg, DUMMY_GBLDIR_SECOND_MAP);  /* add map "%" */
	DUMMY_GLD_ADD_MAP(map, mapkey_ptr, basedb_reg, DUMMY_GBLDIR_THIRD_MAP);   /* add map "%Y" */
	DUMMY_GLD_ADD_MAP(map, mapkey_ptr, statsdb_reg, DUMMY_GBLDIR_FOURTH_MAP); /* add map "%Z" */
	DUMMY_GLD_ADD_MAP(map, mapkey_ptr, basedb_reg, DUMMY_GBLDIR_FIFTH_MAP);   /* add map ".." */
	assert(map == map_top);
	/* MAPS sections (Fixed and Variable) initialization complete */
	basedb_seg->acc_meth = dba_bg;
	statsdb_seg->acc_meth = dba_mm;
	addr->end = (UINTPTR_T)(basedb_seg + DUMMY_GBLDIR_N_REGS);
	GLD_REG_INIT(basedb_reg, "DEFAULT", addr);	/* basedb region */
	GLD_REG_INIT(statsdb_reg, "default", addr);	/* statsdb region */
	addr->id = (gd_id *)malloc(SIZEOF(gd_id));
	memset(addr->id, 0, SIZEOF(gd_id));
	addr->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname((hash_table_mname *)addr->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE );
	return addr;
}
