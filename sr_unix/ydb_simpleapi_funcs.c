/****************************************************************
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <stdarg.h>

#include "lv_val.h"
#include "hashtab_mname.h"
#include "callg.h"
#include "op.h"
#include "error.h"
#include "nametabtyp.h"
#include "compiler.h"
#include "namelook.h"
#include "stringpool.h"
#include "libyottadb_int.h"
#include "libydberrors.h"

GBLREF 	boolean_t		gtm_startup_active;
GBLREF	symval			*curr_symval;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;

/* Macro to pull subscripts out of caller's parameter list and buffer them for call to a runtime routine */
#define COPY_PARMS_TO_CALLG_BUFFER(COUNT)											\
MBSTART	{															\
	mval		*mvalp;													\
	void		**parmp, **parmp_top;											\
				 												\
	/* Now for each subscript */												\
	VAR_START(var, varname);												\
	for (parmp = &plist.arg[1], parmp_top = parmp + (COUNT), mvalp = &plist_mvals[0]; parmp < parmp_top; parmp++, mvalp++)	\
	{	/* Pull each subscript descriptor out of param list and put in our parameter buffer */	    	     		\
		subval = va_arg(var, ydb_buffer_t *);	       	    	       	   	     	    				\
		if (NULL != subval)		  										\
		{	/* A subscript has been specified - copy it to the associated mval and put its address			\
			 * in the param list to pass to op_putindx()								\
			 */													\
			SET_LVVAL_VALUE_FROM_BUFFER(mvalp, subval);								\
		} else					   									\
		{	/* No subscript specified - error */									\
			va_end(var);		    	  									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VARNAMEINVALID);						\
		}				    		 								\
		*parmp = mvalp;													\
	}	       	 													\
	plist.n = (COUNT) + 1;			/* Bump to include varname in parms */						\
	va_end(var);														\
} MBEND

/* Condition handler for simpleAPI environment. This routine catches all errors thrown by the YottaDB engine. The error
 * is basically returned to the user as the negative of the errror to differentiate those errors from positive (success
 * or informative) return codes of this API.
 */
CONDITION_HANDLER(ydb_simpleapi_ch)
{
	mstr		error_loc;

	START_CH(TRUE);
	if ((DUMPABLE) && !SUPPRESS_DUMP)
	{	/* Fatal errors need to create a core dump */
		need_core = TRUE;
		gtm_fork_n_core();
	}
	error_loc.addr = "error at xxx";
	error_loc.len = strlen(error_loc.addr);
	set_zstatus(&error_loc, arg, NULL, FALSE);
	TREF(ydb_error_code) = arg;	/* Record error code for caller */
	UNWIND(NULL, NULL); 		/* Return back to ESTABLISH_NORET() in caller */
}

/* Routine to set local, global and ISV values
 *
 * Parameters:
 *   value   - Value to be set into local/global/ISV
 *   count   - Count of subscripts (if any else 0)
 *   varname - Gives name of local, global or ISV variable
 *   subscrN - a list of 0 or more ydb_buffer_t subscripts follows varname in the parm list
 */
int ydb_set_s(ydb_buffer_t *value, int count, ydb_buffer_t *varname, ...)
{
	char		chr;
	ydb_set_types	set_type;
	int		set_svn_index, i;
	lv_val		*lvvalp, *dst_lv;
	mval		set_value, gvname, plist_mvals[YDB_MAX_SUBS + 1];
	boolean_t	error_encountered;
	gparam_list	plist;
	mname_entry	var_mname;
	ht_ent_mname	*tabent;
	ydb_buffer_t	*subval;
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up, ESTABLISH error handler, etc */
	LIBYOTTADB_INIT(LYDB_RTN_SET);
	/* Do some validation */
	VALIDATE_VARNAME(varname, set_type, set_svn_index);
	if (0 > count)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VARNAME_INVALID);
	if (YDB_MAX_SUBS < count)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	VALIDATE_VALUE(value);			/* Value must exist for SET */
	/* Separate actions depending on the type of SET being done */
	VAR_START(var, varname);
	switch(set_type)
	{
		case LYDB_SET_LOCAL:
			/* Set the given local variable with the given value */
			FIND_BASE_VAR(varname, &var_mname, tabent, lvvalp);		/* Locate the base var lv_val */
			if (0 == count)
			{	/* We have a var with NO subscripts - simple case - just copy value pointers in and rebuffer */
				SET_LVVAL_VALUE_FROM_BUFFER(lvvalp, value);
				break;
			}
			/* We have some subscripts - load the varname and the subscripts into our array for callg so we can drive
			 * op_putindx() to locate the mval associated with those subscripts that need to be set.
			 */
			plist.arg[0] = lvvalp;	/* First arg is lv_val of the base var */
			COPY_PARMS_TO_CALLG_BUFFER(count);		/* Set up plist for callg invocation of op_putindx */
			dst_lv = (lv_val *)callg((callgfnptr)op_putindx, &plist);	/* Drive op_putindx to locate/create node */
			SET_LVVAL_VALUE_FROM_BUFFER(dst_lv, value);	/* Set value into located/created node */
			SET_ACTIVE_LV(NULL, TRUE, actlv_ydb_set_s);	/* If get here, subscript set was successful so
									 * clear active_lv to avoid later cleanup issues.
									 */
			break;
		case LYDB_SET_GLOBAL:
			/* Set the given global variable with the given value. We do this by:
			 *   1. Drive op_gvname() with the global name and all subscripts to setup the key we need to global node.
			 *   2. Drive op_gvput() to place value into target global node
			 */
			gvname.mvtype = MV_STR;
			gvname.str.addr = varname->buf_addr + 1;	/* Point past '^' to var name */
			gvname.str.len = varname->len_used - 1;
			plist.arg[0] = &gvname;
			COPY_PARMS_TO_CALLG_BUFFER(count);		/* Set up plist for callg invocation of op_putindx */
			callg((callgfnptr)op_gvname, &plist);		/* Drive op_gvname() to create key */
			SET_LVVAL_VALUE_FROM_BUFFER(&set_value, value);	/* Put value to set into mval for op_gvput() */
			op_gvput(&set_value);				/* Save the global value */
			break;
		case LYDB_SET_ISV:
			/* Set the given ISV (subscripts not currently supported) with the given value */
			break;
		default:
			va_end(var);
			assertpro(FALSE);
	}
	va_end(var);
	REVERT;
	return YDB_OK;
}
