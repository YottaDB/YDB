/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
#include "error.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "io.h"
#include "hashtab_str.h"
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_update_protos.h"
#include "trigger_select_protos.h"
#include "trigger_trgfile_protos.h"

GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF  uint4		dollar_tlevel;
GBLREF	gd_addr		*gd_header;
GBLREF	gd_binding	*gd_map;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	boolean_t	dollar_ztrigger_invoked;
GBLREF	int4		gtm_trigger_depth;
GBLREF	mstr		*dollar_ztname;
#ifdef DEBUG
GBLREF	boolean_t	donot_INVOKE_MUMTSTART;
#endif

LITREF	mval	literal_zero;
LITREF	mval	literal_one;

STATICDEF char		save_currkey[SIZEOF(gv_key) + SIZEOF(short) + DBKEYSIZE(MAX_KEY_SZ)];	/* SIZEOF(short) for alignment */
STATICDEF gd_addr	*save_gd_header;
STATICDEF gd_binding	*save_gd_map;
STATICDEF gv_key	*save_gv_currkey;
STATICDEF gv_namehead	*save_gv_target;
STATICDEF gd_region	*save_gv_cur_region;
STATICDEF boolean_t	save_gv_last_subsc_null, save_gv_some_subsc_null;
#ifdef DEBUG
STATICDEF boolean_t	in_op_fnztrigger;
#endif

error_def(ERR_DZTRIGINTRIG);
error_def(ERR_FILENAMETOOLONG);
error_def(ERR_ZTRIGINVACT);


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

/* reset global variables from the values noted down at op_fnztrigger entry */
#define RESTORE_ZTRIGGER_ENTRY_STATE												\
{																\
	unsigned short	top;													\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	/* Reset global variables noted down at function entry */								\
	gd_map = save_gd_map;													\
	gd_header = save_gd_header;												\
	if (NULL != save_gv_cur_region)												\
	{															\
		gv_cur_region = save_gv_cur_region;										\
		TP_CHANGE_REG(gv_cur_region);											\
	}															\
	gv_target = save_gv_target;												\
	if (NULL != save_gv_currkey)												\
	{	/* gv_currkey->top could have changed if we opened a database with bigger keysize				\
		 * inside the trigger* functions above. Therefore take care not to overwrite that				\
		 * part of the gv_currkey structure. Restore everything else.							\
		 */														\
		top = gv_currkey->top;												\
		memcpy(gv_currkey, save_gv_currkey, SIZEOF(gv_key) + save_gv_currkey->end);					\
		gv_currkey->top = top;												\
	} else if (NULL != gv_currkey)												\
	{															\
		gv_currkey->end = 0;												\
		gv_currkey->base[0] = KEY_DELIMITER;										\
		TREF(gv_last_subsc_null) = save_gv_last_subsc_null;								\
		TREF(gv_some_subsc_null) = save_gv_some_subsc_null;								\
	}															\
	DEBUG_ONLY(in_op_fnztrigger = FALSE);											\
}

/* In case of an rts_error deep inside the op_fnztrigger invocation, we need to restore certain global variables that could
 * have been modified.
 */
CONDITION_HANDLER(op_fnztrigger_ch)
{
	START_CH;

	RESTORE_ZTRIGGER_ENTRY_STATE;
	DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);

	NEXTCH;

}

void op_fnztrigger(mval *func, mval *arg1, mval *arg2, mval *dst)
{
	int				inparm_len, index;
	uint4				dummy_stats[NUM_STATS], filename_len;
	boolean_t			failed;
	char				filename[MAX_FN_LEN + 1];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(FALSE == in_op_fnztrigger); /* $ZTRIGGER() should not nest */
	DEBUG_ONLY(in_op_fnztrigger = TRUE);
	/* $ZTRIGGER() is not allowed while already inside the trigger frame */
	if (0 < gtm_trigger_depth)
	{
		DEBUG_ONLY(in_op_fnztrigger = FALSE);
		rts_error(VARLSTCNT(4) ERR_DZTRIGINTRIG, 2, dollar_ztname->len, dollar_ztname->addr);
	}
	MV_FORCE_STR(func);
	MV_FORCE_STR(arg1);
	/* MV_FORCE_STR(arg2); optional arg2 not currently used - added so parm easily implemented when added */
	inparm_len = func->str.len;
	if ((0 >= inparm_len) || (NAME_ENTRY_SZ < inparm_len) || !ISALPHA_ASCII(func->str.addr[0]))
	{
		DEBUG_ONLY(in_op_fnztrigger = FALSE;)
		/* We have a wrong-sized keyword */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 1);
	}
	if (0 > (index = namelook(ztrprm_index, ztrprm_names, func->str.addr, inparm_len)))	/* Note assignment */
	{
		DEBUG_ONLY(in_op_fnztrigger = FALSE);
		/* Specified parm was not found */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 1);
	}
	if ((0 < arg1->str.len) && (0 == arg1->str.addr[0]))
	{
		DEBUG_ONLY(in_op_fnztrigger = FALSE);
		/* 2nd parameter is invalid */
		rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
	}
	save_gd_header = gd_header;
	save_gv_target = gv_target;
	save_gd_map = gd_map;
	save_gv_cur_region = gv_cur_region;
	if (NULL != gv_currkey)
	{	/* Align save_gv_currkey on a "short" boundary, but first we check that field "gv_currkey->end" is truly short */
		assert(SIZEOF(short) == SIZEOF(save_gv_currkey->end));
		save_gv_currkey = (gv_key *)ROUND_UP2((INTPTR_T)&save_currkey[0], SIZEOF(gv_currkey->end));
		memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
		save_gv_last_subsc_null = TREF(gv_last_subsc_null);
		save_gv_some_subsc_null = TREF(gv_some_subsc_null);
	} else
		save_gv_currkey = NULL;
	util_out_print(NULL, RESET);	/* Make sure the util_out_print_gtmio() messages start at the beginning of the buffer
					 * in case there are any stale messages already in the buffer - for example rts_error
					 * output left over from a trapped error and hence not printed
					 */
	/* Any time gv_currkey is manipulated, the global variables - gv_last_subsc_null and gv_some_subsc_null should also be
	 * set appropriately. Although the BUILD_HASHT_*_CURRKEY_T macros maintain this property, the
	 * INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED macro does not maintain the global variables. Since the macro is used in various
	 * other places (including update process and GT.M runtime), the implications of setting the global variables is not
	 * clear. So, reset gv_currkey and the global variables at the $ztrigger() entry as they will be restored before exiting
	 * from this function.
	 */
	if (gv_currkey)
	{
		gv_currkey->end = 0;
		gv_currkey->base[0] = KEY_DELIMITER;
	}
	TREF(gv_last_subsc_null) = TREF(gv_some_subsc_null) = FALSE;
	ESTABLISH(op_fnztrigger_ch);
	dollar_ztrigger_invoked = TRUE; /* reset after use when the transaction commits, restarts or rollbacks */
	switch(ztrprm_data[index])
	{
		case ZTRP_FILE:
			if (0 == arg1->str.len)
				/* 2nd parameter is missing */
				rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
			if (MAX_FN_LEN < arg1->str.len)
				rts_error(VARLSTCNT(1) ERR_FILENAMETOOLONG);
			/* The file name is in string pool so make a local copy in case GC happens */
			strncpy(filename, arg1->str.addr, arg1->str.len);
			filename_len = arg1->str.len;
			filename[filename_len] = '\0';
			failed = trigger_trgfile_tpwrap(filename, filename_len, TRUE);
			break;
		case ZTRP_ITEM:
			if (0 == arg1->str.len)
				/* 2nd parameter is missing */
				rts_error(VARLSTCNT(3) ERR_ZTRIGINVACT, 1, 2);
			failed = trigger_update(arg1->str.addr, arg1->str.len);
			break;
		case ZTRP_SELECT:
			failed = (TRIG_FAILURE == trigger_select(arg1->str.addr, arg1->str.len, NULL, 0));
			break;
		default:
			GTMASSERT;	/* Should never happen with checks above */
	}
	REVERT;
	RESTORE_ZTRIGGER_ENTRY_STATE;
	memcpy(dst, (failed ? &literal_zero : &literal_one), SIZEOF(mval));
	return;
}
#else /* !GTM_TRIGGER */

error_def(ERR_UNIMPLOP);

void op_fnztrigger(mval *func, mval *arg1, mval *arg2, mval *dst)
{
	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
}
#endif
