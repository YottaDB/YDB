/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gvname_info.h
 * -------------
 *
 * Following structure is to save result of a call to op_gvname().
 * Specially in merge we do not need to call op_gvname again and again.
 * Just call once and save result.
 * Also note that it is not easy to call op_gvname with variable arguments again and again.
 */
#ifndef MERGE_GLOBAL_DEFINED
typedef struct gvname_info_struct {
        gv_key           *s_gv_currkey;
        gv_namehead      *s_gv_target;
        gd_region        *s_gv_cur_region;
        sgmnt_addrs      *s_cs_addrs;
	sgm_info         *s_sgm_info_ptr;
} gvname_info;
typedef gvname_info     *gvname_info_ptr;

/* Function Prototypes for M global variable functions of MERGE */
void 		gvname_env_restore(gvname_info *curr_gvname_info);
void	 	gvname_env_save(gvname_info *  curr_gvname_info);
#define MERGE_GLOBAL_DEFINED
#endif
