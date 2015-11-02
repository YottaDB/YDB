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

GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF  short  		dollar_tlevel;
GBLREF	gd_addr		*gd_header;
GBLREF	gd_binding	*gd_map;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;

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
	unsigned short	top;
	boolean_t	failed;
	char		filename[MAX_FN_LEN];
	char		save_currkey[SIZEOF(gv_key) + SIZEOF(short) + DBKEYSIZE(MAX_KEY_SZ)];	/* SIZEOF(short) for alignment */
	gd_addr		*save_gd_header;
	gd_binding	*save_gd_map;
	gv_key		*save_gv_currkey;
	gv_namehead	*save_gv_target;
	gd_region	*save_gv_cur_region;

	error_def(ERR_FILENAMETOOLONG);
	error_def(ERR_ZTRIGINVACT);
	error_def(ERR_ZTRIGNOTP);

	MV_FORCE_STR(func);
	MV_FORCE_STR(arg1);
	/* MV_FORCE_STR(arg2); optional arg2 not currently used - added so parm easily implemented when added */
	inparm_len = func->str.len;
	if ((0 >= inparm_len) || (NAME_ENTRY_SZ < inparm_len) || !ISALPHA_ASCII(func->str.addr[0]))
		/* We have a wrong-sized keyword */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 1);
	if (0 > (index = namelook(ztrprm_index, ztrprm_names, func->str.addr, inparm_len)))
		/* Specified parm was not found */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 1);
	if ((0 < arg1->str.len) && (0 == arg1->str.addr[0]))
		/* 2nd parameter is invalid */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
	save_gd_header = gd_header;
	save_gv_target = gv_target;
	save_gd_map = gd_map;
	save_gv_cur_region = gv_cur_region;
	if (NULL != gv_currkey)
	{	/* Align save_gv_currkey on a "short" boundary, but first we check that field "gv_currkey->end" is truly short */
		assert(SIZEOF(short) == SIZEOF(save_gv_currkey->end));
		save_gv_currkey = (gv_key *)ROUND_UP2((INTPTR_T)&save_currkey[0], SIZEOF(gv_currkey->end));
		memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
	} else
		save_gv_currkey = NULL;
	switch(ztrprm_data[index])
	{
		case ZTRP_FILE:
			if (0 == arg1->str.len)
				/* 2nd parameter is missing */
				rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
			if (0 < dollar_tlevel)
				rts_error(VARLSTCNT(1) ERR_ZTRIGNOTP);
			if (MAX_FN_LEN < arg1->str.len)
				rts_error(VARLSTCNT(1) ERR_FILENAMETOOLONG);
			/* The file name is in string pool so make a local copy in case GC happens */
			memcpy(filename, arg1->str.addr, arg1->str.len);
			failed = trigger_trgfile_tpwrap(filename, arg1->str.len, TRUE);
			break;
		case ZTRP_ITEM:
			if (0 == arg1->str.len)
				/* 2nd parameter is missing */
				rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
			if (0 < dollar_tlevel)
				rts_error(VARLSTCNT(1) ERR_ZTRIGNOTP);
			failed = trigger_update(arg1->str.addr, arg1->str.len);
			break;
		case ZTRP_SELECT:
			failed = (TRIG_FAILURE == trigger_select(arg1->str.addr, arg1->str.len, NULL, 0));
			break;
		default:
			GTMASSERT;	/* Should never happen with checks above */
	}
	gd_map = save_gd_map;
	gd_header = save_gd_header;
	if (NULL != save_gv_cur_region)
	{
		gv_cur_region = save_gv_cur_region;
		TP_CHANGE_REG(gv_cur_region);
	}
	gv_target = save_gv_target;
	if (NULL != save_gv_currkey)
	{	/* gv_currkey->top could have changed if we opened a database with bigger keysize
		 * inside the trigger* functions above. Therefore take care not to overwrite that
		 * part of the gv_currkey structure. Restore everything else.
		 */
		top = gv_currkey->top;
		memcpy(gv_currkey, save_gv_currkey, SIZEOF(gv_key) + save_gv_currkey->end);
		gv_currkey->top = top;
	} else if (NULL != gv_currkey)
	{
		gv_currkey->end = 0;
		gv_currkey->base[0] = KEY_DELIMITER;
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
