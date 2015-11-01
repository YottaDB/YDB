/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ERROR_H__
#define __ERROR_H__

#include "errorsp.h"

CONDITION_HANDLER(ccp_ch);
CONDITION_HANDLER(ccp_exi_ch);
CONDITION_HANDLER(compiler_ch);
CONDITION_HANDLER(crit_handler);
CONDITION_HANDLER(dbinit_ch);
CONDITION_HANDLER(dse_dmp_handler);
CONDITION_HANDLER(dse_f_blk_ch);
CONDITION_HANDLER(exi_ch);
CONDITION_HANDLER(fgncal_ch);
CONDITION_HANDLER(fntext_ch);
CONDITION_HANDLER(gds_rundown_ch);
CONDITION_HANDLER(gtcm_ch);
CONDITION_HANDLER(gtmrecv_ch);
CONDITION_HANDLER(gtmrecv_fetchresync_ch);
CONDITION_HANDLER(gtmsource_ch);
CONDITION_HANDLER(gvzwrite_ch);
CONDITION_HANDLER(iob_io_error);
CONDITION_HANDLER(io_init_ch);
CONDITION_HANDLER(iomt_ch);
CONDITION_HANDLER(iosocket_ch);
CONDITION_HANDLER(lastchance1);
CONDITION_HANDLER(lastchance2);
CONDITION_HANDLER(lastchance3);
CONDITION_HANDLER(mdb_condition_handler);
CONDITION_HANDLER(mu_freeze_ch);
CONDITION_HANDLER(mu_int_ch);
CONDITION_HANDLER(mu_int_reg_ch);
CONDITION_HANDLER(mupip_load_ch);
CONDITION_HANDLER(mupip_recover_ch);
CONDITION_HANDLER(mupip_set_jnl_ch);
CONDITION_HANDLER(ojch);
CONDITION_HANDLER(region_init_ch);
CONDITION_HANDLER(replication_ch);
CONDITION_HANDLER(stp_gcol_ch);
CONDITION_HANDLER(t_ch);
CONDITION_HANDLER(terminate_ch);
CONDITION_HANDLER(tp_restart_ch);
CONDITION_HANDLER(trans_code_ch);
CONDITION_HANDLER(updproc_ch);
CONDITION_HANDLER(util_base_ch);
CONDITION_HANDLER(util_ch);
CONDITION_HANDLER(zyerr_ch);

void mum_tstart();

#endif
