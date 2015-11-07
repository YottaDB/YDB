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

#include <rms.h>
#include <ssdef.h>
#include "stringpool.h"
#include "op.h"

#define MAX_STRM_CT	256

typedef struct fnzsearch
{	short	index;
	struct FAB fab;
	struct NAM nam;
	struct fnzsearch *next;
} search_struct;

#define SEA_SIZE SIZEOF(search_struct)

static search_struct *fab_sea;
static bool search_init = FALSE;

GBLREF spdesc stringpool;

error_def(ERR_ZFILENMTOOLONG);
error_def(ERR_ZSRCHSTRMCT);

int op_fnzsearch(mval *file,mint strm,mval *ret)
{
	search_struct	*sea_ptr,*sea,*ptr;
	unsigned char	esa[MAX_FN_LEN];
	uint4		status;
	int		retlen;
	short		index, ct;

	if (!search_init)
	{
		fab_sea = malloc(SEA_SIZE);
		fab_sea->index = 0;
		fab_sea->fab = cc$rms_fab;
		fab_sea->nam = cc$rms_nam;
		fab_sea->fab.fab$l_nam = &(fab_sea->nam);
		fab_sea->fab.fab$l_fop = FAB$M_NAM;
		fab_sea->fab.fab$l_fna = malloc(MAX_FN_LEN);
		fab_sea->nam.nam$l_esa = malloc(MAX_FN_LEN);
		fab_sea->nam.nam$b_ess = MAX_FN_LEN;
		fab_sea->nam.nam$l_rsa = malloc(MAX_FN_LEN);
		fab_sea->nam.nam$b_rss = MAX_FN_LEN;
		fab_sea->fab.fab$b_fns = 0;
		fab_sea->next = 0;
		search_init = TRUE;
	}
	assert(fab_sea != 0);
	index = (short)strm;
	if (index > MAX_STRM_CT || index < 0)
		rts_error(VARLSTCNT(1) ERR_ZSRCHSTRMCT);
	MV_FORCE_STR(file);
	if (file->str.len > MAX_FN_LEN)
		rts_error(VARLSTCNT(4) ERR_ZFILENMTOOLONG,2,file->str.len,file->str.addr);
	sea_ptr = fab_sea;
	while(sea_ptr->next != 0 && sea_ptr->next->index <= index)
		sea_ptr = sea_ptr->next;
	if (sea_ptr->index != index)
	{
		sea = malloc(SEA_SIZE);
		sea->index = index;
		sea->fab = cc$rms_fab;
		sea->nam = cc$rms_nam;
		sea->fab.fab$l_nam = &(sea->nam);
		sea->fab.fab$l_fop = FAB$M_NAM;
		sea->fab.fab$l_fna = malloc(file->str.len);
		sea->fab.fab$b_fns = file->str.len;
		sea->nam.nam$l_esa = malloc(MAX_FN_LEN);
		sea->nam.nam$b_ess = MAX_FN_LEN;
		sea->nam.nam$l_rsa = malloc(MAX_FN_LEN);
		sea->nam.nam$b_rss = MAX_FN_LEN;
		sea->next = sea_ptr->next;
		sea_ptr->next = sea;
		sea_ptr =sea_ptr->next;
		memcpy(sea_ptr->fab.fab$l_fna,file->str.addr,file->str.len);
		if ((status = sys$parse(&(sea_ptr->fab),0,0)) != RMS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	} else
	{
		if (file->str.len > sea_ptr->fab.fab$b_fns)
		{
			free (sea_ptr->fab.fab$l_fna);
			sea_ptr->fab.fab$l_fna = malloc(file->str.len);
		}
		if (file->str.len != sea_ptr->fab.fab$b_fns || memcmp(sea_ptr->fab.fab$l_fna,file->str.addr,file->str.len))
		{
			memcpy(sea_ptr->fab.fab$l_fna,file->str.addr,file->str.len);
			sea_ptr->fab.fab$b_fns = file->str.len;
			if ((status = sys$parse(&(sea_ptr->fab),0,0)) != RMS$_NORMAL)
				rts_error(VARLSTCNT(1) status);
		}
	}
	status = sys$search(&(sea_ptr->fab), 0, 0);
	switch(status)
	{
		case RMS$_NORMAL:
			assert(stringpool.free >= stringpool.base);
			assert(stringpool.top >= stringpool.free);
			retlen = sea_ptr->nam.nam$b_rsl;
			ENSURE_STP_FREE_SPACE(retlen);
			ret->str.len = retlen;
			ret->str.addr = stringpool.free;
			memcpy(ret->str.addr,sea_ptr->nam.nam$l_rsa,ret->str.len);
			stringpool.free += ret->str.len;
			assert(stringpool.free >= stringpool.base);
			assert(stringpool.top >= stringpool.free);
			break;
		case RMS$_NMF:
		case RMS$_FNF:
			ret->str.len = 0;
			if (sea_ptr->index != 0)
			{	ptr = fab_sea;
				while(ptr->next->index < sea_ptr->index)
					ptr = ptr->next;
				ptr->next = sea_ptr->next;
				free(sea_ptr->nam.nam$l_esa);
				free(sea_ptr->nam.nam$l_rsa);
				free(sea_ptr->fab.fab$l_fna);
				free(sea_ptr);
			} else
				sea_ptr->fab.fab$b_fns = 0;
			break;
		default:
			rts_error(VARLSTCNT(1)  status );
	}
	ret->mvtype = MV_STR;
	return 0; /* dummy for compatibility with unix prototype */
}
