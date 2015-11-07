/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif
#include "gtm_stdlib.h"
#include "gtm_string.h"

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

GBLREF void		(*ctrlc_handler_ptr)();
GBLREF void		(*tp_timeout_action_ptr)(void);
GBLREF void		(*tp_timeout_clear_ptr)(void);
GBLREF void		(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF void		(*op_write_ptr)(mval *v);
GBLREF void		(*op_wteol_ptr)(int4 n);
GBLREF void		(*unw_prof_frame_ptr)(void);

GBLREF mstr		default_sysid;
#ifdef GTM_PTHREAD
GBLREF pthread_t	gtm_main_thread_id;
GBLREF boolean_t	gtm_main_thread_id_set;
GBLREF boolean_t	gtm_jvm_process;
#endif
GBLDEF boolean_t	gtm_startup_active = FALSE;

void init_gtm(void)
{
	struct startup_vector   svec;
	DEBUG_ONLY(mval		chkmval;)
	DEBUG_ONLY(mval		chkmval_b;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We believe much of our code depends on these relationships.  */
	assert(SIZEOF(int) == 4);
	assert(SIZEOF(int4) == 4);
	assert(SIZEOF(short) == 2);
#ifdef OFF_T_LONG
	assert(SIZEOF(off_t) == 8);
#else
	assert(SIZEOF(off_t) == 4);
#endif
	assert(SIZEOF(sgmnt_data) == ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
#ifdef KEY_T_LONG
	assert(8 == SIZEOF(key_t));
#else
	assert(SIZEOF(key_t) == SIZEOF(int4));
#endif
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
	if (!gtm_main_thread_id_set && gtm_jvm_process)
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
	op_write_ptr = op_write;
	op_wteol_ptr = op_wteol;
	unw_prof_frame_ptr = unw_prof_frame;

	if (MUMPS_COMPILE == invocation_mode)
		exit(gtm_compile());

	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
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
