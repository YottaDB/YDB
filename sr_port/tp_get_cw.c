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

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"

GBLREF sgm_info		*sgm_info_ptr;

void tp_get_cw (cw_set_element *cs, int depth, cw_set_element **cs1)
{
	cw_set_element *cs_tmp;			/* to avoid double dereferencing in the TRAVERSE macro */
	assert (depth < sgm_info_ptr->cw_set_depth);
	cs_tmp = (cw_set_element *)find_element(sgm_info_ptr->cw_set_list, depth);

	/* Above returns the first cse (least t_level) in the horizontal list.
	 * Traverse the horizontal list to go to the latest -
	 * since the usual transaction depth is not much (on an average 2), this does
	 * not hamper performance so much to necessiate maintaining links to the head
	 * and tail of horizontal list of cw_set_elements
	 */

	assert(cs_tmp);
	TRAVERSE_TO_LATEST_CSE(cs_tmp);
	*cs1 = cs_tmp;
}
