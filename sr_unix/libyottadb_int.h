/****************************************************************
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* A header file for the internal uses of libyottadb that do not need to be distributed as
 * part of the user interface. Includes libyottadb.h.
 */
#ifndef LIBYOTTADB_INT_H
#define LIBYOTTADB_INT_H

#include "gtm_pthread.h"
#include "gtm_string.h"		/* for strlen in LIBYOTTADB_INIT in RTS_ERROR_TEXT macro */
#include "gtm_stdlib.h"

#include "libyottadb.h"
#include "libyottadb_dbg.h"
#include "ydbmerrors.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "compiler.h"		/* needed for funsvn.h */
#include "funsvn.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "stack_frame.h"
#include "lv_val.h"
#include "libydberrors.h"	/* Define YDB_ERR_* errors */
#include "error.h"
#include "setup_error.h"
#include "sleep.h"
#include "gt_timer.h"
#include "sig_init.h"
#include "invocation_mode.h"
#include "sighnd_debug.h"

#define MAX_SAPI_MSTR_GC_INDX		YDB_MAX_NAMES
#define YDB_GET_VALUE_SIZE_ONLY		TRUE
#define YDB_GET_VALUE_FULL_VALUE	FALSE

GBLREF	symval		*curr_symval;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		process_id;
GBLREF 	boolean_t	ydb_init_complete;
GBLREF	sig_pending	sigPendingQue;		/* Queue of pending signals handled in alternate signal handling mode */

LITREF	char		ctypetab[NUM_CHARS];
LITREF	nametabent	svn_names[];
LITREF	unsigned char	svn_index[];
LITREF	svn_data_type	svn_data[];
LITREF	char		*lydb_simpleapi_rtnnames[];
LITREF	char		*lydb_simplethreadapi_rtnnames[];

#define YDB_MAX_SAPI_ARGS GTM64_ONLY(5) NON_GTM64_ONLY(6)	/* The most args any simpleapi routine has (excepting ydb_lock_s)
								 * is 5 but in 32 bit mode the max is 6.
								 */

/* Dimension of the thread work queue array. It holds one extra so if we go one level too far, we don't have to worry about
 * detecting it. We can go ahead and pass it to ydb_tp_s() who will detect the issue and fail the request. Note TP_MAX_LEVEL
 * itself has a built-in extra element. It is defined as 127 but the actual max level is 126. This extra block (we use as the
 * [0] block as the main work queue and subsequent levels as the TP work levels.
 */
#define STMWORKQUEUEDIM (TP_MAX_LEVEL + 1)

/* Values for TREF(libyottadb_active_rtn) */
#define LYDBRTN(lydbtype, simpleapi_rtnname, simplethreadapi_rtnname)	lydbtype
typedef enum
{
#include "libyottadb_rtns.h"
} libyottadb_routines;
#undef LYDBRTN

#define	LYDBRTNNAME(lydbtype)	(simpleThreadAPI_active					\
					? lydb_simplethreadapi_rtnnames[lydbtype]	\
					: lydb_simpleapi_rtnnames[lydbtype])

/* Returned values for VARTYPE in VALIDATE-VARNAME() macro */
typedef enum
{
	LYDB_VARREF_INVALID = 0,
	LYDB_VARREF_GLOBAL,		/* Referencing a global variable */
	LYDB_VARREF_LOCAL,		/* Referencing a local variable */
	LYDB_VARREF_ISV			/* Referencing an ISV (Intrinsic Special Variable) */
} ydb_var_types;

/* Create a common-ground type between ydb_tpfnptr_t and ydb_tp2fnptr_t by removing the arguments so ydb_tp_common can be
 * called with either one of them. It's just a basic function pointer.
 */
typedef int (*ydb_basicfnptr_t)();

/* Macros to startup YottaDB runtime if it is not going yet - one for routines with return values, one for not */
#define LIBYOTTADB_RUNTIME_CHECK(RETTYPE, ERRSTR)								\
MBSTART	{													\
	GBLREF boolean_t exit_handler_active;									\
	int	status;												\
														\
	/* If exit_handler_active is set, the context for calling YDB is gone */				\
	if (exit_handler_active)										\
	{													\
		SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_CALLINAFTERXIT, (ydb_buffer_t *)ERRSTR);		\
		return RETTYPE YDB_ERR_CALLINAFTERXIT;								\
	}													\
	/* No threadgbl usage in this macro until the following block completes */				\
	if (!ydb_init_complete)											\
	{	/* Have to initialize things before we can establish an error handler */			\
		assert(!USING_ALTERNATE_SIGHANDLING);	/* Go uses ydb_main_lang_init() instead */		\
		if (0 != (status = ydb_init()))		/* Note - sets fgncal_stack */				\
		{												\
			SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(-status, (ydb_buffer_t *)ERRSTR);			\
			return RETTYPE -status;									\
		}												\
	}		       											\
	/* At this point we are guaranteed "ydb_init_complete" is TRUE and so we are sure "gtm_threadgbl"	\
	 * would have been set to a non-null VALUE. So now is a safe point to set "lcl_gtm_threadgbl" to	\
	 * point to the global variable "gtm_threadgbl". We previously had this set inside the			\
	 * "if (!ydb_init_complete)" code block above. But that is not correct as in case of a multi-threaded	\
	 * application that uses the Simple Thread API it is possible multiple threads do concurrent calls to	\
	 * say "ydb_tp_st()" at the same time and it is possible the callers C1 and C2 both come to the "if"	\
	 * above at the same time and one of them, say C1, does the "ydb_init()" and sets "ydb_init_complete"	\
	 * while C2 finds "ydb_init_complete" set and comes here. In that case, C2 would still have		\
	 * not set "lcl_gtm_threadgbl" to a non-NULL value which is incorrect. Hence the set now happens after	\
	 * the "if" block. But this means callers of this macro can skip the SETUP_THREADGBL_ACCESS call as	\
	 * it is taken care of inside the macro now in all cases.						\
	 */													\
	SETUP_THREADGBL_ACCESS;											\
} MBEND

/* Macros to startup YottaDB runtime if it is not going yet - one for returns, one for not */
#define LIBYOTTADB_RUNTIME_CHECK_NORETVAL(ERRSTR)								\
MBSTART	{													\
	GBLREF boolean_t exit_handler_active;									\
	int	status;												\
														\
	/* If exit_handler_active is set, the context for calling YDB is gone */				\
	if (exit_handler_active)										\
	{													\
		SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_CALLINAFTERXIT, (ydb_buffer_t *)ERRSTR);		\
		return;												\
	}													\
	/* No threadgbl usage in this macro until the following block completes */				\
	if (!ydb_init_complete)											\
	{	/* Have to initialize things before we can establish an error handler */			\
		assert(!USING_ALTERNATE_SIGHANDLING);	/* Go uses ydb_main_lang_init() instead */		\
		if (0 != (status = ydb_init()))		/* Note - sets fgncal_stack */				\
		{												\
			SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(-status, (ydb_buffer_t *)ERRSTR);			\
			return;											\
		}												\
	}		       											\
	/* See comment above similar code in LIBYOTTADB_RUNTIME_CHECK for why the below line is placed here */	\
	SETUP_THREADGBL_ACCESS;											\
} MBEND

/* Initialization and cleanup macros for main simpleAPI calls. Having a value in TREF(libyottadb_active_rtn)
 * denotes a simpleAPI routine is active and no other simpleAPI routine can be started - i.e. calls cannot be
 * nested with the exception of ydb_tp_s() which clears this indicator just before it engages the call-back
 * routine which has a high probability of calling more simpleAPI routines.
 *
 * We need two flavors - one for use in routines with a return value and one without.
 */
#define LIBYOTTADB_INIT2(ROUTINE, RETTYPE, DO_SIMPLEAPINEST_CHECK)								\
MBSTART	{															\
	int		errcode;												\
																\
	GBLREF	int	mumps_status;												\
																\
	LIBYOTTADB_RUNTIME_CHECK(RETTYPE, NULL);	/* Note: Also does SETUP_THREADGBL_ACCESS; May return if error */	\
	assert(ydb_init_complete);	/* must be set by LIBYOTTADB_RUNTIME_CHECK above */					\
	/* Check if a nested call-in frame is needed (logic similar to INVOKE_YDB_NESTED_CALLIN_AND_RETURN_ON_ERROR,		\
	 * but cannot be directly used since the return is slightly different here.						\
	 */															\
	if (!(frame_pointer->type & SFT_CI))											\
	{															\
		boolean_t		error_encountered;									\
																\
		ESTABLISH_NORET(gtmci_ch, error_encountered);									\
		if (error_encountered)												\
		{	/* "gtmci_ch" encountered an error and transferred control back here. Return. */			\
			REVERT;													\
			assert(0 < mumps_status);										\
			return RETTYPE -mumps_status;										\
		}														\
		ydb_nested_callin();            /* Note - sets fgncal_stack */							\
		REVERT;														\
	}															\
	if (DO_SIMPLEAPINEST_CHECK)												\
	{															\
		/* Verify simpleAPI routines are not nesting. If we detect a problem here, the routine has not yet		\
		 * established the condition handler to take care of these issues so we simulate it's effect by			\
		 * doing the "set_zstatus", setting TREF(ydb_error_code) and returning the error code.				\
		 */														\
		if (LYDB_RTN_NONE != TREF(libyottadb_active_rtn))								\
		{														\
			errcode = YDB_ERR_SIMPLEAPINEST;									\
			SETUP_GENERIC_ERROR_2PARMS(errcode, LYDBRTNNAME(TREF(libyottadb_active_rtn)), LYDBRTNNAME(ROUTINE));	\
			return RETTYPE errcode;											\
		}														\
		TREF(libyottadb_active_rtn) = ROUTINE;										\
	}															\
	DBGAPI((stderr, "Entering routine %s\n", LYDBRTNNAME(ROUTINE)));							\
} MBEND

/* And now for the no return value edition */
#define LIBYOTTADB_INIT_NORETVAL2(ROUTINE, DO_SIMPLEAPINEST_CHECK)								\
MBSTART	{															\
	int		errcode;												\
																\
	LIBYOTTADB_RUNTIME_CHECK_NORETVAL(NULL);	/* Note: Also does SETUP_THREADGBL_ACCESS; May return if error */	\
	assert(ydb_init_complete);	/* must be set by LIBYOTTADB_RUNTIME_CHECK_NORETVAL above */				\
	/* Check if a nested call-in frame is needed (logic similar to INVOKE_YDB_NESTED_CALLIN_AND_RETURN_ON_ERROR,		\
	 * but cannot be directly used since the return is slightly different here.						\
	 */															\
	if (!(frame_pointer->type & SFT_CI))											\
	{															\
		boolean_t		error_encountered;									\
																\
		ESTABLISH_NORET(gtmci_ch, error_encountered);									\
		if (error_encountered)												\
		{	/* "gtmci_ch" encountered an error and transferred control back here. Return. */			\
			REVERT;													\
			return;													\
		}														\
		ydb_nested_callin();            /* Note - sets fgncal_stack */							\
		REVERT;														\
	}															\
	if (DO_SIMPLEAPINEST_CHECK)												\
	{															\
		/* Verify simpleAPI routines are not nesting. If we detect a problem here, the routine has not yet		\
		 * established the condition handler to take care of these issues so we simulate it's effect by			\
		 * doing the "set_zstatus", setting TREF(ydb_error_code) and returning the error code.				\
		 */														\
		if (LYDB_RTN_NONE != TREF(libyottadb_active_rtn))								\
		{														\
			errcode = YDB_ERR_SIMPLEAPINEST;									\
			SETUP_GENERIC_ERROR_2PARMS(errcode, LYDBRTNNAME(TREF(libyottadb_active_rtn)), LYDBRTNNAME(ROUTINE));	\
			return;													\
		}														\
		TREF(libyottadb_active_rtn) = ROUTINE;										\
	}															\
	DBGAPI((stderr, "Entering routine %s\n", LYDBRTNNAME(ROUTINE)));							\
} MBEND

#define	DO_SIMPLEAPINEST_CHECK_FALSE	FALSE
#define	DO_SIMPLEAPINEST_CHECK_TRUE	TRUE

#define	LIBYOTTADB_INIT(ROUTINE, RETTYPE)	LIBYOTTADB_INIT2(ROUTINE, RETTYPE, DO_SIMPLEAPINEST_CHECK_TRUE)
#define	LIBYOTTADB_INIT_NORETVAL(ROUTINE)	LIBYOTTADB_INIT_NORETVAL2(ROUTINE, DO_SIMPLEAPINEST_CHECK_TRUE)
#define	LIBYOTTADB_INIT_NOSIMPLEAPINEST_CHECK(RETTYPE) LIBYOTTADB_INIT2(LYDB_RTN_NONE, RETTYPE, DO_SIMPLEAPINEST_CHECK_FALSE)

#ifdef YDB_TRACE_API
# define LIBYOTTADB_DONE 									\
MBSTART {											\
	DBGAPI((stderr, "Exiting routine %s\n", LYDBRTNNAME(TREF(libyottadb_active_rtn))));	\
	TREF(libyottadb_active_rtn) = LYDB_RTN_NONE;						\
} MBEND
#else
# define LIBYOTTADB_DONE	TREF(libyottadb_active_rtn) = LYDB_RTN_NONE
#endif

/* Macros to promote checking an mname as being a valid mname. There are two flavors - one where
 * the first character needs to be checked and one where the first character has already been
 * checked and no further checking needs to be done.
 */

/* A macro to check the entire MNAME for validity. Sets ISVALID to TRUE if valid and FALSE otherwise */
#define VALIDATE_MNAME_C1(VARNAMESTR, VARNAMELEN, ISVALID)					\
MBSTART {											\
	char ctype;										\
	     											\
	assert(0 < (VARNAMELEN));								\
	ctype = ctypetab[(unsigned char)(VARNAMESTR)[0]];					\
	ISVALID = TRUE;										\
	switch(ctype)										\
	{											\
		case TK_LOWER:									\
		case TK_UPPER:									\
		case TK_PERCENT: 								\
			/* Valid first character */						\
			break;	       		 						\
		default:									\
			ISVALID = FALSE;							\
			break;									\
	}											\
	if (ISVALID)										\
		VALIDATE_MNAME_C2((VARNAMESTR) + 1, (VARNAMELEN) - 1, ISVALID);			\
} MBEND

/* Validate the 2nd char through the end of a given MNAME for validity. Assumes ISVALID is set to TRUE
 * at macro entry and sets it to FALSE if it finds some invalidity. Otherwise it leaves ISVALID as is (i.e. TRUE).
 */
#define VALIDATE_MNAME_C2(VARNAMESTR, VARNAMELEN, ISVALID)					\
MBSTART {											\
	char 		ctype, *cptr, *ctop;							\
	signed char	ch;	      								\
	       											\
	assert(0 <= (VARNAMELEN));								\
	assert(ISVALID);									\
	for (cptr = (VARNAMESTR), ctop = (VARNAMESTR) + (VARNAMELEN); cptr < ctop; cptr++)	\
	{	    			     	    			  		      	\
		ctype = ctypetab[(unsigned char)*cptr];						\
		switch(ctype)									\
		{										\
			case TK_LOWER:								\
			case TK_UPPER:								\
			case TK_DIGIT:								\
				continue;							\
			default:								\
				ISVALID = FALSE;						\
				break;								\
		}		       								\
		assert(!ISVALID);								\
		break;										\
	}											\
} MBEND

/* A macro for verifying an input variable name. This macro checks for things like
 *   - Non-zero length
 *   - Non-NULL address
 *   - Determines the type of var (global, local, ISV)
 *
 * Any failure in verification results in an error (various error scenarios are tested below).
 *
 * VARNAMEP, SUBS_USED, UPDATE, LYDB_RTN_NAME and INDEX are input parameters.
 * VARTYPE and VARINDEX are output parameters.
 */
#define VALIDATE_VARNAME(VARNAMEP, SUBS_USED, UPDATE, LYDB_RTN_NAME, INDEX, VARTYPE, VARINDEX)				\
MBSTART {														\
	boolean_t	isvalid;											\
	char		ctype;												\
	int		iNDX;	/* named "iNDX" to avoid potential name conflict with "index" variable in caller */	\
	int		lenUsed;											\
															\
	if (IS_INVALID_YDB_BUFF_T(VARNAMEP))										\
	{														\
		char		buff[256];		/* snprintf() buffer */						\
															\
		/* If INDEX is -1, it means caller is "ydb_delete_excl_s" or "ydb_tp_s_common" where it processes	\
		 * a list of local variable names. In that case, we need to specify a variable name index in the	\
		 * error message so the user can easily spot which variable had the PARAMINVALID error. Hence the	\
		 * slightly different error message logic below.							\
		 */													\
		if (-1 == INDEX)											\
			SNPRINTF(buff, SIZEOF(buff), "Invalid varname");						\
		else													\
			SNPRINTF(buff, SIZEOF(buff), "Invalid varname array (index %d)", INDEX);			\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,						\
			LEN_AND_STR(buff), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_NAME)));					\
	}														\
	lenUsed = (VARNAMEP)->len_used;											\
	if (0 == lenUsed)												\
		ydb_issue_invvarname_error(VARNAMEP);									\
	/* Characterize first char of name ($, ^, %, or letter) */							\
	ctype = ctypetab[(unsigned char)(VARNAMEP)->buf_addr[0]];							\
	switch(ctype)													\
	{														\
		case TK_CIRCUMFLEX:											\
			lenUsed--;											\
			if (YDB_MAX_IDENT < lenUsed)									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);		\
			if (0 == lenUsed)										\
				ydb_issue_invvarname_error(VARNAMEP);							\
			VARTYPE = LYDB_VARREF_GLOBAL;									\
			VALIDATE_MNAME_C1((VARNAMEP)->buf_addr + 1, lenUsed, isvalid);					\
			if (!isvalid)											\
				ydb_issue_invvarname_error(VARNAMEP);							\
			/* If INDEX is -1, it means caller is "ydb_delete_excl_s" or "ydb_tp_s_common" where it		\
			 * processes a list of local variable names. In that case, we do not expect a global variable	\
			 * name. Issue an error.									\
			 */												\
			if (-1 != INDEX)										\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GVNUNSUPPORTED, 4,				\
					(VARNAMEP)->len_used, (VARNAMEP)->buf_addr,					\
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_NAME)));					\
			break;				      	   		      					\
		case TK_LOWER:												\
		case TK_UPPER:												\
		case TK_PERCENT: 											\
			if (YDB_MAX_IDENT < lenUsed)									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);		\
			VARTYPE = LYDB_VARREF_LOCAL;									\
			isvalid = TRUE;											\
			VALIDATE_MNAME_C2((VARNAMEP)->buf_addr + 1, lenUsed - 1, isvalid);				\
			if (!isvalid)											\
				ydb_issue_invvarname_error(VARNAMEP);							\
			break;												\
		case TK_DOLLAR:												\
			lenUsed--;											\
			if (0 == lenUsed)										\
				ydb_issue_invvarname_error(VARNAMEP);							\
			VARTYPE = LYDB_VARREF_ISV;									\
			iNDX = namelook(svn_index, svn_names, (VARNAMEP)->buf_addr + 1, lenUsed); 			\
			if (-1 == iNDX) 	     			     						\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVSVN);					\
			if (UPDATE && !svn_data[iNDX].can_set)								\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SVNOSET);					\
			/* ISV usages are valid only if caller is "ydb_get_s" or "ydb_set_s". Else issue error. */	\
			if ((LYDB_RTN_GET != LYDB_RTN_NAME) && (LYDB_RTN_SET != LYDB_RTN_NAME))				\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ISVUNSUPPORTED, 4,				\
					(VARNAMEP)->len_used, (VARNAMEP)->buf_addr,					\
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_NAME)));					\
			else if (0 != SUBS_USED)									\
				/* Subscripted ISVs are not allowed/supported */					\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ISVSUBSCRIPTED, 3,				\
								(VARNAMEP)->len_used, (VARNAMEP)->buf_addr, SUBS_USED);	\
			VARINDEX = svn_data[iNDX].opcode;								\
			break;	   											\
		default:												\
			ydb_issue_invvarname_error(VARNAMEP);								\
			break;												\
	}		       												\
	/* If we reach here, it means VARNAMEP has been determined to be a valid variable name (if not we would have	\
	 * invoked "ydb_issue_invvarname_error()" and transferred control out of this macro), do additional checks.	\
	 */														\
	if (LYDB_RTN_LOCK != LYDB_RTN_NAME)										\
	{														\
		if (0 > SUBS_USED)											\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);					\
		if (YDB_MAX_SUBS < SUBS_USED)										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);					\
	} else														\
	{														\
		if (0 > SUBS_USED)											\
		{													\
			char		buff[256];		/* snprintf() buffer */					\
															\
			SNPRINTF(buff, SIZEOF(buff), "Invalid subsarray for %.*s",					\
					(VARNAMEP)->len_used, (VARNAMEP)->buf_addr);					\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MINNRSUBSCRIPTS, 0,				\
									ERR_TEXT, 2, LEN_AND_STR(buff));		\
		}													\
		if (YDB_MAX_SUBS < SUBS_USED)										\
		{													\
			char		buff[256];		/* snprintf() buffer */					\
															\
			SNPRINTF(buff, SIZEOF(buff), "Invalid subsarray for %.*s",					\
					(VARNAMEP)->len_used, (VARNAMEP)->buf_addr);					\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MAXNRSUBSCRIPTS, 0,				\
									ERR_TEXT, 2, LEN_AND_STR(buff));		\
		}													\
	}														\
} MBEND

/* Macro to locate or create an entry in the symbol table for the specified base variable name. There are
 * two flavors:
 *   1. FIND_BASE_VAR_UPD()   - var is being set so input names need rebuffering.
 *   2. FIND_BASE_VAR_NOUPD() - is only fetching, not setting so no rebuffering of input names/vars is needed.
 */
#define FIND_BASE_VAR_UPD(VARNAMEP, VARMNAMEP, VARTABENTP, VARLVVALP)								\
MBSTART {															\
	boolean_t	added;													\
	lv_val		*lv;													\
																\
	(VARMNAMEP)->var_name.addr = (VARNAMEP)->buf_addr;	/* Load up the imbedded varname with our base var name */	\
	(VARMNAMEP)->var_name.len = (VARNAMEP)->len_used;	   	       			     	      	       		\
	s2pool(&(VARMNAMEP)->var_name);		/* Rebuffer in stringpool for protection */					\
	RECORD_MSTR_FOR_GC(&(VARMNAMEP)->var_name);										\
	(VARMNAMEP)->marked = 0;												\
	COMPUTE_HASH_MNAME((VARMNAMEP));			/* Compute its hash value */					\
	added = add_hashtab_mname_symval(&curr_symval->h_symtab, (VARMNAMEP), NULL, &(VARTABENTP));				\
	assert(VARTABENTP);													\
	if (NULL == (VARTABENTP)->value)											\
	{	    														\
		assert(added);													\
		lv_newname((VARTABENTP), curr_symval);										\
	}							 	      	  						\
	assert(NULL != LV_GET_SYMVAL((lv_val *)((VARTABENTP)->value)));								\
	VARLVVALP = (VARTABENTP)->value;											\
} MBEND

/* Now the NOUPD version */
#define FIND_BASE_VAR_NOUPD(VARNAMEP, VARMNAMEP, VARTABENTP, VARLVVALP)								\
MBSTART {															\
	lv_val		*lv;													\
																\
	(VARMNAMEP)->var_name.addr = (VARNAMEP)->buf_addr;	/* Load up the imbedded varname with our base var name */	\
	(VARMNAMEP)->var_name.len = (VARNAMEP)->len_used;	   	       			     	      	       		\
	(VARMNAMEP)->marked = 0;												\
	COMPUTE_HASH_MNAME((VARMNAMEP));			/* Compute its hash value */					\
	(VARTABENTP) = lookup_hashtab_mname(&curr_symval->h_symtab, (VARMNAMEP));						\
	if ((NULL == (VARTABENTP)) || (NULL == (lv_val *)((VARTABENTP)->value)))						\
		VARLVVALP = NULL;												\
	else															\
	{															\
		assert(NULL != LV_GET_SYMVAL((lv_val *)((VARTABENTP)->value)));							\
		VARLVVALP = (VARTABENTP)->value;										\
	}															\
} MBEND

/* Macro to check whether input to ydb_*_s() function from the user is a valid "ydb_buffer_t" structure */
#define	IS_INVALID_YDB_BUFF_T(YDBBUFF)	(((YDBBUFF)->len_alloc < (YDBBUFF)->len_used)				\
					 || ((NULL == (YDBBUFF)->buf_addr) && (0 != (YDBBUFF)->len_used)))

/* Macro to set a supplied ydb_buffer_t value into an mval.
 * This macro is kept simple and initializes only the bare minimum fields in an mval that are needed for
 * correct SimpleAPI working. If there is the potential for this mval to be used in M code, then one also
 * needs to invoke the INIT_MVAL_BEFORE_USE_IN_M_CODE macro or else garbage fields in the mval can cause
 * hard-to-debug issues (e.g. $PIECE behaving incorrectly due to fnpc_indx field being garbage etc.).
 */
#define SET_MVAL_FROM_YDB_BUFF_T(MVALP, YDBBUFF)							\
MBSTART	{												\
	assert(!IS_INVALID_YDB_BUFF_T(YDBBUFF)); /* caller should have issued error appropriately */	\
	(MVALP)->mvtype = MV_STR;									\
	(MVALP)->str.addr = (YDBBUFF)->buf_addr;							\
	(MVALP)->str.len = (YDBBUFF)->len_used;								\
} MBEND

#define	INIT_MVAL_BEFORE_USE_IN_M_CODE(MVALP)								\
MBSTART	{												\
	/* The below fields are normally not used in SimpleAPI mode but could be if we are inside a	\
	 * database trigger (which currently only runs in M and not in C) and if $PIECE etc. are used	\
	 * in the trigger M code, then these fields need to be properly initialized or else we could	\
	 * see incorrect results from $PIECE for example. Hence the need to initialize these fields.	\
	 */												\
	(MVALP)->fnpc_indx = FNPC_INDX_INVALID;								\
	(MVALP)->utfcgr_indx = UTFCGR_INDX_INVALID;							\
} MBEND

/* Macro to set a supplied ydb_buffer_t value from a supplied mval.
 * PARAM1 and PARAM2 are parameters supplied to the PARAMINVALID error if it needs to be issued.
 */
#define SET_YDB_BUFF_T_FROM_MVAL(YDBBUFF, MVALP, PARAM1, PARAM2)						\
MBSTART	{													\
	mval		*sRC;	/* named so to avoid name collision with caller */				\
	ydb_buffer_t	*dST;	/* named so to avoid name collision with caller */				\
														\
	sRC = MVALP;												\
	/* It is possible source mval is not of type MV_STR. But ydb_buff_t needs one				\
	 * so convert incoming mval to a string and then copy it, if needed.					\
	 */													\
	MV_FORCE_STR(sRC);											\
	assert(MV_IS_STRING(sRC));										\
	dST = YDBBUFF;												\
	if ((unsigned)sRC->str.len > dST->len_alloc)								\
	{													\
		dST->len_used = sRC->str.len;	/* Set len to what it needed to be */				\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, sRC->str.len, dST->len_alloc);	\
	}													\
	if (sRC->str.len)											\
	{													\
		if (NULL == dST->buf_addr)									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6)						\
				ERR_PARAMINVALID, 4, LEN_AND_LIT(PARAM1), LEN_AND_STR(PARAM2));			\
		memcpy(dST->buf_addr, sRC->str.addr, sRC->str.len);						\
	}													\
	dST->len_used = sRC->str.len;										\
} MBEND

#define GET_REQUIRED_YDB_BUFF_T_SIZE_FROM_MVAL(YDBBUFF, MVALP, PARAM1, PARAM2)					\
MBSTART	{													\
	mval		*sRC;	/* named so to avoid name collision with caller */				\
	ydb_buffer_t	*dST;	/* named so to avoid name collision with caller */				\
														\
	sRC = MVALP;												\
	/* It is possible source mval is not of type MV_STR. But ydb_buff_t needs one				\
	 * so convert incoming mval to a string.								\
	 */													\
	MV_FORCE_STR(sRC);											\
	assert(MV_IS_STRING(sRC));										\
	dST = YDBBUFF;												\
	dST->len_used = sRC->str.len;	/* Set len to what it needs to be */					\
} MBEND

/* Debug macro to dump the PLIST structure that is being passed into a runtime opcode. To use, be sure to
 * set DEBUG_LIBYOTTADB *prior* to including libyottadb_int.h
 */
#ifdef DEBUG_LIBYOTTADB
#define DBG_DUMP_PLIST_STRUCT(PLIST)												\
MBSTART	{															\
	int		idx;													\
	void		**parmp, **parmp_top;											\
																\
	printf("\n******** Dump of parameter list structure passed to runtime opcodes:\n");					\
	fflush(stdout);														\
	/* Note, skip the first entry as that is the lv_val address of the base var being referenced */				\
	for (idx = 1, parmp = &PLIST.arg[1], parmp_top = parmp + PLIST.n - 1; parmp < parmp_top; idx++, parmp++)		\
	{															\
		printf("Index %d - parmp: 0x%08lx, parmp_top: 0x%08lx,  ", idx, (gtm_uint8)parmp, (gtm_uint8)parmp_top);	\
		printf("mval (type %d) value: %.*s\n", ((mval *)(*parmp))->mvtype, ((mval *)(*parmp))->str.len,			\
		       ((mval *)(*parmp))->str.addr);										\
		fflush(stdout); \
	}															\
	printf("******** End of dump\n");											\
	fflush(stdout);														\
} MBEND
#else
#define DBG_DUMP_PLIST_STRUCT(PLIST)
#endif

/* Macro to pull subscripts out of caller's parameter list and buffer them for call to a runtime routine.
 * PARAM2 is parameter # 2 supplied to the PARAMINVALID error if it needs to be issued.
 */
#define COPY_PARMS_TO_CALLG_BUFFER(COUNT, SUBSARRAY, PLIST, PLIST_MVALS, REBUFFER, STARTIDX, PARAM2)		\
MBSTART	{													\
	mval		*mvalp;											\
	void		**parmp, **parmp_top;									\
	ydb_buffer_t	*subs;											\
	char		buff[256];	/* snprintf buffer */							\
				 										\
	subs = (ydb_buffer_t *)SUBSARRAY;	 								\
	if ((COUNT) && (NULL == subs))										\
	{       /* count of subscripts is non-zero but no subscript specified - error */			\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SUBSARRAYNULL, 3, (COUNT), LEN_AND_STR(PARAM2));	\
	}													\
	/* Now for each subscript */										\
	for (parmp = &PLIST.arg[STARTIDX], parmp_top = parmp + (COUNT), mvalp = &PLIST_MVALS[0];		\
	     parmp < parmp_top;											\
	     parmp++, mvalp++, subs++)										\
	{	/* Pull each subscript descriptor out of param list and put in our parameter buffer.	    	\
		 * A subscript has been specified - copy it to the associated mval and put its address		\
		 * in the param list. But before that, do validity checks on input ydb_buffer_t.		\
		 */												\
		if (IS_INVALID_YDB_BUFF_T(subs))								\
		{												\
			SNPRINTF(buff, SIZEOF(buff), "Invalid subsarray (index %d)",				\
							subs - ((ydb_buffer_t *)SUBSARRAY));			\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,				\
				      LEN_AND_STR(buff), LEN_AND_STR(PARAM2));					\
		}												\
		CHECK_MAX_STR_LEN(subs);		/* Generates error if subscript too long */		\
		SET_MVAL_FROM_YDB_BUFF_T(mvalp, subs);								\
		if (REBUFFER)											\
		{												\
			s2pool(&(mvalp->str));	/* Rebuffer in stringpool for protection */			\
			RECORD_MSTR_FOR_GC(&(mvalp->str));							\
		}												\
		*parmp = mvalp;											\
	}	       	 											\
	PLIST.n = (COUNT) + (STARTIDX);		/* Bump to include varname lv_val as 2nd parm (after count) */	\
	DBG_DUMP_PLIST_STRUCT(PLIST);										\
} MBEND

/* Macro to check the length of a ydb_buff_t and verify the len_used field does not exceed YDB_MAX_STR (the
 * maximum length of a string in YDB).
 */
#define CHECK_MAX_STR_LEN(YDBBUFF)										\
MBSTART {													\
	if (YDB_MAX_STR < (YDBBUFF)->len_used)									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, (YDBBUFF)->len_used, YDB_MAX_STR);	\
} MBEND

/* Macro to record the address of a given mstr so garbage collection knows where to find it to fix it up if needbe */
#define RECORD_MSTR_FOR_GC(MSTRP)										\
MBSTART	{													\
	mstr	*mstrp;												\
														\
	if (NULL == (mstrp = TREF(sapi_mstrs_for_gc_ary)))	/* Note assignment */				\
	{													\
		mstrp = TREF(sapi_mstrs_for_gc_ary) = malloc(SIZEOF(mstr) * (MAX_SAPI_MSTR_GC_INDX + 1));	\
		TREF(sapi_mstrs_for_gc_indx) = 0;			    					\
	}				       									\
	assert(MAX_SAPI_MSTR_GC_INDX > TREF(sapi_mstrs_for_gc_indx));						\
	if (0 < (MSTRP)->len)											\
		TAREF1(sapi_mstrs_for_gc_ary, (TREF(sapi_mstrs_for_gc_indx))++) = MSTRP;			\
} MBEND

/* Macro to determine if the simple API environment is active. The check is to see if the top stack frame is
 * a call-in base frame. This is the normal state for the simple API while the normal state (while in the
 * runtime) for call-ins will have an executable frame on top. Care must be taken in the use of this macro
 * though because if a call-in has returned to its caller in gtmci.c, but not yet to the call-in's caller,
 * there is also no executable frame (and likewise during initialization of a call-in before the executable
 * frame has been setup. As long as this macro is only used in places where we know we are dealing with a
 * runtime call (i.e. op_*), then this macro is accurate. Note there are various places in MUPIP where this
 * macro is used that also have issues with frame_pointer being NULL.
 */
#define IS_SIMPLEAPI_MODE	((NULL != frame_pointer) && (frame_pointer->type & SFT_CI))

#define	ISSUE_TIME2LONG_ERROR_IF_NEEDED(INPUT_TIME_IN_NANOSECONDS)				\
MBSTART {											\
	unsigned long long	max_time_nsec;							\
												\
	assert(SIZEOF(unsigned long long) == SIZEOF(INPUT_TIME_IN_NANOSECONDS));		\
	assert(MAXPOSINT4 == (YDB_MAX_TIME_NSEC / NANOSECS_IN_MSEC));				\
	if (YDB_MAX_TIME_NSEC < INPUT_TIME_IN_NANOSECONDS)					\
	{											\
		max_time_nsec = YDB_MAX_TIME_NSEC;						\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TIME2LONG, 2,			\
			(unsigned long long *)&INPUT_TIME_IN_NANOSECONDS, &max_time_nsec);	\
	}											\
} MBEND

#define	SET_M_ENTRYREF_TO_SIMPLEAPI_OR_SIMPLETHREADAPI(ENTRYREF)					\
{													\
	ENTRYREF.addr = (simpleThreadAPI_active ? SIMPLETHREADAPI_M_ENTRYREF : SIMPLEAPI_M_ENTRYREF);	\
	ENTRYREF.len = STRLEN(ENTRYREF.addr);								\
}

/* Macro to create a SYSCALL error. Since these are usually "encountered" instead of "thrown" in the
 * various no-mans-land parts of the code we'll be running in (not in user code but prior to any
 * condition handlers being installed), we can't use rts_error_csa() to reflect errors back to the
 * caller. The caller is going to expect an error return code means an appropriate value is stored
 * in $ZSTATUS so we have to get that setup before returning to the caller.
 */
#define SETUP_SYSCALL_ERROR(CALLNAME, STATUS)								\
MBSTART {												\
	mstr	entryref;										\
	setup_error(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,						\
		    RTS_ERROR_LITERAL(CALLNAME), RTS_ERROR_LITERAL(__FILE__), __LINE__, STATUS);	\
	SET_M_ENTRYREF_TO_SIMPLEAPI_OR_SIMPLETHREADAPI(entryref);					\
	set_zstatus(&entryref, MAX_ENTRYREF_LEN, ERR_SYSCALL, NULL, FALSE);				\
	TREF(ydb_error_code) = ERR_SYSCALL;								\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error (no parameters) */
#define SETUP_GENERIC_ERROR(ERRNUM)									\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(1) errnum);							\
	SET_M_ENTRYREF_TO_SIMPLEAPI_OR_SIMPLETHREADAPI(entryref);					\
	set_zstatus(&entryref, MAX_ENTRYREF_LEN, errnum, NULL, FALSE);					\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error with 2 parameters */
#define SETUP_GENERIC_ERROR_2PARMS(ERRNUM, PARM1, PARM2)						\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(4) errnum, 2, (PARM1), (PARM2));				\
	SET_M_ENTRYREF_TO_SIMPLEAPI_OR_SIMPLETHREADAPI(entryref);					\
	set_zstatus(&entryref, MAX_ENTRYREF_LEN, errnum, NULL, FALSE);					\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error with 3 parameters */
#define SETUP_GENERIC_ERROR_3PARMS(ERRNUM, PARM1, PARM2, PARM3)						\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(5) errnum, 3, (PARM1), (PARM2), (PARM3));			\
	SET_M_ENTRYREF_TO_SIMPLEAPI_OR_SIMPLETHREADAPI(entryref);					\
	set_zstatus(&entryref, MAX_ENTRYREF_LEN, errnum, NULL, FALSE);					\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* A process is not allowed to switch "modes" in midstream. This means if a process has started using non-threaded APIs and
 * services, it cannot switch to using threaded APIs and services and vice versa.
 *
 * To that end, the following macros verify these conditions.
 */
#define VERIFY_NON_THREADED_API 										\
MBSTART {	/* If threaded API but in worker thread, that is OK */						\
	GBLREF boolean_t noThreadAPI_active;									\
	GBLREF boolean_t simpleThreadAPI_active;								\
	GBLREF boolean_t caller_func_is_stapi;									\
	if (simpleThreadAPI_active)										\
	{													\
		if (!caller_func_is_stapi)									\
		{												\
			SETUP_GENERIC_ERROR(ERR_SIMPLEAPINOTALLOWED);						\
			DBGAPITP_ONLY(gtm_fork_n_core());							\
			/* No need to reset active routine indicator before returning an error.			\
			 * Caller would not have done LIBYOTTADB_INIT before invoking this macro.		\
			 * Assert that. The only exception is if we were already inside a "ydb_ci_t" or		\
			 * "ydb_cip_t" so account for that in the assert.					\
			 */											\
			assert((LYDB_RTN_NONE == TREF(libyottadb_active_rtn))					\
				|| (LYDB_RTN_YDB_CI == TREF(libyottadb_active_rtn)));				\
			return YDB_ERR_SIMPLEAPINOTALLOWED;							\
		}												\
		caller_func_is_stapi = FALSE;									\
		/* We are in threaded mode but running an unthreaded command in the main work thread which	\
		 * is allowed. In that case just fall out (verified).						\
		 */												\
	} else													\
		noThreadAPI_active = TRUE;									\
} MBEND

/* Variant of VERIFY_NON_THREADED_API macro that does a "return" but without any value */
#define VERIFY_NON_THREADED_API_NORETVAL									\
MBSTART {	/* If threaded API but in worker thread, that is OK */						\
	GBLREF boolean_t noThreadAPI_active;									\
	GBLREF boolean_t simpleThreadAPI_active;								\
	GBLREF boolean_t caller_func_is_stapi;									\
	if (simpleThreadAPI_active)										\
	{													\
		if (!caller_func_is_stapi)									\
		{												\
			SETUP_GENERIC_ERROR(ERR_SIMPLEAPINOTALLOWED);						\
			DBGAPITP_ONLY(gtm_fork_n_core());							\
			/* No need to reset active routine indicator before returning an error.			\
			 * Caller would not have done LIBYOTTADB_INIT before invoking this macro.		\
			 * Assert that. The only exception is if we were already inside a "ydb_ci_t" or		\
			 * "ydb_cip_t" so account for that in the assert.					\
			 */											\
			assert((LYDB_RTN_NONE == TREF(libyottadb_active_rtn))					\
				|| (LYDB_RTN_YDB_CI == TREF(libyottadb_active_rtn)));				\
			return;											\
		}												\
		caller_func_is_stapi = FALSE;									\
		/* We are in threaded mode but running an unthreaded command in the main work thread which	\
		 * is allowed. In that case just fall out (verified).						\
		 */												\
	} else													\
		noThreadAPI_active = TRUE;									\
} MBEND

/* Variant of VERIFY_NON_THREADED_API macro that does a "return" but with a NULL pointer value */
#define VERIFY_NON_THREADED_API_RETNULL										\
	MBSTART {	/* If threaded API but in worker thread, that is OK */					\
	GBLREF boolean_t noThreadAPI_active;									\
	GBLREF boolean_t simpleThreadAPI_active;								\
	GBLREF boolean_t caller_func_is_stapi;									\
	if (simpleThreadAPI_active)										\
	{													\
		if (!caller_func_is_stapi)									\
		{												\
			SETUP_GENERIC_ERROR(ERR_SIMPLEAPINOTALLOWED);						\
			DBGAPITP_ONLY(gtm_fork_n_core());							\
			/* No need to reset active routine indicator before returning an error.			\
			 * Caller would not have done LIBYOTTADB_INIT before invoking this macro.		\
			 * Assert that. The only exception is if we were already inside a "ydb_ci_t" or		\
			 * "ydb_cip_t" so account for that in the assert.					\
			 */											\
			assert((LYDB_RTN_NONE == TREF(libyottadb_active_rtn))					\
				|| (LYDB_RTN_YDB_CI == TREF(libyottadb_active_rtn)));				\
			return NULL;										\
		}												\
		caller_func_is_stapi = FALSE;									\
		/* We are in threaded mode but running an unthreaded command in the main work thread which	\
		 * is allowed. In that case just fall out (verified).						\
		 */												\
	} else													\
		noThreadAPI_active = TRUE;									\
} MBEND

#define VERIFY_THREADED_API(RETTYPE, ERRSTR)									\
MBSTART {													\
	GBLREF boolean_t noThreadAPI_active;									\
	if (noThreadAPI_active)											\
	{													\
		SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_THREADEDAPINOTALLOWED, (ydb_buffer_t *)ERRSTR);	\
		DBGAPITP_ONLY(gtm_fork_n_core());								\
		return RETTYPE YDB_ERR_THREADEDAPINOTALLOWED;							\
	}													\
} MBEND
#define VERIFY_THREADED_API_NORETVAL(ERRSTR)									\
MBSTART {													\
	GBLREF boolean_t noThreadAPI_active;									\
	if (noThreadAPI_active)											\
	{													\
		SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_THREADEDAPINOTALLOWED, (ydb_buffer_t *)ERRSTR);	\
		DBGAPITP_ONLY(gtm_fork_n_core());								\
		return;												\
	}													\
} MBEND

/* tptoken is a 64-bit quantity. The least significant 57 bits is a counter (that matches the global variable "stmTPToken").
 * When passed to a user function, the current TP depth (dollar_tlevel) which can only go upto 127 (i.e. 7 bits) is bitwise-ORed
 * into the most significant 7 bits thereby generating a 64-bit token.
 */
#define	TPTOKEN_NBITS				57
#define	USER_VISIBLE_TPTOKEN(tpdepth, tptoken)	(((uint64_t)tpdepth << TPTOKEN_NBITS) | tptoken)
#define	GET_TPDEPTH_FROM_TPTOKEN(tptoken)	(tptoken >> TPTOKEN_NBITS)
#define	GET_INTERNAL_TPTOKEN(tptoken)		(tptoken & (((uint64_t)1 << TPTOKEN_NBITS) - 1))

/* This macro fills in the "ydb_buffer_t" structure pointed to by "errstr" with the error string corresponding
 * to the error code "errnum". errstr->buf_addr is a null terminated string at the end. errstr->len_used is
 * set just like is done in the function "ydb_simpleapi_ch" for TREF(stapi_errstr).
 */
#define	SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(ERRNUM, ERRSTR)							\
{														\
	GBLREF boolean_t noThreadAPI_active;									\
														\
	char	msgbuf[YDB_MAX_ERRORMSG];									\
	mstr	msg;												\
	int	errNum, lclStatus;										\
														\
	if (NULL != ERRSTR)											\
	{													\
		assert(simpleThreadAPI_active || !ydb_init_complete);						\
		/* The below code is similar to that in ydb_mesage.c */						\
		msg.len = SIZEOF(msgbuf);									\
		msg.addr = msgbuf;										\
		errNum = abs(ERRNUM);										\
		lclStatus = gtm_getmsg(errNum, &msg);								\
		if (ERR_UNKNOWNSYSERR == lclStatus)								\
		{	/* Unknown message. Just null terminate it */						\
			msg.addr[0] = '\0';									\
		} else												\
			assert('\0' == msg.addr[msg.len]);	/* assert null termination */			\
		/* Copy message to user's buffer depending on available room.					\
		 * Note: Cannot use SNPRINTF as it is not multi-thread-safe (invokes "eintr_handling_check()"	\
		 * or "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL").							\
		 */												\
		snprintf((ERRSTR)->buf_addr, (ERRSTR)->len_alloc, "%d,%s,%s", ERRNUM,				\
			 (noThreadAPI_active ? SIMPLEAPI_M_ENTRYREF : SIMPLETHREADAPI_M_ENTRYREF), msg.addr);	\
	}													\
}

/* This macro negates the positive return value from the "ydb_ci_exec()" function call.
 * Used by "ydb_ci()", "ydb_cip()", "ydb_ci_t()", "ydb_cip_t()" so they mostly return a negative value in case of errors
 *	(exceptions are YDB_TP_RESTART etc. which are huge positive numbers) that way staying compatible with general
 *	SimpleAPI/SimpleThreadAPI function behavior on error returns.
 * Note: This is not used by "gtm_ci()" or "gtm_cip()" so those functions continue to return a positive value in case of errors
 *	and staying compatible with GT.M behavior (TP restart returns ERR_TPRETRY and not YDB_TP_RESTART).
 */
#define	CONVERT_YDB_CI_EXEC_TO_SIMPLEAPI_RETVAL(RETVAL)				\
{										\
	GBLREF	uint4		dollar_tlevel;					\
										\
	assert(0 <= RETVAL);							\
	RETVAL = ((ERR_TPRETRY == RETVAL) ? YDB_TP_RESTART : -RETVAL);		\
	assert(dollar_tlevel || (YDB_TP_RESTART != RETVAL));			\
}

/* Macro to copy $ZSTATUS into an errstr buffer. If an error has occurred and we need the text currently residing in
 * $ZSTATUS to be copied to our errstr buffer (if errstr is not NULL), copy the text to the errstr buffer. Note there
 * are certain "status errors" that are internal and don't get returned to the user that just simply have no text
 * (e.g. TPRETRY, JOBINTRRETHROW, JOBINTRRQST) and others that are return code only (no text). In those cases, this
 * macro could copy a leftover value (from a previous error) of $ZSTATUS into errstr. But that is considered okay for
 * now because errstr is not guaranteed to be initialized for those error codes and so the caller is not supposed to
 * be looking at errstr in those cases anyways. For example, if YDB_TP_RESTART is returned, the caller is supposed to
 * be forwarding this status code to its caller as is.
 */
#define SET_ERRSTR_FROM_ZSTATUS_IF_NOT_NULL(ERRSTR)									\
MBSTART	{														\
	GBLREF mval dollar_zstatus;											\
	assert((ERRSTR) == TREF(stapi_errstr));										\
	if (NULL != (ERRSTR))												\
	{	/* We are inside a SimpleThreadAPI call and user wants us to fill error string in addition to returning	\
		 * an integer error code. Set the text part here after $zstatus has been filled in.			\
		 */													\
		assert(simpleThreadAPI_active);										\
		ydb_zstatus((ERRSTR)->buf_addr, (ERRSTR)->len_alloc);							\
		/* The call to ydb_zstatus() won't set the message unless len_alloc is more than one byte.		\
		 * Note that the returned buffer (in errstr->buf_addr) is null terminated so we can just set		\
		 * the output length equal to the actual length of the error string.					\
		 * If errstr->len_used > errstr->len_alloc - 1, then user knows they got a truncated error string	\
		 * and need to pass in a bigger buffer to avoid truncation in future SimpleThreadAPI calls.		\
		 */													\
		(ERRSTR)->len_used = dollar_zstatus.str.len;								\
	}														\
} MBEND

/* Define routines before get to inline routine definitions */
int	sapi_return_subscr_nodes(int *ret_subs_used, ydb_buffer_t *ret_subsarray, char *ydb_caller_fn);
void	sapi_save_targ_key_subscr_nodes(void);
void	*ydb_stm_thread(void *parm);
int	ydb_tp_s_common(libyottadb_routines lydbrtn,
			ydb_basicfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, const ydb_buffer_t *varnames);
int	ydb_lock_s_va(unsigned long long timeout_nsec, int namecount, va_list var);
void	ydb_nested_callin(void);
void	ydb_stm_thread_exit(void);

/* Below are the 3 functions invoked by "pthread_atfork" during a "fork" call to ensure all SimpleThreadAPI related
 * mutex and condition variables are safely released (without any deadlocks, inconsistent states) in the child after the fork.
 */
void	ydb_stm_atfork_prepare(void);
void	ydb_stm_atfork_parent(void);
void	ydb_stm_atfork_child(void);

/* Declarations for routines used by Simple API */
unsigned int get_value_and_subtree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
	ydb_var_types data_type, char *ydb_caller_fn);
void ydb_get_value(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_buffer_t *ret_value,
	ydb_var_types get_type, int get_svn_index, boolean_t only_return_size, char *ydb_caller_fn);
int get_next_node(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, int *ret_subs_used,
	ydb_buffer_t *ret_subsarray, ydb_var_types nodenext_type, char *ydb_caller_fn);
void ydb_set_value(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, const ydb_buffer_t *value,
	ydb_var_types set_type, int set_svn_index, char *ydb_caller_fn);
void	ydb_issue_invvarname_error(const ydb_buffer_t *varname);

/* Use NO_THREADGBL_DEFTYPES here to keep these inline routines from expanding in gtm_threadgbl_deftypes.c where they are
 * unnecessary and cause build issues of the gtm_threadgbl_deftypes executable that builds gtm_threadgbl_deftypes.h.
 */
#ifndef NO_THREADGBL_DEFTYPES
/* Inline func invoked by all ydb_*_st() and ydb_*_t() functions to get the appropriate multi-thread safe mutex and return.
 * TPTOKEN, ERRSTR, CALLTYP are input parameters passed in from the caller function.
 * SAVE_ACTIVE_STAPI_RTN, SAVE_ERRSTR, GET_LOCK, RETVAL are output parameters from this macro. Except for RETVAL,
 * the rest of the output parameters will need to be later passed as is to threaded_api_ydb_engine_unlock (i.e. at unlock time).
 */
static inline void threaded_api_ydb_engine_lock(uint64_t TPTOKEN, ydb_buffer_t *ERRSTR, int CALLTYP,
						libyottadb_routines *SAVE_ACTIVE_STAPI_RTN, ydb_buffer_t **SAVE_ERRSTR,
						boolean_t *GET_LOCK, int *RETVAL)
{
	GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
	GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];
	GBLREF	boolean_t	simpleThreadAPI_active;
	GBLREF	pthread_t	ydb_stm_worker_thread_id;
	GBLREF	uint4		dollar_tlevel;
	GBLREF	uint64_t 	stmTPToken;	/* Counter used to generate unique token for SimpleThreadAPI TP */

	int		i, lockIndex, lclStatus, tLevel;
	uint64_t	tpToken;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGSIGHND_ONLY(fprintf(stderr, "threaded_api_ydb_engine_lock: Obtaining lock for %s - tptoken: %"PRIu64",  "
			       "save area 0x%p\n", LYDBRTNNAME(CALLTYP), TPTOKEN, ERRSTR); fflush(stderr));
	*RETVAL = YDB_OK;
	*GET_LOCK = TRUE;
	/* Since this macro can be called from "ydb_init" as part of opening YottaDB for the first time in the process,
	 * we need to handle the case where "lcl_gtm_threadgbl" is NULL in which case we should skip TREF usages.
	 */
	*SAVE_ACTIVE_STAPI_RTN = (NULL != lcl_gtm_threadgbl) ? TREF(libyottadb_active_rtn) : LYDB_RTN_NONE;
	lockIndex = GET_TPDEPTH_FROM_TPTOKEN((uint64_t)TPTOKEN);
	/* Check for INVTPTRANS error. Note that we cannot detect ALL possible cases of an invalid supplied tptoken.
	 * For example, if the TP callback function of a $TLEVEL=2 TP does a "ydb_get_st" call with a TPTOKEN that
	 * has high order 8 bits set to 1, and low order 56 bits set to same as the tptoken passed in the callback
	 * function, then it won't fail with an INVTPTRANS error even though the "ydb_get_st" call will deadlock
	 * since it will be waiting to get the ydb engine thread lock on index 1 whereas that cannot happen until the
	 * $TLEVEL=2 TP callback function finishes which again cannot finish until the "ydb_get_st" call finishes.
	 */
	if (YDB_NOTTP != TPTOKEN)
	{
		tLevel = dollar_tlevel;
		tpToken = GET_INTERNAL_TPTOKEN((uint64_t)TPTOKEN);
		if (!tLevel || (tpToken != stmTPToken) || !lockIndex || (lockIndex > tLevel))
		{
			SETUP_GENERIC_ERROR(YDB_ERR_INVTPTRANS);
			*RETVAL = YDB_ERR_INVTPTRANS;
		}
	}
	if (ydb_engine_threadsafe_mutex_holder[lockIndex] == pthread_self())
	{	/* This thread already holds this lock - check for special conditions */
		if (LYDB_RTN_NONE != *SAVE_ACTIVE_STAPI_RTN)
		{	/* We are already in the middle of a SimpleThreadAPI call. Since we already hold the mutex lock as
			 * part of the original SimpleThreadAPI call, skip lock get/release in this nested call.
			 */
			if (LYDB_RTN_TP == CALLTYP)
			{	/* Disallow starting a new TP transaction */
				SETUP_GENERIC_ERROR_2PARMS(YDB_ERR_SIMPLEAPINEST, LYDBRTNNAME(*SAVE_ACTIVE_STAPI_RTN),
							   LYDBRTNNAME(CALLTYP));
				*RETVAL = YDB_ERR_SIMPLEAPINEST;
			} else
			{
				if ((LYDB_RTN_YDB_CI == *SAVE_ACTIVE_STAPI_RTN) || (LYDB_RTN_YDB_CIP == *SAVE_ACTIVE_STAPI_RTN))
				{	/* We are the thread that started a "ydb_ci_t"/"ydb_cip_t" call. And that same thread
					 * has done an external call in the call-in M code which in turn wants to do the
					 * current SimpleThreadAPI function call. Allow all calls while inside the call-in by
					 * shutting off the active rtn indicator for the duration of this SimpleThreadAPI call.
					 */
					assert(NULL != lcl_gtm_threadgbl);
					TREF(libyottadb_active_rtn) = LYDB_RTN_NONE;
				}
				/* else: It is possible we are the thread that started a "ydb_set_st"/"ydb_kill_st" which
				 * invoked a trigger M code that did an external call and the C code reinvoked the current
				 * SimpleThreadAPI function. In this case, we allow all calls but do not reset the active rtn
				 * indicator. The corresponding SimpleAPI function invocation will issue the needed
				 * SIMPLEAPINEST error. The reason we do not issue the error here is because CALLTYP would most
				 * likely be LYDB_RTN_NONE at this point if this is a "ydb_init" call (a utility function).
				 * In that case, deferring the error would give us a more accurate SIMPLEAPINEST error message
				 * when the non-utility SimpleAPI function is made a little later.
				 */
				*GET_LOCK = FALSE;
			}
		}
	}
	if (YDB_OK == *RETVAL)
	{
		if (*GET_LOCK)
		{
			*RETVAL = pthread_mutex_lock(&ydb_engine_threadsafe_mutex[lockIndex]);
			assert(0 == YDB_OK);
			if (0 == *RETVAL)
			{
				ydb_engine_threadsafe_mutex_holder[lockIndex] = pthread_self();
				/* Mark this process as having SimpleThreadAPI active if not already done */
				if (!simpleThreadAPI_active && (LYDB_RTN_NONE != CALLTYP))
				{	/* The LYDB_RTN_NONE check above is needed to take into account calls of this
					 * macro from "ydb_init" which are done even in SimpleAPI mode. Those calls
					 * should not mark SimpleThreadAPI as active.
					 */
					assert(0 == lockIndex);
					/* Start the signal thread - in normal signal handling, this thread handles all of the
					 * SIGALRM signals and many of the other signals if they get forwarded. In alternate
					 * signal handling mode, this thread makes sure all queued pending signals get driven
					 * by polling for them. It gets awakened by a YDBSIGNOTIFY signal when a pending signal
					 * is queued.
					 */
					lclStatus = pthread_create(&ydb_stm_worker_thread_id, NULL, &ydb_stm_thread, NULL);
					if (0 == lclStatus)
					{
						/* Wait for STAPI signal thread to set "simpleThreadAPI_active" */
						for (i = 0; ; i++)
						{
							if (simpleThreadAPI_active)
								break;
							if (MICROSECS_IN_SEC == i)
							{	/* Worker thread did not reach desired state in given
								 * time. Treat this as if "pthread_create" call failed.
								 */
								lclStatus = ETIMEDOUT;
								break;
							}
							SLEEP_USEC(1, TRUE);
						}
					}
					if (0 != lclStatus)
					{	/* If lclStatus is non-zero, we do have the YottaDB engine lock so
						 * we CAN call "rts_error_csa" etc. therefore do just that.
						 */
						assert(FALSE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							      RTS_ERROR_LITERAL("pthread_create()"), CALLFROM, lclStatus);
					}
				}
			}
			/* If *RETVAL is non-zero, we do not have the YottaDB engine lock so we cannot call
			 * "rts_error_csa" etc. therefore just silently return this error code to caller.
			 */
		}
		if (YDB_OK == *RETVAL)
		{
			if (NULL != lcl_gtm_threadgbl)
			{
				*SAVE_ERRSTR = TREF(stapi_errstr);
				TREF(stapi_errstr) = ERRSTR;	/* Set this so "ydb_simpleapi_ch" can fill in
								 * error string in case error is seen.
								 */
			} else
				*SAVE_ERRSTR = NULL;
		}
	}
	DBGSIGHND((stderr, "threaded_api_ydb_engine_lock: Lock granted for %s with: tptoken %"PRIu64", save area 0x%p\n",
		   LYDBRTNNAME(CALLTYP), TPTOKEN, ERRSTR));
	/* Now that we have the engine lock, see if there are any pending (alternate) signals waiting to be processed */
	PROCESS_PENDING_ALTERNATE_SIGNALS;
}

static inline void threaded_api_ydb_engine_unlock(uint64_t TPTOKEN, ydb_buffer_t *ERRSTR,
						  libyottadb_routines SAVE_ACTIVE_STAPI_RTN, ydb_buffer_t *SAVE_ERRSTR,
						  boolean_t RELEASE_LOCK)
{
	GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
	GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];

	int	lockIndex, lclStatus;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGSIGHND((stderr, "threaded_api_ydb_engine_unlock: Releasing lock for tptoken: %"PRIu64", save area 0x%p\n", TPTOKEN, ERRSTR));
	/* Before releasing the YottaDB engine lock, check if there is a pending signal that needs to be processed
	 * while we hold the engine lock.
	 */
	PROCESS_PENDING_ALTERNATE_SIGNALS;
	/* Also before releasing the YottaDB engine lock, check if any signal handler got deferred in the MAIN
	 * worker thread and is still pending. If so, handle it now since we own the engine lock for sure here
	 * and it is a safe logical point.
	 */
	STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED(OK_TO_NEST_TRUE);
	/* Since this macro can be called from "ydb_init" as part of opening YottaDB for the first time in the process,
	 * we need to handle the case where "lcl_gtm_threadgbl" is NULL in which case we should not skip TREF usages.
	 */
	/* Whether RELEASE_LOCK is TRUE or FALSE, undo set to TREF(stapi_errstr) done in "threaded_api_ydb_engine_lock()". */
	if (NULL != lcl_gtm_threadgbl)
	{
		assert(ERRSTR == TREF(stapi_errstr));
		TREF(stapi_errstr) = SAVE_ERRSTR;
	}
	if (RELEASE_LOCK)
	{
		lockIndex = GET_TPDEPTH_FROM_TPTOKEN((uint64_t)TPTOKEN);
		assert(pthread_self() == ydb_engine_threadsafe_mutex_holder[lockIndex]);
		ydb_engine_threadsafe_mutex_holder[lockIndex] = 0;
		lclStatus = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[lockIndex]);
		if (lclStatus)
		{	/* If lclStatus is non-zero, we do have the YottaDB engine lock so we CAN call
			 * "rts_error_csa" etc. therefore do just that.
			 */
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("pthread_mutex_unlock()"), CALLFROM, lclStatus);
		}
	} else if ((LYDB_RTN_YDB_CI == SAVE_ACTIVE_STAPI_RTN) || (LYDB_RTN_YDB_CIP == SAVE_ACTIVE_STAPI_RTN))
	{	/* Undo active rtn indicator reset done in threaded_api_ydb_engine_lock */
		if (NULL != lcl_gtm_threadgbl)
			TREF(libyottadb_active_rtn) = SAVE_ACTIVE_STAPI_RTN;
	}
}
#endif /* !NO_THREADGBL_DEFTYPES */

#endif /*  LIBYOTTADB_INT_H */
