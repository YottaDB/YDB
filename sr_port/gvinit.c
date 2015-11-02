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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"

GBLREF	gd_addr		*gd_header;

void gvinit(void)
{
	mval	v;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* if gd_header is null then get the current one, and update the gd_map */
	if (!gd_header)
	{
		SET_GD_HEADER(v);
		SET_GD_MAP;
	}
	DEBUG_ONLY(else GD_HEADER_ASSERT);
	/* May get in here after an extended ref call OR in mupip journal recover forward processing (with
	 * function call graph "mur_output_record/gvcst_put/gvtr_init/gvtr_db_tpwrap/op_tstart").
	 * In either case it is possible that gv_currkey has already been set up, so dont lose any preexisting keys.
	 */
	GVKEYSIZE_INCREASE_IF_NEEDED(DBKEYSIZE(gd_header->regions->max_key_size));
}
