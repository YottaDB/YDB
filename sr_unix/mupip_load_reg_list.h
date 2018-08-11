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
#ifndef MUPIP_LOAD_REG_LIST_INCLUDED
#define MUPIP_LOAD_REG_LIST_INCLUDED


void insert_reg_to_list(gd_region **reg_list, gd_region *r_ptr, uint4 *num_of_reg);
boolean_t search_reg_list(gd_region **reg_list, gd_region *r_ptr, uint4 num_of_reg);
boolean_t check_db_status_for_global(mname_entry *gvname, int fmt, gtm_uint64_t *failed_record_count, gtm_uint64_t iter,
		gtm_uint64_t *first_failed_rec_count, gd_region **reg_list, uint4 num_of_reg);

#endif /* MUPIP_LOAD_REG_LIST_INCLUDED */
