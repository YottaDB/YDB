/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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

#define	DUMMY_GLD_ADD_MAP(MAP, MAPKEY_PTR, REG, WHICH_MAP)									\
MBSTART { 															\
	size_t map_size = DUMMY_GBLDIR_SIZE - sizeof(gd_addr);	/* Calculated from malloc for "addr" + "map" init below */	\
																\
	assert((MAPKEY_PTR - (char *) MAP + (sizeof(WHICH_MAP) - 1)) <= map_size);						\
	MAP->gvkey.addr = MAPKEY_PTR;												\
	MEMCPY_LIT(MAPKEY_PTR, WHICH_MAP);											\
	MAP->reg.addr = REG;													\
	MAP->old_reg.addr = NULL;												\
	MAP->gvname_len = DUMMY_GBLDIR_MAP_GVN_SIZE(WHICH_MAP);									\
	MAP->gvkey_len =  DUMMY_GBLDIR_MAP_KEY_SIZE(WHICH_MAP) - 1;								\
	MAPKEY_PTR += MAP->gvkey_len;												\
	MAP++;															\
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
	GTM64_ONLY(assert(!MEMCMP_LIT(GDE_LABEL_LITERAL, "GTCGBDUNX117"));)
	NON_GTM64_ONLY(assert(!MEMCMP_LIT(GDE_LABEL_LITERAL, "GTCGBDUNX017"));)
	addr = (gd_addr *)malloc(DUMMY_GBLDIR_SIZE);
	assert(NULL != addr);
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

/* This function is a helper function to the database upgrade processing. It is possible that other MUPIP processing
 * would need such functionality in the future and the original code started as an adapation of create_dummy_gbldir(),
 * above.
 *
 * The V6 to V7 upgrade processing iterates over blocks and, to generate block history, performs a reverse lookup of the
 * name found in a block. This reverse lookup MUST find the name in the current database/segment. Without this, globals
 * not mapped to the database file under processing point to a different database file tripping asserts and thwarts the
 * attempt at creating a history for the blocks in the current database.
 *
 * Input:
 * 	Current global directory
 * 	Target region to which all globals must map
 *
 * Output:
 * 	Global maps reduced to 3 entries
 * 	Global maps point everything into one region
 */
#define REMAP_GBLDIR_N_MAPS 3
void remap_globals_to_one_region(gd_addr *addr, gd_region *reg)
{
	int	i;

	/* Setup map for only 3 names (once) */
	assert(addr->n_maps >= REMAP_GBLDIR_N_MAPS);
	if (REMAP_GBLDIR_N_MAPS < addr->n_maps)
	{	/* Slam the last name entry onto all maps beyond the first two names */
		for (i = 2; i < addr->n_maps; i++)
			addr->maps[i] = addr->maps[addr->n_maps - 1];
		/* Reduce the name map to 3 (required) names */
		addr->n_maps = REMAP_GBLDIR_N_MAPS;
	} else
	{	/* Clean up the exist M-name hash table which points to the previous region */
		assert(NULL != addr->tab_ptr);
		free_hashtab_mname(addr->tab_ptr);
		free(addr->tab_ptr);
		addr->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
		init_hashtab_mname((hash_table_mname *)addr->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE );
	}
	/* Repoint all names to the target region */
	for (i = 0; i < addr->n_maps; i++)
		addr->maps[i].reg.addr = reg;

	return;
}
