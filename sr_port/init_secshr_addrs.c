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

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "init_secshr_addrs.h"

GBLREF gd_addr_fn_ptr	get_next_gdr_addrs;
GBLREF cw_set_element	*cw_set_addrs;
GBLREF sgm_info		**first_sgm_info_addrs;
GBLREF unsigned char	*cw_depth_addrs;
GBLREF uint4		rundown_process_id;
GBLREF uint4		rundown_image_count;
GBLREF int4		rundown_os_page_size;
GBLREF gd_region	**jnlpool_reg_addrs;

#define DEF_PGSZ	512

void init_secshr_addrs(gd_addr_fn_ptr getnxtgdr, cw_set_element *cwsetaddrs, sgm_info **firstsiaddrs,
	unsigned char *cwsetdepthaddrs, uint4 epid, uint4 icnt, int4 gtmospagesize, gd_region **jpool_reg_address)
{
	get_next_gdr_addrs = getnxtgdr;
	cw_set_addrs = cwsetaddrs;
	first_sgm_info_addrs = firstsiaddrs;
	cw_depth_addrs = cwsetdepthaddrs;
	rundown_process_id = epid;
	rundown_image_count = icnt;
	rundown_os_page_size = ((0 != gtmospagesize) && ((gtmospagesize / DEF_PGSZ) * DEF_PGSZ) == gtmospagesize) ? gtmospagesize
														  : DEF_PGSZ;
	jnlpool_reg_addrs = jpool_reg_address;
}
