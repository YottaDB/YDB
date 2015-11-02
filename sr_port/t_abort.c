/****************************************************************
 *								*
 *	Copyright 2004, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for cws_insert.h */
#include "cws_insert.h"		/* for CWS_RESET macro */
#include "gdsblkops.h"		/* for CHECK_AND_RESET_UPDATE_ARRAY macro */
#include "t_abort.h"		/* for prototype of t_abort() */

GBLREF	unsigned char	cw_set_depth;
GBLREF	unsigned int	t_tries;
GBLREF	uint4		update_trans;

void t_abort(gd_region *reg, sgmnt_addrs *csa)
{
	assert(&FILE_INFO(reg)->s_addrs == csa);
	CWS_RESET;
	/* reset update_array_ptr to update_array; do not use CHECK_AND_RESET_UPDATE_ARRAY since cw_set_depth can be non-zero */
	RESET_UPDATE_ARRAY;
	/* "secshr_db_clnup/t_commit_cleanup" assume an active non-TP transaction if cw_set_depth is non-zero or if
	 * update_trans has the UPDTRNS_TCOMMIT_STARTED_MASK bit set. Now that the transaction is aborted, reset these fields.
	 */
	cw_set_depth = 0;
	update_trans = 0;
	t_tries = 0;
	/* Do not release crit in case of
	 * 	a) MUPIP RECOVER ONLINE  OR
	 * 	b) DSE where a CRIT SEIZE had been done on this region previously
	 * csa->hold_onto_crit is TRUE in both cases.
	 */
	if (csa->now_crit && !csa->hold_onto_crit)
		rel_crit(reg);
}
