/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ERROR_H__
#define __ERROR_H__

typedef struct err_msg_struct
{
	char		*tag;
	char		*msg;
	int		parm_count;
} err_msg;

typedef struct err_ctl_struct
{
	int		facnum;
	char		*facname;
	const err_msg	*fst_msg; /* For VMS, this pointer is not used, and its value will typically be NULL */
	int		msg_cnt;
} err_ctl;

#include "errorsp.h"

#define ERROR_RETURN		error_return

#define FCNTL		1
#define MSGCNTL		27
#define MSGFAC		16
#define MSGNBIT		15
#define MSGSEVERITY	3
#define MSGNUM		3

#define FACMASK(fac)		(FCNTL << MSGCNTL | 1 << MSGNBIT | (fac) << MSGFAC)
#define MSGMASK(msg,fac)	(((msg) & ~FACMASK(fac)) >> MSGSEVERITY)
#define SEVMASK(msg)		((msg) & 7)

/* to change default severity of msg to type */
#define MAKE_MSG_TYPE(msg, type)  ((msg) & ~SEV_MSK | (type))

/* Macro used intermittently to trace various error handling invocations */
/* #define DEBUG_ERRHND */
#ifdef DEBUG_ERRHND
# define DBGEHND(x) DBGFPF(x)
# define DBGEHND_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
#else
# define DBGEHND(x)
# define DBGEHND_ONLY(x)
#endif

const err_ctl *err_check(int err);

CONDITION_HANDLER(ccp_ch);
CONDITION_HANDLER(ccp_exi_ch);
CONDITION_HANDLER(compiler_ch);
CONDITION_HANDLER(cre_priv_ch);
CONDITION_HANDLER(dbinit_ch);
CONDITION_HANDLER(dse_dmp_handler);
CONDITION_HANDLER(dse_f_blk_ch);
CONDITION_HANDLER(exi_ch);
CONDITION_HANDLER(fgncal_ch);
CONDITION_HANDLER(fntext_ch);
CONDITION_HANDLER(gds_rundown_ch);
CONDITION_HANDLER(gtcm_ch);
CONDITION_HANDLER(gtcm_exi_ch);
CONDITION_HANDLER(gtm_env_xlate_ch);
CONDITION_HANDLER(gtm_maxstr_ch);
CONDITION_HANDLER(gtmrecv_ch);
CONDITION_HANDLER(gtmrecv_fetchresync_ch);
CONDITION_HANDLER(gtmsource_ch);
CONDITION_HANDLER(gvcmy_open_ch);
CONDITION_HANDLER(gvcmz_netopen_ch);
CONDITION_HANDLER(gvzwrite_ch);
CONDITION_HANDLER(hashtab_rehash_ch);
CONDITION_HANDLER(io_init_ch);
CONDITION_HANDLER(iob_io_error);
CONDITION_HANDLER(iomt_ch);
CONDITION_HANDLER(jnl_file_autoswitch_ch);
CONDITION_HANDLER(job_init_ch);
CONDITION_HANDLER(jobexam_dump_ch);
CONDITION_HANDLER(lastchance1);
CONDITION_HANDLER(lastchance2);
CONDITION_HANDLER(lastchance3);
CONDITION_HANDLER(mdb_condition_handler);
CONDITION_HANDLER(mu_freeze_ch);
CONDITION_HANDLER(mu_int_ch);
CONDITION_HANDLER(mu_int_reg_ch);
CONDITION_HANDLER(mu_rndwn_file_ch);
CONDITION_HANDLER(mupip_load_ch);
CONDITION_HANDLER(mupip_recover_ch);
CONDITION_HANDLER(mupip_set_jnl_ch);
CONDITION_HANDLER(mur_multi_rehash_ch);
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
CONDITION_HANDLER(zshow_ch);
CONDITION_HANDLER(zyerr_ch);

void mum_tstart();

#endif
