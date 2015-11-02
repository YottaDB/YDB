/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "jnl.h"		/* needed for WCSFLU_* macros */
#include "sleep_cnt.h"
#include "util.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "change_reg.h"
#include "mupint.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "interlock.h"

GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_data		mu_int_data;
GBLREF unsigned char		*mu_int_master;
GBLREF uint4			mu_int_errknt;
GBLREF sgmnt_data_ptr_t		cs_data;

void mu_int_reg(gd_region *reg, boolean_t *return_value)
{
	int			lcnt;
	sgmnt_addrs     	*csa;
	cache_que_head_ptr_t    crq, crqwip;
	freeze_status		status;

	error_def(ERR_BUFFLUFAILED);
	error_def(ERR_DBRDONLY);
	error_def(ERR_MUKILLIP);
	*return_value = FALSE;

	ESTABLISH(mu_int_reg_ch);
	if (dba_usr == reg->dyn.addr->acc_meth)
	{
		util_out_print("!/Can't integ region !AD; not GDS format", TRUE,  REG_LEN_STR(reg));
		mu_int_errknt++;
		return;
	}
	gv_cur_region = reg;
	if (reg_cmcheck(reg))
	{
		util_out_print("!/Can't integ region across network", TRUE);
		mu_int_errknt++;
		return;
	}
	gvcst_init(gv_cur_region);
	if (gv_cur_region->was_open)
	{	/* already open under another name */
		gv_cur_region->open = FALSE;
		return;
	}
	change_reg();

	if ((gv_cur_region->read_only) && (dba_mm == cs_data->acc_meth))
	{
		util_out_print("!/MM database is read only. MM database cannot be frozen without write access.", TRUE);
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		mu_int_errknt++;
		return;
	}
	status = region_freeze(gv_cur_region, TRUE, FALSE, TRUE);
	switch (status)
	{
		case REG_ALREADY_FROZEN:
			util_out_print("!/Database for region !AD is already frozen, not integing",
				TRUE, REG_LEN_STR(gv_cur_region));
			mu_int_errknt++;
			return;
		case REG_HAS_KIP:
			/* We have already waited for KIP to reset. This time do not wait for KIP */
			status = region_freeze(gv_cur_region, TRUE, FALSE, FALSE);
			if (REG_ALREADY_FROZEN == status)
			{
				util_out_print("!/Database for region !AD is already frozen, not integing",
					TRUE, REG_LEN_STR(gv_cur_region));
				mu_int_errknt++;
				return;
			}
			break;
		case REG_FREEZE_SUCCESS:
			break;
		default:
			assert(FALSE);
	}
	if (gv_cur_region->read_only)
	{
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		crq = &csa->acc_meth.bg.cache_state->cacheq_active;
		crqwip = &csa->acc_meth.bg.cache_state->cacheq_wip;
		for (lcnt=1;  (lcnt <= BUF_OWNER_STUCK) && ((0 != crq->fl) || (0 != crqwip->fl));  lcnt++)
		{
#ifdef VMS
			if (0 != crqwip->fl)
			{
				bool	ok;

				grab_crit(gv_cur_region);
				ok = wcs_wtfini(gv_cur_region);
				rel_crit(gv_cur_region);
				if (!ok)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("INTEG"),
						DB_LEN_STR(gv_cur_region));
					mu_int_errknt++;
					return;
				}
			}
#endif
			if (0 != crq->fl)
				wcs_sleep(lcnt);
		}
		if (0 != crq->fl || 0 != crqwip->fl)
		{
			/* Cannot proceed for read-only data files */
			util_out_print("!/Database requires flushing, which can't be performed without write access",TRUE);
			gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
			mu_int_errknt++;
			return;
		}
	} else  if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH))
	{
		gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("INTEG"), DB_LEN_STR(gv_cur_region));
		mu_int_errknt++;
		return;
	}
	/* Take a copy of the file-header. To ensure it is consistent, do it while holding crit. */
	grab_crit(gv_cur_region);
	memcpy((uchar_ptr_t)&mu_int_data, (uchar_ptr_t)cs_data, sizeof(sgmnt_data));
	rel_crit(gv_cur_region);
	mu_int_master = malloc(MASTER_MAP_SIZE(cs_data));
	memcpy(mu_int_master, MM_ADDR(cs_data), MASTER_MAP_SIZE(cs_data));
	*return_value = mu_int_fhead();
	REVERT;
	return;
}
