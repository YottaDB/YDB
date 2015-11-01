/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "startup.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "error.h"
#include "cli.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtmimagename.h"
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

GBLREF void		(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF void		(*tp_timeout_clear_ptr)(void);
GBLREF void		(*tp_timeout_action_ptr)(void);
GBLREF void		(*ctrlc_handler_ptr)();
GBLREF int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF void		(*unw_prof_frame_ptr)(void);

GBLREF enum gtmImageTypes	image_type;
GBLREF mstr			default_sysid;
GBLDEF boolean_t		gtm_startup_active = FALSE;
GBLDEF void			(*restart)() = &mum_tstart;

void init_gtm(void)
{
	struct startup_vector   svec;

	image_type = GTM_IMAGE;

	/* We believe much of our code depends on these relationships.  */
	assert (sizeof(int) == 4);
	assert (sizeof(int4) == 4);
	assert (sizeof(short) == 2);
#if defined(OFF_T_LONG) || defined(__MVS__)
	assert (sizeof(off_t) == 8);
#else
	assert (sizeof(off_t) == 4);
#endif
	assert (sizeof(sgmnt_data) == ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));
	assert (sizeof(key_t) == sizeof(int4));
	assert (sizeof(boolean_t) == 4); /* generated code passes 4 byte arguments, run time rtn might be expecting boolean_t arg */
	assert (BITS_PER_UCHAR == 8);

	tp_timeout_start_timer_ptr = tp_start_timer;
	tp_timeout_clear_ptr = tp_clear_timeout;
	tp_timeout_action_ptr = tp_timeout_action;
	ctrlc_handler_ptr = ctrlc_handler;
	op_open_ptr = op_open;
	unw_prof_frame_ptr = unw_prof_frame;

	if (MUMPS_COMPILE == invocation_mode)
		exit(gtm_compile());

	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	memset(&svec, 0, sizeof(svec));
	svec.argcnt = sizeof(svec);
	svec.rtn_start = svec.rtn_end = malloc(sizeof(rtn_tables));
	memset(svec.rtn_start, 0, sizeof(rtn_tables));
	svec.user_stack_size = 120 * 1024;
	svec.user_indrcache_size = 32;
	svec.user_strpl_size = 20480;
	svec.ctrlc_enable = 1;
	svec.break_message_mask = 15;
	svec.labels = 1;
	svec.lvnullsubs = 1;
	svec.base_addr = (unsigned char *)1;
	svec.zdate_form = 0;
	svec.sysid_ptr = &default_sysid;
	gtm_startup(&svec);
	gtm_startup_active = TRUE;
}
