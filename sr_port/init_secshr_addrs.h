/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INIT_SECSHR_ADDRS_INCLUDED
#define INIT_SECSHR_ADDRS_INCLUDED

void init_secshr_addrs(gd_addr_fn_ptr getnxtgdr, cw_set_element *cwsetaddrs,
		       sgm_info **firstsiaddrs, unsigned char *cwsetdepthaddrs, uint4 epid,
		       uint4 icnt, int4 gtmospagesize, gd_region **jpool_reg_address);

#endif /* INIT_SECSHR_ADDRS_INCLUDED */
