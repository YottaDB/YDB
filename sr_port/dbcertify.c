/****************************************************************
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

<<<<<<< HEAD
#include "cli.h"
=======
#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_signal.h"

#include "continue_handler.h"
#include "sig_init.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "mupip_exit.h"
#include "gtmimagename.h"
#include "error.h"
#include "iosp.h"
#include "dbcertify.h"
#include "cli.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "startup.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "mstack_size_init.h"
#include "getjobname.h"
#include "op.h"
#include "io.h"
#include "wbox_test_init.h"
#include "gtm_post_startup_check_init.h"

GBLREF	CLI_ENTRY		dbcertify_cmd_ary[];			/* define before the GBLDEF below */
GBLREF	mstr			default_sysid;
GBLREF	char			gtm_dist[GTM_PATH_MAX];
GBLREF	boolean_t		gtm_dist_ok_to_use;
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	int			(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF	uint4			process_id;
GBLREF	spdesc			rts_stringpool, stringpool;
GBLREF	ch_ret_type		(*stpgc_ch)();				/* Function pointer to stp_gcol_ch */
>>>>>>> 7a1d2b3e... GT.M V6.3-007

int main(int argc, char **argv, char **envp)
{
<<<<<<< HEAD
	return dlopen_libyottadb(argc, argv, envp, "dbcertify_main");
=======
	struct startup_vector   svec;
	DCL_THREADGBL_ACCESS;

	/* Initialization of scaffolding we run on */
	GTM_THREADGBL_INIT;
	common_startup_init(DBCERTIFY_IMAGE);
	memset(&svec, 0, SIZEOF(svec));
	svec.argcnt = SIZEOF(svec);
	svec.rtn_start = svec.rtn_end = malloc(SIZEOF(rtn_tabent));
	memset(svec.rtn_start, 0, SIZEOF(rtn_tabent));
	svec.user_strpl_size = STP_INITSIZE_REQUESTED;
	svec.ctrlc_enable = 1;
	svec.break_message_mask = 31;
	svec.labels = 1;
	svec.lvnullsubs = 1;
	svec.base_addr = (unsigned char *)1L;
	svec.zdate_form = 0;
	svec.sysid_ptr = &default_sysid;
	mstack_size_init(&svec);
	mv_chain = (mv_stent *)msp;
	op_open_ptr = op_open;
	stp_init(STP_INITSIZE);
	stpgc_ch = &stp_gcol_ch;
	rts_stringpool = stringpool;
	getjobname();
	gtm_utf8_mode = FALSE; 		/* Only ever runs in V4 database so NO utf8 mode -- ever */
	psa_gbl = malloc(SIZEOF(*psa_gbl));
	memset(psa_gbl, 0, SIZEOF(*psa_gbl));
	err_init(dbcertify_base_ch);
	sig_init(dbcertify_signal_handler, dbcertify_signal_handler, NULL, continue_handler);
	/* Structure checks .. */
	assert((24 * 1024) == SIZEOF(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */
	op_open_ptr = op_open;
	stp_init(STP_INITSIZE);
	stpgc_ch = &stp_gcol_ch;
	rts_stringpool = stringpool;
	getjobname();
	io_init(FALSE);
	gtm_dist_ok_to_use = TRUE;	/* For RESTRICTED, but it is running alongside V4 and not the current, so lie */
	gtm_post_startup_check_init();
	OPERATOR_LOG_MSG;
	/* Platform dependent method to get the option scan going and invoke necessary driver routine */
	dbcertify_parse_and_dispatch(argc, argv);
	return SS_NORMAL;
>>>>>>> 7a1d2b3e... GT.M V6.3-007
}
