/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SEC_SHR_BLK_BUILD_INCLUDED
#define SEC_SHR_BLK_BUILD_INCLUDED

int sec_shr_blk_build(sgmnt_addrs *csa, sgmnt_data_ptr_t csd, boolean_t is_bg,
			cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn);

#endif /* SEC_SHR_BLK_BUILD_INCLUDED */
