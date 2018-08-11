/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "mupip_load_reg_list.h"

GBLREF gd_addr		*gd_header;
GBLREF gd_region	*db_init_region;

error_def(ERR_RECLOAD);

void insert_reg_to_list(gd_region **reg_list, gd_region *r_ptr, uint4 *num_of_reg)
{
	int	reg_iter;
	boolean_t	reg_found = FALSE;
	for (reg_iter = 0; reg_iter < gd_header->n_regions; reg_iter++)
	{
		if (reg_list[reg_iter] == r_ptr)
		{
			reg_found = TRUE;
			break;
		}
	}
	if (!reg_found)
		reg_list[(*num_of_reg)++] = r_ptr;
}

boolean_t search_reg_list(gd_region **reg_list, gd_region *r_ptr, uint4 num_of_reg)
{
	int	reg_iter;
	for (reg_iter = 0; reg_iter < gd_header->n_regions; reg_iter++)
	{
		if (reg_list[reg_iter] == r_ptr)
			return TRUE;
	}
	return FALSE;
}

boolean_t check_db_status_for_global(mname_entry *gvname, int fmt, gtm_uint64_t *failed_record_count, gtm_uint64_t iter,
		gtm_uint64_t *first_failed_rec_count, gd_region **reg_list, uint4 num_of_reg)
{
	gd_binding		*map;
	ht_ent_mname		*tabent;
	hash_table_mname	*tab_ptr;
	char			msg_buff[128];
	gd_region		*reg_ptr = NULL;
	gvnh_reg_t		*gvnh_reg;
	gtm_uint64_t		tmp_rec_count;

	tab_ptr = gd_header->tab_ptr;
	if (NULL == (tabent = lookup_hashtab_mname((hash_table_mname *)tab_ptr, gvname)))
	{
		map = gv_srch_map(gd_header, gvname->var_name.addr,
					gvname->var_name.len, SKIP_BASEDB_OPEN_TRUE);
		reg_ptr = map->reg.addr;
	} else
	{
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		assert(NULL != gvnh_reg);
		reg_ptr = gvnh_reg->gd_reg;
	}
	if ((reg_ptr == db_init_region) || search_reg_list(reg_list, reg_ptr, num_of_reg))
	{
		(*failed_record_count) += (1 + (MU_FMT_GO == fmt));
		return FALSE;
	} else
	{
		tmp_rec_count = iter - 1;
		if (tmp_rec_count > *first_failed_rec_count)
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld to %lld", *first_failed_rec_count, tmp_rec_count );
		else
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", tmp_rec_count );
		*first_failed_rec_count = 0;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
	}
	return TRUE;
}

