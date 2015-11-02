/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
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
#include "sleep_cnt.h"
#include "util.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "mu_int_wait_rdonly.h"

#define	 NO_WRITE_ACCESS_ERR_STRING	"Database requires flushing, which can't be performed without write access"
/* If a database is read-only then, wcs_flu cannot be called as the database is read-only. In such cases, we might
 * want to wait for sometime for all the active writers to empty their write queues.
 */
boolean_t	mu_int_wait_rdonly(sgmnt_addrs *csa, char *waiting_process)
{
	int				lcnt;
	gd_region			*reg;
	cache_que_head_ptr_t		crq;
	VMS_ONLY(
		cache_que_head_ptr_t	crqwip;
	)
	boolean_t			ok, was_crit;

	error_def(ERR_BUFFLUFAILED);
	error_def(ERR_DBRDONLY);
	error_def(ERR_TEXT);

	reg = csa->region;
	crq = &csa->acc_meth.bg.cache_state->cacheq_active;
	VMS_ONLY(crqwip = &csa->acc_meth.bg.cache_state->cacheq_wip;)
	assert(reg->read_only);
	for (lcnt = 1; (lcnt <= BUF_OWNER_STUCK) && ((0 != crq->fl) VMS_ONLY(|| (0 != crqwip->fl)));  lcnt++)
	{
#		ifdef VMS
		if (0 != crqwip->fl)
		{
			was_crit = csa->now_crit;
			if (!was_crit)
				grab_crit(reg);
			ok = wcs_wtfini(reg);
			if (!was_crit)
				rel_crit(reg);
			if (!ok)
			{
				gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_STR(waiting_process), DB_LEN_STR(reg));
				return FALSE;
			}
		}
#		endif
		assert(!csa->now_crit); /* We better not hold crit before we start sleeping */
		if (0 != crq->fl)
			wcs_sleep(lcnt);
	}
	if (0 != crq->fl VMS_ONLY(|| 0 != crqwip->fl))
	{	/* Cannot proceed for read-only data files */
		gtm_putmsg(VARLSTCNT(8) ERR_DBRDONLY, 2, DB_LEN_STR(reg), ERR_TEXT, 2, LEN_AND_LIT(NO_WRITE_ACCESS_ERR_STRING));
		return FALSE;
	}
	return TRUE;
}
