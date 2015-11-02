/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "nametabtyp.h"
#include "namelook.h"
#include "op.h"
#include "util.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_update_protos.h"
#include "trigger_select_protos.h"
#include "trigger_trgfile_protos.h"

GBLREF  short  	dollar_tlevel;

LITREF	mval	literal_zero;
LITREF	mval	literal_one;

enum ztrprms
{
	ZTRP_FILE,	/* $ZTRIGGER() FILE parameter - process given file of triggers */
	ZTRP_ITEM,	/* $ZTRIGGER() ITEM parameter - process single line of trigger file */
	ZTRP_SELECT	/* $ZTRIGGER() SELECT parameter - perform mupip trigger select with given parms */
};

LITDEF nametabent ztrprm_names[] =
{
	{1, "F"}, {4, "FILE"},
	{1, "I"}, {4, "ITEM"},
	{1, "S"}, {6, "SELECT"}
};
LITDEF unsigned char ztrprm_index[27] =
{
	 0,  0,  0,  0,  0,  0,  2,  2,  2,	/* a b c d e f g h i */
	 4,  4,  4,  4,  4,  4,  4,  4,  4,	/* j k l m n o p q r */
	 4,  6,  6,  6,  6,  6,  6,  6,  6	/* s t u v w x y z ~ */
};
LITDEF enum ztrprms ztrprm_data[] =
{
	ZTRP_FILE, ZTRP_FILE,
	ZTRP_ITEM, ZTRP_ITEM,
	ZTRP_SELECT, ZTRP_SELECT
};

void op_fnztrigger(mval *func, mval *arg1, mval *arg2, mval *dst)
{
	int		inparm_len, index;
	uint4		dummy_stats[NUM_STATS];
	boolean_t	failed;

	error_def(ERR_ZTRIGINVACT);
	error_def(ERR_ZTRIGNOTP);

	MV_FORCE_STR(func);
	MV_FORCE_STR(arg1);
	/* MV_FORCE_STR(arg2); optional arg2 not currently used - added so parm easily implemented when added */
	inparm_len = func->str.len;
	if ((0 >= inparm_len) || (NAME_ENTRY_SZ < inparm_len))
		/* We have a wrong-sized keyword */
		rts_error(VARLSTCNT(1) ERR_ZTRIGINVACT);
	if (0 > (index = namelook(ztrprm_index, ztrprm_names, func->str.addr, inparm_len)))
		/* Specified parm was not found */
		rts_error(VARLSTCNT(1) ERR_ZTRIGINVACT);
	switch(ztrprm_data[index])
	{
		case ZTRP_FILE:
			if (0 < dollar_tlevel)
				rts_error(VARLSTCNT(1) ERR_ZTRIGNOTP);
			failed = trigger_trgfile_tpwrap(arg1->str.addr, arg1->str.len, TRUE);
			break;
		case ZTRP_ITEM:
			if (0 < dollar_tlevel)
				rts_error(VARLSTCNT(1) ERR_ZTRIGNOTP);
			failed = trigger_update(arg1->str.addr, arg1->str.len);
			break;
		case ZTRP_SELECT:
			failed = trigger_select(arg1->str.addr, arg1->str.len, NULL, 0);
			break;
		default:
			GTMASSERT;	/* Should never happen with checks above */
	}
	memcpy(dst, (failed ? &literal_zero : &literal_one), SIZEOF(mval));
	return;
}
#else /* !GTM_TRIGGER */
void op_fnztrigger(mval *func, mval *arg1, mval *arg2, mval *dst)
{
	error_def(ERR_UNIMPLOP);

	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
}
#endif
