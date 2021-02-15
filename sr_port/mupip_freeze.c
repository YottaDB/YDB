/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_limits.h"

#include "gdsroot.h"
#include "gtm_string.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "error.h"
#include "stp_parms.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "tp.h"
#include "util.h"
#include "gt_timer.h"
#include "mupipbckup.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "change_reg.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "wcs_wt.h"
#include "mupip_freeze.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "region_freeze_multiproc.h"

GBLREF	bool		mu_ctrly_occurred;
GBLREF	bool		mu_ctrlc_occurred;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
GBLREF	tp_region	*halt_ptr;
GBLREF	tp_region	*grlist;
GBLREF	bool		in_mupip_freeze;
GBLREF	bool		error_mupip;
GBLREF	boolean_t	debug_mupip;
GBLREF	boolean_t	jnlpool_init_needed;
GBLREF	jnl_gbls_t	jgbl;

uint4			parallel_freeze_online;
freeze_multiproc_state	*parallel_shm_hdr;

#define INTERRUPTED	(mu_ctrly_occurred || mu_ctrlc_occurred)
#define PRINT_FREEZEERR 													\
MBSTART {															\
	gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(6)									\
			ERR_FREEZEERR, 4, LEN_AND_STR(msg1[freeze]), REG_LEN_STR(rptr->reg));					\
	status = ERR_MUNOFINISH;												\
} MBEND
#define PRINT_UNFROZEN_MSG	util_out_print("All regions will be unfrozen", TRUE)

error_def(ERR_BUFFLUFAILED);
error_def(ERR_DBRDONLY);
error_def(ERR_FREEZECTRL);
error_def(ERR_JNLEXTEND);
error_def(ERR_JNLFILOPN);
error_def(ERR_MUNOACTION);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUNOFINISH);
error_def(ERR_OFRZNOTHELD);
error_def(ERR_KILLABANDONED);
error_def(ERR_FREEZEERR);

uint4 freeze_online_multi_proc_init(reg_ctl_list *rctl);
uint4 freeze_online_multi_proc(reg_ctl_list *rctl);
uint4 freeze_online_multi_proc_finish(reg_ctl_list *rctl);

/* This initialization function is called by gtm_multi_proc after shared memory creation */
uint4 freeze_online_multi_proc_init(reg_ctl_list *rctl)
{
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */
	int			status;

	assert(multi_proc_in_use);
	mp_hdr = multi_proc_shm_hdr;
	parallel_shm_hdr = (freeze_multiproc_state *)((sm_uc_ptr_t)mp_hdr->shm_ret_array
							+ (SIZEOF(void *) * mp_hdr->ntasks));
	parallel_shm_hdr->ntasks = mp_hdr->ntasks;

	status = pthread_mutexattr_init(&parallel_shm_hdr->reg_frozen_mutex_attr);
	if (0 != status)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			LEN_AND_LIT("pthread_mutexattr_init"), CALLFROM, status, 0);
	status = pthread_condattr_init(&parallel_shm_hdr->reg_frozen_cond_attr);
	if (0 != status)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			LEN_AND_LIT("pthread_condattr_init"), CALLFROM, status, 0);
	status = pthread_mutexattr_setpshared(&parallel_shm_hdr->reg_frozen_mutex_attr, PTHREAD_PROCESS_SHARED);
	if (0 != status)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			LEN_AND_LIT("pthread_mutexattr_setpshared"), CALLFROM, status, 0);
	status = pthread_condattr_setpshared(&parallel_shm_hdr->reg_frozen_cond_attr, PTHREAD_PROCESS_SHARED);
	if (0 != status)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			LEN_AND_LIT("pthread_condattr_setpshared"), CALLFROM, status, 0);
	status = pthread_mutex_init(&parallel_shm_hdr->reg_frozen_mutex, &parallel_shm_hdr->reg_frozen_mutex_attr);
	if (0 != status)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			LEN_AND_LIT("pthread_mutex_init"), CALLFROM, status, 0);
	status = pthread_cond_init(&parallel_shm_hdr->reg_frozen_cond, &parallel_shm_hdr->reg_frozen_cond_attr);
	if (0 != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("pthread_cond_init"), CALLFROM, status, 0);
	return SS_NORMAL;
}

uint4 freeze_online_multi_proc(reg_ctl_list *rctl)
{
	gd_region		*reg;
	freeze_status		freeze_ret;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	boolean_t		release_latch;
	freeze_reg_mp_state	fr_multiproc;
	unix_db_info		*udi;

	reg = rctl->gd;
	csa = &FILE_INFO(reg)->s_addrs;
	udi = FILE_INFO(reg);
	csd = csa->hdr;
	assert(multi_proc_in_use);
	assert(udi->owning_gd);
	udi->owning_gd->thread_gdi = NULL;
	fr_multiproc.region_index = rctl->region_index;
	fr_multiproc.pfms = parallel_shm_hdr;
	MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);
	if (FROZEN_CHILLED(csa) || !IS_REG_BG_OR_MM(reg) || reg_cmcheck(reg) || reg->was_open ||
	       reg->read_only || dba_mm == csd->acc_meth)
	{	/* Mark as already frozen */
		while (fr_multiproc.region_index > fr_multiproc.pfms->grab_crit_counter)
			SLEEP_USEC(10, FALSE);
		grab_crit(reg, WS_102);
		INCR_CNT(&fr_multiproc.pfms->grab_crit_counter, &fr_multiproc.pfms->grab_crit_latch);
		INCR_REG_FROZEN_COUNT(fr_multiproc.pfms, FALSE);
		rel_crit(reg);
		fr_multiproc.pfms->freeze_ret_array[fr_multiproc.region_index] = REG_ALREADY_FROZEN;
		return SS_NORMAL;
	}
	/* Flush the right region when using gtm_multi_proc */
	TP_CHANGE_REG(reg);
	/* Stash the return values in a shared memory array */
	fr_multiproc.pfms->freeze_ret_array[fr_multiproc.region_index] =
				region_freeze_main(reg, TRUE, FALSE, TRUE, parallel_freeze_online, TRUE, &fr_multiproc);
	while (REG_FREEZE_SUCCESS != fr_multiproc.pfms->freeze_ret_array[fr_multiproc.region_index])
	{
		switch (fr_multiproc.pfms->freeze_ret_array[fr_multiproc.region_index])
		{
			case REG_ALREADY_FROZEN:
				/* Exit normally do not retry to freeze an already frozen region */
				return SS_NORMAL;
			case REG_HAS_KIP:
				/* Return REG_HAS_KIP status only when the retries exceed MAX_CRIT_TRY */
				release_latch = FALSE;	/* needed to avoid [-Wsometimes-uninitialized] warning from C compiler */
				GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
				assert(release_latch);
				util_out_print("Kill in progress indicator is set for database file !AD", TRUE,
						DB_LEN_STR(reg));
				REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
				multi_proc_key = NULL;  /* reset key until it can be set to rctl's region-name again */
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(1) ERR_MUNOFINISH);
				break;
			case REG_FLUSH_ERROR:
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("MUPIP FREEZE"),
						DB_LEN_STR(reg));
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(1) ERR_MUNOFINISH);
				break;
			case REG_JNL_OPEN_ERROR:
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_JNLFILOPN, 4, JNL_LEN_STR(csd),
						DB_LEN_STR(reg), csa->jnl->status);
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(1) ERR_MUNOFINISH);
				break;
			case REG_JNL_SWITCH_ERROR:
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(csd));
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(1) ERR_MUNOFINISH);
				break;
			default:
				assert(FALSE);
		}
	}
	release_latch = FALSE;	/* needed to avoid [-Wsometimes-uninitialized] warning from C compiler */
	GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
	assert(release_latch);
	util_out_print("Region !AD is now FROZEN", TRUE, REG_LEN_STR(reg));
	REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
	multi_proc_key = NULL;  /* reset key until it can be set to rctl's region-name again */
	return SS_NORMAL;
}

/* Return the "worst" freeze status from the stashed array from freeze_online_multi_proc */
uint4 freeze_online_multi_proc_finish(reg_ctl_list *rctl)
{
	int			regno;

	assert(multi_proc_in_use);
	pthread_mutexattr_destroy(&parallel_shm_hdr->reg_frozen_mutex_attr);
	pthread_condattr_destroy(&parallel_shm_hdr->reg_frozen_cond_attr);
	pthread_mutex_destroy(&parallel_shm_hdr->reg_frozen_mutex);
	pthread_cond_destroy(&parallel_shm_hdr->reg_frozen_cond);
	for (regno=0 ; regno < parallel_shm_hdr->ntasks ; regno++)
	{	/* Look for a non-zero freeze status */
		if (0 != parallel_shm_hdr->freeze_ret_array[regno])
			return ERR_MUNOFINISH;
	}
	return SS_NORMAL;
}

void	mupip_freeze(void)
{
	int4			status;
	bool			record;
	tp_region		*rptr, *rptr1;
	boolean_t		freeze, override, parallel = FALSE;
	uint4			online;
	freeze_status		freeze_ret;
	int			dummy_errno, regno, reg_total = 0;
	const char 		*msg1[] = { "unfreeze", "freeze" };
	const char 		*msg2[] = { "UNFROZEN", "FROZEN" };
	const char 		*msg3[] = { "unfrozen", "frozen" };
	reg_ctl_list		*parallel_ctl;
	void			**ret_array;		/* "gtm_multi_thread" related field */
	size_t			shm_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	status = SS_NORMAL;
	in_mupip_freeze = TRUE;
	UNIX_ONLY(jnlpool_init_needed = TRUE);
	mu_outofband_setup();
	gvinit();
	freeze = (CLI_PRESENT == cli_present("ON"));
	online = (CLI_PRESENT == cli_present("ONLINE"));
	if (online)
		online |= ((!cli_negated("AUTORELEASE")) ? CHILLED_AUTORELEASE_MASK : 0);
	if (CLI_PRESENT == cli_present("OFF"))
	{
		if (TRUE == freeze)
		{
			util_out_print("The /ON qualifier is invalid with the /OFF qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	if (CLI_PRESENT == cli_present("RECORD"))
	{
		record = TRUE;
		if (FALSE == freeze)
		{
			util_out_print("The /RECORD qualifier is invalid with the /OFF qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		record = FALSE;
	if (CLI_PRESENT == cli_present("OVERRIDE"))
	{
		override = TRUE;
		if (freeze)
		{
			util_out_print("The /OVERRIDE qualifier is invalid with the /ON qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		override = FALSE;
	error_mupip = FALSE;
	/* DBG qualifier prints extra debug messages while waiting for KIP in region freeze */
	debug_mupip = (CLI_PRESENT == cli_present("DBG"));
	mu_getlst("REG_NAME", SIZEOF(tp_region));
	if (error_mupip)
	{
		util_out_print("!/MUPIP cannot start freeze with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	halt_ptr = grlist;
	ESTABLISH(mu_freeze_ch);
	/* Count the regions in a case of a parallel freeze */
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
		reg_total++;
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
	{
		if (INTERRUPTED)
			break;
		if (!IS_REG_BG_OR_MM(rptr->reg))
		{
			util_out_print("Can only FREEZE BG and MM databases", TRUE);
			PRINT_FREEZEERR;
			continue;
		}
		if (reg_cmcheck(rptr->reg))
		{
			util_out_print("!/Can't FREEZE region !AD across network", TRUE, REG_LEN_STR(rptr->reg));
			PRINT_FREEZEERR;
			continue;
		}
		gv_cur_region = rptr->reg;
		gvcst_init(gv_cur_region);
		if (gv_cur_region->was_open)	/* Already open under another name.  Region will not be marked open*/
		{
			gv_cur_region->open = FALSE;
			util_out_print("FREEZE region !AD is already open under another name", TRUE, REG_LEN_STR(rptr->reg));
			continue;
		}
		change_reg();
		assert(&FILE_INFO(gv_cur_region)->s_addrs == cs_addrs);
		/* Cannot flush for read-only data files */
		if (gv_cur_region->read_only)
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, gv_cur_region->dyn.addr->fname_len,
					gv_cur_region->dyn.addr->fname);
			PRINT_FREEZEERR;
			continue;
		}
		if (online && (dba_mm == cs_addrs->hdr->acc_meth))
		{
			util_out_print("FREEZE -ONLINE can't apply to MM region !AD", TRUE, REG_LEN_STR(rptr->reg));
			cs_addrs->persistent_freeze = TRUE;	/* Prevent removal of existing freeze */
			PRINT_FREEZEERR;
			continue;
		}
		if (freeze && (0 != cs_addrs->hdr->abandoned_kills))
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_KILLABANDONED, 4, DB_LEN_STR(rptr->reg),
					LEN_AND_LIT("database could have incorrectly marked busy integrity errors"));
			util_out_print("WARNING: The region !AD to be frozen contains abandoned kills",
				TRUE, REG_LEN_STR(gv_cur_region));
		}
		if (freeze && FROZEN_CHILLED(cs_addrs))
		{
			util_out_print("FREEZE region !AD already has an online freeze", TRUE, REG_LEN_STR(rptr->reg));
			PRINT_FREEZEERR;
			cs_addrs->persistent_freeze = TRUE;	/* Prevent removal of existing freeze */
			continue;
		}
		if (!cs_data->freeze && cs_addrs->nl->freeze_online)
		{
			util_out_print("WARNING: The region !AD had an online freeze, but it was autoreleased.",
					TRUE, REG_LEN_STR(gv_cur_region));
			status = ERR_OFRZNOTHELD;
		}
		if ((reg_total > 1) && online)
		{	/* Use gtm_multi_proc for FREEZE -ONLINE */
			parallel = TRUE;
			parallel_freeze_online = online;
			continue;
		}
		while (REG_FREEZE_SUCCESS !=
				(freeze_ret = region_freeze_main(gv_cur_region, freeze, override, TRUE, online, TRUE, NULL)))
		{
			if (REG_ALREADY_FROZEN == freeze_ret)
			{
				hiber_start(1000);
				if (INTERRUPTED)
					break;
			} else if (REG_HAS_KIP == freeze_ret)
			{
				assert(!override);
				util_out_print("Kill in progress indicator is set for database file !AD", TRUE,
					DB_LEN_STR(gv_cur_region));
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else if (REG_FLUSH_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("MUPIP FREEZE"),
										DB_LEN_STR(gv_cur_region));
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else if (REG_JNL_OPEN_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_JNLFILOPN, 4, JNL_LEN_STR(cs_data),
						DB_LEN_STR(gv_cur_region), cs_addrs->jnl->status);
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else if (REG_JNL_SWITCH_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(cs_data));
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else
				assert(FALSE);
		}
		cs_addrs->persistent_freeze = freeze;	/* secshr_db_clnup() shouldn't clear the freeze up */
		halt_ptr = rptr->fPtr;
		if (record && gv_cur_region->open)
			cs_addrs->hdr->last_rec_backup = cs_addrs->ti->curr_tn;
		if (REG_FREEZE_SUCCESS == freeze_ret)
			util_out_print("Region !AD is now !AD", TRUE, REG_LEN_STR(gv_cur_region), LEN_AND_STR(msg2[freeze]));
		else
			PRINT_FREEZEERR;
	}
	if (parallel)
	{
		ret_array = (void **)malloc(SIZEOF(void *) * reg_total);
		parallel_ctl = (reg_ctl_list *)malloc(SIZEOF(reg_ctl_list) * reg_total);
		memset(parallel_ctl, 0, SIZEOF(reg_ctl_list) * reg_total);
		shm_size = (size_t)(SIZEOF(freeze_multiproc_state)
				+ (SIZEOF(int) * reg_total));
		for (rptr = grlist, regno = 0;  NULL != rptr;  rptr = rptr->fPtr, regno++)
		{
			parallel_ctl[regno].region_index = regno;
			parallel_ctl[regno].gd = rptr->reg;
		}
		status = gtm_multi_proc((gtm_multi_proc_fnptr_t)&freeze_online_multi_proc, reg_total,
				MULTI_PROC_ONE_PER_REG, ret_array, (void *)parallel_ctl, SIZEOF(reg_ctl_list),
				shm_size, (gtm_multi_proc_fnptr_t)&freeze_online_multi_proc_init,
				(gtm_multi_proc_fnptr_t)&freeze_online_multi_proc_finish);
	}
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
	{
		gv_cur_region = rptr->reg;
		change_reg();
		if (parallel)
			cs_addrs->persistent_freeze = TRUE;
		freeze_ret = region_freeze_post(rptr->reg);
		if (REG_FREEZE_SUCCESS != freeze_ret)
		{
			if (REG_FLUSH_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("MUPIP FREEZE"),
										DB_LEN_STR(gv_cur_region));
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else if (REG_JNL_OPEN_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_JNLFILOPN, 4, JNL_LEN_STR(cs_data),
						DB_LEN_STR(gv_cur_region), cs_addrs->jnl->status);
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else if (REG_JNL_SWITCH_ERROR == freeze_ret)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(cs_data));
				PRINT_UNFROZEN_MSG;
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MUNOFINISH);
			} else
				assert(FALSE);
		}
	}
	REVERT;
	if (INTERRUPTED)
	{
		for (rptr1 = grlist;  rptr1 != rptr;  rptr1 = rptr1->fPtr)
		{
			gv_cur_region = rptr1->reg;
			if (FALSE == gv_cur_region->open)
				continue;
			region_freeze(gv_cur_region, FALSE, FALSE, FALSE, FALSE, FALSE);
		}
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_FREEZECTRL);
		status = ERR_MUNOFINISH;
	}
	if (SS_NORMAL == status)
		util_out_print("All requested regions !AD", TRUE, LEN_AND_STR(msg3[freeze]));
	mupip_exit(status);
}
