/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "init_secshr_addrs.h"

GBLREF gd_addr_fn_ptr	get_next_gdr_addrs;
GBLREF cw_set_element	*cw_set_addrs;
GBLREF sgm_info		**first_sgm_info_addrs;
GBLREF sgm_info		**first_tp_si_by_ftok_addrs;
GBLREF unsigned char	*cw_depth_addrs;
GBLREF uint4		rundown_process_id;
GBLREF uint4		rundown_image_count;
GBLREF int4		rundown_os_page_size;
GBLREF gd_region	**jnlpool_reg_addrs;
GBLREF inctn_opcode_t	*inctn_opcode_addrs;
GBLREF inctn_detail_t	*inctn_detail_addrs;
GBLREF uint4		*dollar_tlevel_addrs;
GBLREF uint4		*update_trans_addrs;
GBLREF sgmnt_addrs	**cs_addrs_addrs;
GBLREF sgmnt_addrs 	**kip_csa_addrs;
GBLREF boolean_t	*need_kip_incr_addrs;
GBLREF trans_num	*start_tn_addrs;

#define DEF_PGSZ	512

void init_secshr_addrs(gd_addr_fn_ptr getnxtgdr, cw_set_element *cwsetaddrs,
	sgm_info **firstsiaddrs, sgm_info **firstsibyftokaddrs,
	unsigned char *cwsetdepthaddrs, uint4 epid,
	uint4 icnt, int4 gtmospagesize, gd_region **jpool_reg_address,
	inctn_opcode_t *inctn_opcode_address,
	inctn_detail_t *inctn_detail_address, uint4 *dollar_tlevel_address,
	uint4 *update_trans_address, sgmnt_addrs **cs_addrs_address,
	sgmnt_addrs **kip_csa_address, boolean_t *need_kip_incr_address,
	trans_num *start_tn_address)
{
	get_next_gdr_addrs = getnxtgdr;
	cw_set_addrs = cwsetaddrs;
	first_sgm_info_addrs = firstsiaddrs;
	first_tp_si_by_ftok_addrs = firstsibyftokaddrs;
	cw_depth_addrs = cwsetdepthaddrs;
	rundown_process_id = epid;
	assert(rundown_process_id);
	rundown_image_count = icnt;
	rundown_os_page_size = ((0 != gtmospagesize) && ((gtmospagesize / DEF_PGSZ) * DEF_PGSZ) == gtmospagesize) ? gtmospagesize
														  : DEF_PGSZ;
	jnlpool_reg_addrs = jpool_reg_address;
	inctn_opcode_addrs = inctn_opcode_address;
	inctn_detail_addrs = inctn_detail_address;
	dollar_tlevel_addrs = dollar_tlevel_address;
	update_trans_addrs = update_trans_address;
	cs_addrs_addrs = cs_addrs_address;
	kip_csa_addrs = kip_csa_address;
	need_kip_incr_addrs = need_kip_incr_address;
	start_tn_addrs = start_tn_address;
}
