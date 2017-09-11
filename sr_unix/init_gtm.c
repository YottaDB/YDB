/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_string.h"

#include <stdarg.h>

#include "gtm_multi_thread.h"
#include "startup.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "error.h"
#include "cli.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "tp_timeout.h"
#include "ctrlc_handler.h"
#include "mprof.h"
#include "gtm_startup_chk.h"
#include "gtm_compile.h"
#include "gtm_startup.h"
#include "jobchild_init.h"
#include "cli_parse.h"
#include "invocation_mode.h"
#include "fnpc.h"
#include "gtm_malloc.h"
#include "stp_parms.h"
#include "create_fatal_error_zshow_dmp.h"
#include "mtables.h"
#include "show_source_line.h"
#include "patcode.h"
#include "collseq.h"

GBLREF boolean_t	utf8_patnumeric;
GBLREF int4		exi_condition;
GBLREF mstr		dollar_zchset;
GBLREF mstr		dollar_zpatnumeric;
GBLREF mstr		default_sysid;
GBLREF pattern		*pattern_list;
GBLREF pattern		*curr_pattern;
GBLREF pattern		mumps_pattern;
GBLREF uint4		*pattern_typemask;
GBLREF int		(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF void		(*ctrlc_handler_ptr)();
GBLREF void		(*tp_timeout_action_ptr)(void);
GBLREF void		(*tp_timeout_clear_ptr)(void);
GBLREF void		(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF void		(*unw_prof_frame_ptr)(void);
GBLREF void		(*stx_error_fptr)(int in_error, ...);	/* Function pointer for stx_error() so gtm_utf8.c can avoid pulling
								 * stx_error() into gtmsecshr, and thus just about everything else
								 * as well.
								 */
GBLREF void		(*stx_error_va_fptr)(int in_error, va_list args);	/* Function pointer for stx_error() so rts_error.c
										 * can avoid pulling stx_error() into gtmsecshr,
										 * and thus just about everything else as well.
										 */
GBLREF	void		(*mupip_exit_fp)(int4 errnum);		/* Func ptr for mupip_exit() but in GTM, points to assert rtn */
GBLREF	void		(*show_source_line_fptr)(boolean_t warn); /* Func ptr for show_source_line() for same reason as above */
#ifdef GTM_PTHREAD
GBLREF pthread_t	gtm_main_thread_id;
GBLREF boolean_t	gtm_main_thread_id_set;
GBLREF boolean_t	gtm_jvm_process;
#endif
GBLDEF boolean_t	gtm_startup_active = FALSE;

STATICFNDCL void assert_on_entry(int4 arg);

error_def(ERR_COLLATIONUNDEF);

void init_gtm(void)
{	/* initialize process characteristics and states, but beware as initialization occurs in other places as well
	 * the function pointer initializations below happen in both the GT.M runtime and in mupip
	 */
	struct startup_vector   svec;
	int			i;
	int4			lct;
	DEBUG_ONLY(mval		chkmval;)
	DEBUG_ONLY(mval		chkmval_b;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We believe much of our code depends on these relationships.  */
	assert(SIZEOF(int) == 4);
	assert(SIZEOF(int4) == 4);
	assert(SIZEOF(short) == 2);
#	ifdef OFF_T_LONG
	assert(SIZEOF(off_t) == 8);
#	else
	assert(SIZEOF(off_t) == 4);
#	endif
	assert(SIZEOF(sgmnt_data) == ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
#	ifdef KEY_T_LONG
	assert(8 == SIZEOF(key_t));
#	else
	assert(SIZEOF(key_t) == SIZEOF(int4));
#	endif
	assert(SIZEOF(boolean_t) == 4); /* generated code passes 4 byte arguments, run time rtn might be expecting boolean_t arg */
	assert(BITS_PER_UCHAR == 8);
	assert(SIZEOF(enum db_ver) == SIZEOF(int4));
	assert(254 >= FNPC_MAX);	/* The value 255 is reserved */
	assert(SIZEOF(mval) == SIZEOF(mval_b));
	assert(SIZEOF(chkmval.fnpc_indx) == SIZEOF(chkmval_b.fnpc_indx));
	assert(OFFSETOF(mval, fnpc_indx) == OFFSETOF(mval_b, fnpc_indx));
	DEBUG_ONLY(mtables_chk());	/* Validate mtables.c assumptions */
	SFPTR(create_fatal_error_zshow_dmp_fptr, create_fatal_error_zshow_dmp);
#	ifdef GTM_PTHREAD
	assert(!gtm_main_thread_id_set);
	if (!gtm_main_thread_id_set)
	{
		gtm_main_thread_id = pthread_self();
		gtm_main_thread_id_set = TRUE;
	}
#	endif
	tp_timeout_start_timer_ptr = tp_start_timer;
	tp_timeout_clear_ptr = tp_clear_timeout;
	tp_timeout_action_ptr = tp_timeout_action;
	ctrlc_handler_ptr = ctrlc_handler;
	if (MUMPS_UTILTRIGR != invocation_mode)
		op_open_ptr = op_open;
	unw_prof_frame_ptr = unw_prof_frame;
	stx_error_fptr = stx_error;
	stx_error_va_fptr = stx_error_va;
	show_source_line_fptr = show_source_line;
	/* For compile time optimization, we need to have the cache for $PIECE enabled */
	for (i = 0; FNPC_MAX > i; i++)
	{	/* Initialize cache structure for $[Z]PIECE function */
		(TREF(fnpca)).fnpcs[i].indx = i;
	}
	(TREF(fnpca)).fnpcsteal = (TREF(fnpca)).fnpcs;			/* Starting place to look for cache reuse */
	(TREF(fnpca)).fnpcmax = &(TREF(fnpca)).fnpcs[FNPC_MAX - 1];	/* The last element */
	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask;
	initialize_pattern_table();
	/* Initialize local collating sequence */
	TREF(transform) = TRUE;
	lct = find_local_colltype();
	if (lct != 0)
	{
		TREF(local_collseq) = ready_collseq(lct);
		if (!TREF(local_collseq))
		{
			exi_condition = -ERR_COLLATIONUNDEF;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
			EXIT(exi_condition);
		}
	} else
		TREF(local_collseq) = 0;
	if (gtm_utf8_mode)
	{
		dollar_zchset.len = STR_LIT_LEN(UTF8_NAME);
		dollar_zchset.addr = UTF8_NAME;
		if (utf8_patnumeric)
		{
			dollar_zpatnumeric.len = STR_LIT_LEN(UTF8_NAME);
			dollar_zpatnumeric.addr = UTF8_NAME;
		}
	}
	if (MUMPS_COMPILE == invocation_mode)				/* MUMPS compile branches here and gets none of the below */
	{
		TREF(transform) = FALSE;
		EXIT(gtm_compile());
	}
	/* With the advent of reservedDBs, the ability to create a new database is not only in MUPIP but is in MUMPS too.
	 * This means mucregini() (called by mu_cre_file()) is also in libgtmshr. But mucregini calls mupip_exit for certain
	 * types of errors we won't run into with reservedDBs. So, if mupip.c has not already initialized this function
	 * pointer for mupip_exit to be used in mucregini() (MUPIP calls this routine too), then initialize it to point to a
	 * routine that will assert fail if it is used in MUMPS .
	 */
	if (NULL == mupip_exit_fp)
		mupip_exit_fp = assert_on_entry;
	/* This should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	memset(&svec, 0, SIZEOF(svec));
	svec.argcnt = SIZEOF(svec);
	svec.rtn_start = svec.rtn_end = malloc(SIZEOF(rtn_tabent));
	memset(svec.rtn_start, 0, SIZEOF(rtn_tabent));
	svec.user_stack_size = (272 ZOS_ONLY(+ 64))* 1024;	/* ZOS stack frame 2x other platforms so give more stack */
	svec.user_strpl_size = STP_INITSIZE_REQUESTED;
	svec.ctrlc_enable = 1;
	svec.break_message_mask = 31;
	svec.labels = 1;
	svec.lvnullsubs = 1;
	svec.base_addr = (unsigned char *)1L;
	svec.zdate_form = 0;
	svec.sysid_ptr = &default_sysid;
	gtm_startup(&svec);
	gtm_startup_active = TRUE;
}

/* Routine to be driven by a function pointer when that function pointer should never be driven.
 * Does an assertpro() to stop things.
 */
void assert_on_entry(int4 arg)
{
	assertpro(FALSE);
}
