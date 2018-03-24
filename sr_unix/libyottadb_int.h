/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
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

#include "libyottadb.h"
#include "ydbmerrors.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "compiler.h"	/* needed for funsvn.h */
#include "funsvn.h"
#include "error.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "stack_frame.h"
#include "lv_val.h"
#include "libydberrors.h"	/* Define YDB_ERR_* errors */
#include "gtm_string.h"		/* for strlen in LIBYOTTADB_INIT in RTS_ERROR_TEXT macro */
#include "setup_error.h"

#define MAX_SAPI_MSTR_GC_INDX	YDB_MAX_NAMES

GBLREF	symval		*curr_symval;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		process_id;

LITREF	char		ctypetab[NUM_CHARS];
LITREF	nametabent	svn_names[];
LITREF	unsigned char	svn_index[];
LITREF	svn_data_type	svn_data[];
LITREF	int		lydbrtnpkg[];
LITREF	char 		*lydbrtnnames[];

#define LYDB_NONE	0				/* Routine is part of no package */
#define LYDB_UTILITY 	1				/* Routine is a utility routine */
#define LYDB_SIMPLEAPI	2				/* Routine is part of the simpleAPI */

/* Values for TREF(libyottadb_active_rtn) */
#define LYDBRTN(a, b, c) a
typedef enum
{
#include "libyottadb_rtns.h"
} libyottadb_routines;
#undef LYDBRTN

/* Returned values for VARTYPE in VALIDATE-VARNAME() macro */
typedef enum
{
	LYDB_VARREF_GLOBAL = 1,		/* Referencing a global variable */
	LYDB_VARREF_LOCAL,		/* Referencing a local variable */
	LYDB_VARREF_ISV			/* Referencing an ISV (Intrinsic Special Variable) */
} ydb_var_types;

/* Initialization and cleanup macros for main simpleAPI calls. Having a value in TREF(libyottadb_active_rtn)
 * denotes a simpleAPI routine is active and no other simpleAPI routine can be started - i.e. calls cannot be
 * nested with the exception of ydb_tp_s() which clears this indicator just before it engages the call-back
 * routine which has a high probability of calling more simpleAPI routines.
 */
#define LIBYOTTADB_INIT(ROUTINE)										\
MBSTART	{													\
	int		status, errcode;									\
	mstr		entryref;										\
														\
	GBLREF	stack_frame	*frame_pointer;									\
	GBLREF 	boolean_t	gtm_startup_active;								\
														\
	/* A prior invocation of ydb_exit() would have set process_exiting = TRUE. Use this to disallow further	\
	 * API calls.	      	 	    	       	   		     	       	       			\
	 */    													\
	if (process_exiting)											\
	{	/* YDB runtime environment not setup/available, no driving of errors */				\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);					\
		return YDB_ERR_CALLINAFTERXIT;									\
	}													\
	/* No threadgbl usage in this macro until the following block completes */				\
	if (!gtm_startup_active || !(frame_pointer->type & SFT_CI))						\
	{	/* Have to initialize things before we can establish an error handler */			\
		if (0 != (status = ydb_init()))		/* Note - sets fgncal_stack */				\
			return -status;			   	       		    				\
		/* Since we called "ydb_init" above, "gtm_threadgbl" would have been set to a non-null VALUE	\
		 * and so any call to SETUP_THREADGBL_ACCESS done by the function that called this macro	\
		 * needs to be redone to set "lcl_gtm_threadgbl" to point to this new "gtm_threadgbl".		\
		 */												\
		SETUP_THREADGBL_ACCESS;										\
	}		       											\
	/* Verify simpleAPI routines are not nesting. If we detect a problem here, the routine has not yet	\
	 * established the condition handler to take care of these issues so we simulate it's effect by		\
	 * doing the "set_zstatus", setting TREF(ydb_error_code) and returning the error code.			\
	 */													\
	if ((LYDB_RTN_NONE != TREF(libyottadb_active_rtn)) && (LYDB_SIMPLEAPI == lydbrtnpkg[ROUTINE]))		\
	{													\
		errcode = ERR_SIMPLEAPINEST;									\
		setup_error(CSA_ARG(NULL) VARLSTCNT(6) ERR_SIMPLEAPINEST, 4,					\
				RTS_ERROR_TEXT(lydbrtnnames[TREF(libyottadb_active_rtn)]),			\
				RTS_ERROR_TEXT(lydbrtnnames[ROUTINE]));						\
		entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
		entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
		set_zstatus(&entryref, errcode, NULL, FALSE);							\
		TREF(ydb_error_code) = errcode;									\
		return YDB_ERR_SIMPLEAPINEST;									\
	}													\
	TREF(libyottadb_active_rtn) = ROUTINE;									\
} MBEND

#define LIBYOTTADB_DONE	TREF(libyottadb_active_rtn) = LYDB_RTN_NONE

/* Macros to promote checking an mname as being a valid mname. There are two flavors - one where
 * the first character needs to be checked and one where the first character has already been
 * checked and no further checking needs to be done.
 */

/* A macro to check the entire MNAME for validity. Returns YDB_ERR_INVVARNAME otherwise */
#define VALIDATE_MNAME_C1(VARNAMESTR, VARNAMELEN)						\
MBSTART {											\
	char ctype;										\
	     											\
	assert(0 < (VARNAMELEN));								\
	ctype = ctypetab[(VARNAMESTR)[0]];							\
	switch(ctype)										\
	{											\
		case TK_LOWER:									\
		case TK_UPPER:									\
		case TK_PERCENT: 								\
			/* Valid first character */						\
			break;	       		 						\
		default:									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);		\
	}											\
	VALIDATE_MNAME_C2((VARNAMESTR) + 1, (VARNAMELEN) - 1);					\
} MBEND

/* Validate the 2nd char through the end of a given MNAME for validity returning YDB_ERR_INVVARNAME otherwise */
#define VALIDATE_MNAME_C2(VARNAMESTR, VARNAMELEN)						\
MBSTART {											\
	char 		ctype, *cptr, *ctop;							\
	signed char	ch;	      								\
	       											\
	assert(0 <= (VARNAMELEN));								\
	for (cptr = (VARNAMESTR), ctop = (VARNAMESTR) + (VARNAMELEN); cptr < ctop; cptr++)	\
	{	    			     	    			  		      	\
		ctype = ctypetab[*cptr];							\
		switch(ctype)									\
		{										\
			case TK_LOWER:								\
			case TK_UPPER:								\
			case TK_DIGIT:								\
				continue;							\
			default:								\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);	\
		}		       								\
	}											\
} MBEND

/* A macro for verifying an input variable name. This macro checks for:
 *   - Non-zero length
 *   - Non-NULL address
 *   - Determines the type of var (global, local, ISV)
 *
 * Any error in validation results in a return with code YDB_ERR_INVVARNAME.
 */
#define VALIDATE_VARNAME(VARNAMEP, VARTYPE, VARINDEX, UPDATE)								\
MBSTART {														\
	char	ctype;													\
	int	index, lenUsed;												\
															\
	if (IS_INVALID_YDB_BUFF_T(VARNAMEP))										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);						\
	lenUsed = (VARNAMEP)->len_used;											\
	if (0 == lenUsed)												\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);						\
	/* Characterize first char of name ($, ^, %, or letter) */							\
	ctype = ctypetab[(VARNAMEP)->buf_addr[0]];									\
	switch(ctype)													\
	{														\
		case TK_CIRCUMFLEX:											\
			lenUsed--;											\
			if (YDB_MAX_IDENT < lenUsed)									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);		\
			if (0 == lenUsed)										\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);				\
			VARTYPE = LYDB_VARREF_GLOBAL;									\
			VALIDATE_MNAME_C1((VARNAMEP)->buf_addr + 1, lenUsed);						\
			break;				      	   		      					\
		case TK_LOWER:												\
		case TK_UPPER:												\
		case TK_PERCENT: 											\
			if (YDB_MAX_IDENT < lenUsed)									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_VARNAME2LONG, 1, YDB_MAX_IDENT);		\
			VARTYPE = LYDB_VARREF_LOCAL;									\
			VALIDATE_MNAME_C2((VARNAMEP)->buf_addr + 1, lenUsed - 1);					\
			break;												\
		case TK_DOLLAR:												\
			lenUsed--;											\
			if (0 == lenUsed)										\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);				\
			VARTYPE = LYDB_VARREF_ISV;									\
			index = namelook(svn_index, svn_names, (VARNAMEP)->buf_addr + 1, lenUsed); 			\
			if (-1 == index) 	     			     						\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVSVN);					\
			if (UPDATE && !svn_data[index].can_set)								\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SVNOSET);					\
			VARINDEX = svn_data[index].opcode;								\
			break;	   											\
		default:												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVVARNAME);					\
	}		       												\
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

#define	ERR_LVUNDEF_OK_FALSE	FALSE
#define	ERR_LVUNDEF_OK_TRUE	TRUE

/* Now the NOUPD version */
#define FIND_BASE_VAR_NOUPD(VARNAMEP, VARMNAMEP, VARTABENTP, VARLVVALP, ERR_LVUNDEF_OK)						\
MBSTART {															\
	lv_val		*lv;													\
																\
	(VARMNAMEP)->var_name.addr = (VARNAMEP)->buf_addr;	/* Load up the imbedded varname with our base var name */	\
	(VARMNAMEP)->var_name.len = (VARNAMEP)->len_used;	   	       			     	      	       		\
	(VARMNAMEP)->marked = 0;												\
	COMPUTE_HASH_MNAME((VARMNAMEP));			/* Compute its hash value */					\
	(VARTABENTP) = lookup_hashtab_mname(&curr_symval->h_symtab, (VARMNAMEP));						\
	if ((NULL == (VARTABENTP)) || (NULL == (lv_val *)((VARTABENTP)->value)))						\
	{															\
		if (ERR_LVUNDEF_OK)												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVUNDEF);							\
		else														\
			VARLVVALP = NULL;											\
	} else															\
	{															\
		assert(NULL != LV_GET_SYMVAL((lv_val *)((VARTABENTP)->value)));							\
		VARLVVALP = (VARTABENTP)->value;										\
	}															\
} MBEND

/* Macro to check whether input to ydb_*_s() function from the user is a valid "ydb_buffer_t" structure */
#define	IS_INVALID_YDB_BUFF_T(YDBBUFF)	(((YDBBUFF)->len_alloc < (YDBBUFF)->len_used)				\
					 || ((NULL == (YDBBUFF)->buf_addr) && (0 != (YDBBUFF)->len_used)))

/* Macro to set a supplied ydb_buffer_t value into an mval */
#define SET_MVAL_FROM_YDB_BUFF_T(MVALP, YDBBUFF)							\
MBSTART	{												\
	assert(!IS_INVALID_YDB_BUFF_T(YDBBUFF)); /* caller should have issued error appropriately */	\
	(MVALP)->mvtype = MV_STR;									\
	(MVALP)->str.addr = (YDBBUFF)->buf_addr;							\
	(MVALP)->str.len = (YDBBUFF)->len_used;								\
} MBEND

/* Macro to set a supplied ydb_buffer_t value from a supplied mval.
 * PARAM1 and PARAM2 are parameters supplied to the PARAMINVALID error if it needs to be issued.
 */
#define SET_YDB_BUFF_T_FROM_MVAL(YDBBUFF, MVALP, PARAM1, PARAM2)				\
MBSTART	{											\
	mval		*sRC;	/* named so to avoid name collision with caller */		\
	ydb_buffer_t	*dST;	/* named so to avoid name collision with caller */		\
												\
	sRC = MVALP;										\
	/* It is possible source mval is not of type MV_STR. But ydb_buff_t needs one		\
	 * so convert incoming mval to a string and then copy it, if needed.			\
	 */											\
	MV_FORCE_STR(sRC);									\
	assert(MV_IS_STRING(sRC));								\
	dST = YDBBUFF;										\
	if (sRC->str.len > dST->len_alloc)							\
	{											\
		dST->len_used = sRC->str.len;	/* Set len to what it needed to be */		\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVSTRLEN);			\
	}											\
	if (sRC->str.len)									\
	{											\
		if (NULL == dST->buf_addr)							\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,	\
						LEN_AND_LIT(PARAM1), LEN_AND_LIT(PARAM2));	\
		memcpy(dST->buf_addr, sRC->str.addr, sRC->str.len);				\
	}											\
	dST->len_used = sRC->str.len;								\
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
	char		buff[256];										\
				 										\
	subs = (ydb_buffer_t *)SUBSARRAY;	 								\
	if ((COUNT) && (NULL == subs))										\
	{       /* count of subscripts is non-zero but no subscript specified - error */			\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SUBSARRAYNULL, 3, (COUNT), LEN_AND_LIT(PARAM2));	\
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
			SPRINTF(buff, "Invalid subsarray (index %d)", subs - ((ydb_buffer_t *)SUBSARRAY));	\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,				\
				      LEN_AND_STR(buff), LEN_AND_LIT(PARAM2));					\
		}												\
		CHECK_MAX_STR_LEN(subs);									\
		SET_MVAL_FROM_YDB_BUFF_T(mvalp, subs);	/* Generates error if subscript too long */		\
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
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, (YDBBUFF)->len_used, YDB_MAX_STR);	\
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
 * runtime call (i.e. op_*), then this macro is accurate.
 */
#define IS_SIMPLEAPI_MODE (frame_pointer->type & SFT_CI)

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

void sapi_return_subscr_nodes(int *ret_subs_used, ydb_buffer_t *ret_subsarray, char *ydb_caller_fn);
void sapi_save_targ_key_subscr_nodes(void);

#endif /*  LIBYOTTADB_INT_H */
