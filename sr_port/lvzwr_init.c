/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "zwrite.h"
#include "subscript.h"
#include "mlkdef.h"
#include "zshow.h"
#include "alias.h"

GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF int		merge_args;
GBLREF symval		*curr_symval;
GBLREF uint4		zwrtacindx;


void lvzwr_init(enum zwr_init_types zwrpattyp, mval *val)
{
	lvzwrite_datablk	*prevzwrb;

	/* Standard call at start of zwrite type functions. If this symval has aliases in it,
	 * prep a hash table we will use to track the lv_val addrs we process (but only if not merging).
	 */
	if (!merge_args)
	{	/* Re-initialize table even if no "aliases" defined since dotted parms are actually aliases too and
		 * will be placed in this table by lvzwr_out().
		 */
		als_zwrhtab_init();
		zwrtacindx = 0;
	}
	if (!lvzwrite_block)
	{
		lvzwrite_block = (lvzwrite_datablk *)malloc(SIZEOF(lvzwrite_datablk));
		memset(lvzwrite_block, 0, SIZEOF(lvzwrite_datablk));
	} else
	{	/* Get back to one zwrite_block if multiples were stacked (and left over) */
		for (prevzwrb = lvzwrite_block->prev; prevzwrb; lvzwrite_block = prevzwrb, prevzwrb = lvzwrite_block->prev)
		{
			if (lvzwrite_block->sub)
				free(lvzwrite_block->sub);
			free(lvzwrite_block);
		}
	}
	lvzwrite_block->zwr_intype = zwrpattyp;
	if (!merge_args && val)
	{	/* val may be null when called from gtm_startup/gtm$startup */
		MV_FORCE_STR(val);
		lvzwrite_block->pat = val;
	} else
		lvzwrite_block->pat = NULL;
	lvzwrite_block->mask = lvzwrite_block->subsc_count = 0;
	if (!lvzwrite_block->sub)
		lvzwrite_block->sub = (zwr_sub_lst *)malloc(SIZEOF(zwr_sub_lst) * MAX_LVSUBSCRIPTS);
	lvzwrite_block->fixed = TRUE;
	return;
}
