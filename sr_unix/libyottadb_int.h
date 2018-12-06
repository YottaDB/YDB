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

#define MAX_SAPI_MSTR_GC_INDX	YDB_MAX_NAMES

GBLREF	symval		*curr_symval;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		process_id;
GBLREF 	boolean_t	ydb_init_complete;

LITREF	char		ctypetab[NUM_CHARS];
LITREF	nametabent	svn_names[];
LITREF	unsigned char	svn_index[];
LITREF	svn_data_type	svn_data[];
LITREF	int		lydbrtnpkg[];
LITREF	char 		*lydbrtnnames[];

#define THREADED_STR	"threaded"
#define UNTHREADED_STR	"un-threaded"
#define THREADED_STR_LEN (SIZEOF(THREADED_STR) - 1)
#define UNTHREADED_STR_LEN (SIZEOF(UNTHREADED_STR) - 1)

#define LYDB_NONE	0				/* Routine is part of no package */
#define LYDB_UTILITY 	1				/* Routine is a utility routine */
#define LYDB_SIMPLEAPI	2				/* Routine is part of the simpleAPI */

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
#define LYDBRTN(a, b, c) a
typedef enum
{
#include "libyottadb_rtns.h"
	, LYDB_RTN_TPCOMPLT		/* Used in stm_que_ent to denote this TP level is complete */
} libyottadb_routines;
#undef LYDBRTN

/* Returned values for VARTYPE in VALIDATE-VARNAME() macro */
typedef enum
{
	LYDB_VARREF_GLOBAL = 1,		/* Referencing a global variable */
	LYDB_VARREF_LOCAL,		/* Referencing a local variable */
	LYDB_VARREF_ISV			/* Referencing an ISV (Intrinsic Special Variable) */
} ydb_var_types;

/* Structure for isolating YottaDB SimpleAPI calls in its own thread. This is the queue entry that conveys
 * the call from one thread to another. Possible queues are the queue array anchored at stmWorkQueue and the
 * special TP work queue stmTPWorkQueue both of which are defined in gbldefs.c and described in more detail there.
 */
typedef struct stm_que_ent_struct				/* SimpleAPI Thread Model */
{
	struct
	{
		struct stm_que_ent_struct	*fl, *bl;	/* Doubly linked chain for free/busy queue */
	} que;
	sem_t			complete;			/* When this is unlocked, it is complete */
	libyottadb_routines	calltyp;			/* Which routine's call is being migrated */
	uintptr_t		args[YDB_MAX_SAPI_ARGS];	/* Args that are moving over */
	uintptr_t		retval;				/* Return value coming back */
#	ifdef DEBUG
	char			*mainqcaller, *tpqcaller;	/* Where queued from */
#	endif
} stm_que_ent;

/* Structure that provides the working structures for SimpleThreadAPI access (running YottaDB requests in a known thread).
 * Note we try to keep the mutexes, and queue headers in separate cachelines so conditional load/store work correctly.
 */
typedef struct
{
	pthread_mutex_t		mutex;				/* Mutex controlling access to queue and CV */
	CACHELINE_PAD(SIZEOF(pthread_mutex_t), 1);
	pthread_cond_t		cond;				/* Worker notifier something may be on queue */
	CACHELINE_PAD(SIZEOF(pthread_cond_t), 2);
	stm_que_ent		stm_wqhead;			/* Work queue head/tail */
	/* TODO SEE - checking for threadid of 0 is verbotten - create a new field that says it is set or not */
	pthread_t		threadid;			/* Thread associated with the queue */
	uintptr_t		tptoken;			/* TP token for this TP level for given transaction */
} stm_workq;

/* Structure to hold the free stm_que_ent entries for re-use. Again, keeping parts in separate cache lines to minimize
 * interference between them.
 */
typedef struct
{
	pthread_mutex_t 	mutex;				/* Mutex controlling access to free queues */
	CACHELINE_PAD(SIZEOF(pthread_mutex_t), 1);
	stm_que_ent		stm_cbqhead;			/* Head element of free callblk queue */
} stm_freeq;

/* Create a common-ground type between ydb_tpfnptr_t and ydb_tp2fnptr_t by removing the arguments so ydb_tp_common can be
 * called with either one of them. It's just a basic function pointer.
 */
typedef int (*ydb_basicfnptr_t)();

/* Macros to startup YottaDB runtime if it is not going yet - one for routines with return values, one for not */
#define LIBYOTTADB_RUNTIME_CHECK(RETTYPE)									\
MBSTART	{													\
	int	status;												\
														\
	/* No threadgbl usage in this macro until the following block completes */				\
	if (!ydb_init_complete || !(frame_pointer->type & SFT_CI))						\
	{	/* Have to initialize things before we can establish an error handler */			\
		if (0 != (status = ydb_init()))		/* Note - sets fgncal_stack */				\
			return RETTYPE -status;									\
		/* Since we called "ydb_init" above, "gtm_threadgbl" would have been set to a non-null VALUE	\
		 * and so any call to SETUP_THREADGBL_ACCESS done by the function that called this macro	\
		 * needs to be redone to set "lcl_gtm_threadgbl" to point to this new "gtm_threadgbl".		\
		 */												\
		SETUP_THREADGBL_ACCESS;										\
	}		       											\
} MBEND

/* Macros to startup YottaDB runtime if it is not going yet - one for returns, one for not */
#define LIBYOTTADB_RUNTIME_CHECK_NORETVAL									\
MBSTART	{													\
	int	status;												\
														\
	/* No threadgbl usage in this macro until the following block completes */				\
	if (!ydb_init_complete || !(frame_pointer->type & SFT_CI))						\
	{	/* Have to initialize things before we can establish an error handler */			\
		if (0 != (status = ydb_init()))		/* Note - sets fgncal_stack */				\
			return;											\
		/* Since we called "ydb_init" above, "gtm_threadgbl" would have been set to a non-null VALUE	\
		 * and so any call to SETUP_THREADGBL_ACCESS done by the function that called this macro	\
		 * needs to be redone to set "lcl_gtm_threadgbl" to point to this new "gtm_threadgbl".		\
		 */												\
		SETUP_THREADGBL_ACCESS;										\
	}		       											\
} MBEND

/* Initialization and cleanup macros for main simpleAPI calls. Having a value in TREF(libyottadb_active_rtn)
 * denotes a simpleAPI routine is active and no other simpleAPI routine can be started - i.e. calls cannot be
 * nested with the exception of ydb_tp_s() which clears this indicator just before it engages the call-back
 * routine which has a high probability of calling more simpleAPI routines.
 *
 * We need two flavors - one for use in routines with a return value and one without.
 */
#define LIBYOTTADB_INIT(ROUTINE, RETTYPE)									\
MBSTART	{													\
	int		status, errcode;									\
	mstr		entryref;										\
														\
	LIBYOTTADB_RUNTIME_CHECK(RETTYPE);									\
	/* Verify simpleAPI routines are not nesting. If we detect a problem here, the routine has not yet	\
	 * established the condition handler to take care of these issues so we simulate it's effect by		\
	 * doing the "set_zstatus", setting TREF(ydb_error_code) and returning the error code.			\
	 */													\
	if (LYDB_RTN_NONE != TREF(libyottadb_active_rtn))							\
	{													\
		errcode = ERR_SIMPLEAPINEST;									\
		setup_error(CSA_ARG(NULL) VARLSTCNT(6) ERR_SIMPLEAPINEST, 4,					\
				RTS_ERROR_TEXT(lydbrtnnames[TREF(libyottadb_active_rtn)]),			\
				RTS_ERROR_TEXT(lydbrtnnames[ROUTINE]));						\
		entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
		entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
		set_zstatus(&entryref, errcode, NULL, FALSE);							\
		TREF(ydb_error_code) = errcode;									\
		return RETTYPE YDB_ERR_SIMPLEAPINEST;								\
	}													\
	TREF(libyottadb_active_rtn) = ROUTINE;									\
	DBGAPI((stderr, "Entering routine %s\n", lydbrtnnames[ROUTINE]));					\
} MBEND

/* And now for the no return value edition */
#define LIBYOTTADB_INIT_NORETVAL(ROUTINE)									\
MBSTART	{													\
	int		status, errcode;									\
	mstr		entryref;										\
														\
	LIBYOTTADB_RUNTIME_CHECK_NORETVAL;									\
	/* Verify simpleAPI routines are not nesting. If we detect a problem here, the routine has not yet	\
	 * established the condition handler to take care of these issues so we simulate it's effect by		\
	 * doing the "set_zstatus", setting TREF(ydb_error_code) and returning the error code.			\
	 */													\
	if (LYDB_RTN_NONE != TREF(libyottadb_active_rtn))							\
	{													\
		errcode = ERR_SIMPLEAPINEST;									\
		setup_error(CSA_ARG(NULL) VARLSTCNT(6) ERR_SIMPLEAPINEST, 4,					\
				RTS_ERROR_TEXT(lydbrtnnames[TREF(libyottadb_active_rtn)]),			\
				RTS_ERROR_TEXT(lydbrtnnames[ROUTINE]));						\
		entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
		entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
		set_zstatus(&entryref, errcode, NULL, FALSE);							\
		TREF(ydb_error_code) = errcode;									\
		return;												\
	}													\
	TREF(libyottadb_active_rtn) = ROUTINE;									\
	DBGAPI((stderr, "Entering routine %s\n", lydbrtnnames[ROUTINE]));					\
} MBEND

#ifdef YDB_TRACE_API
# define LIBYOTTADB_DONE 									\
MBSTART {											\
	DBGAPI((stderr, "Exiting routine %s\n", lydbrtnnames[TREF(libyottadb_active_rtn)]));	\
	TREF(libyottadb_active_rtn) = LYDB_RTN_NONE;						\
} MBEND
#else
# define LIBYOTTADB_DONE	TREF(libyottadb_active_rtn) = LYDB_RTN_NONE
#endif

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
	if (sRC->str.len > dST->len_alloc)									\
	{													\
		dST->len_used = sRC->str.len;	/* Set len to what it needed to be */				\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, sRC->str.len, dST->len_alloc);	\
	}													\
	if (sRC->str.len)											\
	{													\
		if (NULL == dST->buf_addr)									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6)						\
				ERR_PARAMINVALID, 4, LEN_AND_LIT(PARAM1), LEN_AND_LIT(PARAM2));			\
		memcpy(dST->buf_addr, sRC->str.addr, sRC->str.len);						\
	}													\
	dST->len_used = sRC->str.len;										\
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

/* Macro to lock the condition-variable/queue-mutex and, if requested, see if the related thread is running
 * yet and if not, fire it up with the requested start address. If thread gets started, wait for it to start
 * up before going forward else we run into sync-ing issues (TODO SEE). Note, this is one of two places
 * where TREF(curWorkQHeadIndx) is incremented (the other is in ydb_stm_thread()). If we are going to
 * increment it here it can only be when the the main work thread does not yet exist.
 */
#define LOCK_STM_QHEAD_AND_START_WORK_THREAD(QROOT, STARTTHREAD, THREADROUTINE, INCRQINDX, STATUS)	\
MBSTART {												\
	STATUS = pthread_mutex_lock(&((QROOT)->mutex));							\
	if (0 != STATUS)										\
	{	 											\
		SETUP_SYSCALL_ERROR("pthread_mutex_lock()", STATUS);					\
	} else if ((STARTTHREAD) && (0 == (uintptr_t)(QROOT)->threadid))				\
	{	/* The YDB main execution thread is notrunning yet - start it */			\
		STATUS = pthread_create(&((QROOT)->threadid), NULL, &THREADROUTINE, NULL);		\
		if (0 != STATUS)									\
		{     	 										\
			SETUP_SYSCALL_ERROR("pthread_create()", STATUS);				\
		}											\
		if (INCRQINDX)			/* If incrementing queue indx do so now */		\
		{											\
			assert((QROOT) == stmWorkQueue[0]);						\
			(TREF(curWorkQHeadIndx))++;							\
		}											\
	}												\
} MBEND

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
	entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
	entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
	set_zstatus(&entryref, ERR_SYSCALL, NULL, FALSE);						\
	TREF(ydb_error_code) = ERR_SYSCALL;								\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error (no parameters) */
#define SETUP_GENERIC_ERROR(ERRNUM)									\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(1) errnum);							\
	entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
	entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
	set_zstatus(&entryref, errnum, NULL, FALSE);							\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error with 2 parameters */
#define SETUP_GENERIC_ERROR_2PARMS(ERRNUM, PARM1, PARM2)						\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(4) errnum, 2, (PARM1), (PARM2));				\
	entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
	entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
	set_zstatus(&entryref, errnum, NULL, FALSE);							\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* Similar to SETUP_SYSCALL_ERROR(), this is a macro to setup a generic error with 4 parameters */
#define SETUP_GENERIC_ERROR_4PARMS(ERRNUM, PARM1, PARM2, PARM3, PARM4)					\
MBSTART {												\
	mstr	entryref;										\
	int	errnum = abs(ERRNUM);									\
	setup_error(CSA_ARG(NULL) VARLSTCNT(6) errnum, 4, (PARM1), (PARM2), (PARM3), (PARM4));		\
	entryref.addr = SIMPLEAPI_M_ENTRYREF;								\
	entryref.len = STR_LIT_LEN(SIMPLEAPI_M_ENTRYREF);						\
	set_zstatus(&entryref, errnum, NULL, FALSE);							\
	TREF(ydb_error_code) = errnum;									\
} MBEND

/* Macro to determine if we are executing in the worker thread */
#define IS_STAPI_WORKER_THREAD ((NULL != stmWorkQueue[0]) && pthread_equal(pthread_self(), stmWorkQueue[0]->threadid))

/* Similar macro to determine if we are executing in the current TP thread (applicable to ydb_tp_s()) */
#define IS_STAPI_TP_WORKER_THREAD ((NULL != stmWorkQueue[TREF(curWorkQHeadIndx)])					\
				   && (pthread_equal(pthread_self(), stmWorkQueue[TREF(curWorkQHeadIndx)]->threadid)))

/* A process is not allowed to switch "modes" in midstream. This means if a process has started using non-threaded APIs and
 * services, it cannot switch to using threaded APIs and services and vice versa.
 *
 * To that end, the following macros verify these conditions.
 */
#define VERIFY_NON_THREADED_API 										\
	MBSTART {	/* If threaded API but in worker thread, that is OK */					\
	GBLREF boolean_t noThreadAPI_active;									\
	GBLREF boolean_t simpleThreadAPI_active;								\
	GBLREF stm_workq *stmWorkQueue[];									\
	if (simpleThreadAPI_active)										\
	{													\
		if (!IS_STAPI_WORKER_THREAD)									\
		{												\
			DBGAPITP_ONLY(gtm_fork_n_core());							\
			SETUP_GENERIC_ERROR_4PARMS(ERR_INVAPIMODE, THREADED_STR_LEN, THREADED_STR,		\
						   UNTHREADED_STR_LEN, UNTHREADED_STR);				\
		}												\
		/* We are in threaded mode but running an unthreaded command in the main work thread which	\
		 * is allowed. In that case just fall out (verified).						\
		 */												\
	} else													\
		noThreadAPI_active = TRUE;									\
} MBEND
#define VERIFY_THREADED_API(RETTYPE)									\
MBSTART {												\
	GBLREF boolean_t noThreadAPI_active;								\
	GBLREF boolean_t simpleThreadAPI_active;							\
	if (noThreadAPI_active)										\
	{												\
		SETUP_GENERIC_ERROR_4PARMS(ERR_INVAPIMODE, UNTHREADED_STR_LEN, UNTHREADED_STR,		\
					   THREADED_STR_LEN, THREADED_STR);				\
		DBGAPITP_ONLY(gtm_fork_n_core());							\
		return RETTYPE YDB_ERR_INVAPIMODE;							\
	}												\
	simpleThreadAPI_active = TRUE;									\
} MBEND
#define VERIFY_THREADED_API_NORETVAL									\
MBSTART {												\
	GBLREF boolean_t noThreadAPI_active;								\
	GBLREF boolean_t simpleThreadAPI_active;							\
	if (noThreadAPI_active)										\
	{												\
		SETUP_GENERIC_ERROR_4PARMS(ERR_INVAPIMODE, UNTHREADED_STR_LEN, UNTHREADED_STR,		\
					   THREADED_STR_LEN, THREADED_STR);				\
		DBGAPITP_ONLY(gtm_fork_n_core());							\
		return;											\
	}												\
	simpleThreadAPI_active = TRUE;									\
} MBEND

/* Initialize an STM (simple thread mode) mutex */
#define INIT_STM_QUEUE_MUTEX(MUTEX_PTR) 											\
MBSTART {															\
	pthread_mutexattr_t	mattr;												\
	int			status;												\
	pthread_mutexattr_init(&mattr);	/* Initialize a mutex attribute block */						\
	status = pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);							\
	if (0 != status)													\
	{															\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_mutexattr_setrobust()"),	\
			      RTS_ERROR_LITERAL(__FILE__), __LINE__, status);							\
	}															\
	/* Use type error check to find any indications we may be double locking this mutex */					\
	status = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);							\
	if (0 != status)													\
	{															\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_mutexattr_settype()"),	\
			      RTS_ERROR_LITERAL(__FILE__), __LINE__, status);							\
	}															\
	pthread_mutex_init(&((MUTEX_PTR)->mutex), &mattr);									\
	pthread_mutexattr_destroy(&mattr);	/* Destroy mutex attribute block before it goes out of scope */			\
} MBEND


void sapi_return_subscr_nodes(int *ret_subs_used, ydb_buffer_t *ret_subsarray, char *ydb_caller_fn);
void sapi_save_targ_key_subscr_nodes(void);
intptr_t ydb_stm_args(uint64_t tptoken, stm_que_ent *callblk);
intptr_t ydb_stm_args0(uint64_t tptoken, uintptr_t calltyp);
intptr_t ydb_stm_args1(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1);
intptr_t ydb_stm_args2(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1, uintptr_t p2);
intptr_t ydb_stm_args3(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1, uintptr_t p2, uintptr_t p3);
intptr_t ydb_stm_args4(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4);
intptr_t ydb_stm_args5(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4,
		       uintptr_t p5);
#ifndef GTM64
intptr_t ydb_stm_args6(uint64_t tptoken, uintptr_t calltyp, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4,
		       uintptr_t p5, uintptr_t p6);
#endif
stm_que_ent *ydb_stm_getcallblk(void);
int ydb_stm_freecallblk(stm_que_ent *callblk);
void *ydb_stm_thread(void *parm);
void *ydb_stm_tpthread(void *parm);
stm_workq *ydb_stm_init_work_queue(void);
int ydb_tp_s_common(libyottadb_routines lydbrtn,
			ydb_basicfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, ydb_buffer_t *varnames);

/* Below are the 3 functions invoked by "pthread_atfork" during a "fork" call to ensure all SimpleThreadAPI related
 * mutex and condition variables are safely released (without any deadlocks, inconsistent states) in the child after the fork.
 */
void	ydb_stm_atfork_prepare(void);
void	ydb_stm_atfork_parent(void);
void	ydb_stm_atfork_child(void);

#endif /*  LIBYOTTADB_INT_H */
