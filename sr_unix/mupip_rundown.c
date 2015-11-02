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

#include "mdef.h"

#include <sys/shm.h>
#include "gtm_string.h"
#include "gtm_inet.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "gdscc.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "mupipbckup.h"
#include "mu_rndwn_file.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_all.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "dpgbldir.h"
#include "gtmio.h"
#include "dpgbldir_sysops.h"
#include "mu_gv_cur_reg_init.h"
#include "mupip_rundown.h"
#include "gtmmsg.h"
#include "repl_instance.h"
#include "mu_rndwn_repl_instance.h"
#include "util.h"
#include "anticipatory_freeze.h"
#include "gtm_ipc.h"
#include "repl_sem.h"
#include "ftok_sems.h"
#include "ipcrmid.h"

GBLREF	bool			in_backup;
GBLREF	bool			error_mupip;
GBLREF	tp_region		*grlist;
GBLREF	gd_region		*gv_cur_region;
GBLREF	boolean_t		mu_star_specified;
GBLREF	boolean_t		donot_fflush_NULL;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		argumentless_rundown;

error_def(ERR_MUFILRNDWNSUC);
error_def(ERR_MUJPOOLRNDWNFL);
error_def(ERR_MUJPOOLRNDWNSUC);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNOTALLSEC);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUQUALINCOMP);
error_def(ERR_REPLPOOLINST);
error_def(ERR_SEMREMOVED);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void mupip_rundown(void)
{
	int			exit_status, semid, shmid, save_errno, status;
	boolean_t		region, file, arg_present, do_jnlpool_detach, jnlpool_sem_created, jnlpool_rndwn_required;
	boolean_t		repl_inst_available, shm_removed = FALSE, anticipatory_freeze_available;
	tp_region		*rptr, single;
	replpool_identifier	replpool_id;
	repl_inst_hdr		repl_instance;
	struct shmid_ds		shm_buf;
	unsigned int		full_len;
	char			*instfilename;
	unsigned char		ipcs_buff[MAX_IPCS_ID_BUF], *ipcs_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	exit_status = SS_NORMAL;
	file = (CLI_PRESENT == cli_present("FILE"));
	region = (CLI_PRESENT == cli_present("REGION"));
	arg_present = (0 != TREF(parms_cnt));
	if ((file == region) && (TRUE == file))
		mupip_exit(ERR_MUQUALINCOMP);
	if (arg_present && !file && !region)
	{
		util_out_print("MUPIP RUNDOWN only accepts a parameter when -FILE or -REGION is specified.", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	if (region)
	{
		gvinit();
		mu_getlst("WHAT", SIZEOF(tp_region));
		rptr = grlist;
		if (error_mupip)
			exit_status = ERR_MUNOTALLSEC;
	} else if (file)
	{
		mu_gv_cur_reg_init();
		gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
		if (!cli_get_str("WHAT",  (char *)&gv_cur_region->dyn.addr->fname[0], &gv_cur_region->dyn.addr->fname_len))
			mupip_exit(ERR_MUNODBNAME);
		*(gv_cur_region->dyn.addr->fname + gv_cur_region->dyn.addr->fname_len) = 0;
		rptr = &single;		/* a dummy value that permits one trip through the loop */
		rptr->fPtr = NULL;
	}
	in_backup = FALSE;		/* Only want yes/no from mupfndfil, not an address */
	if (region || file)
	{
		do_jnlpool_detach = FALSE;
		anticipatory_freeze_available = ANTICIPATORY_FREEZE_AVAILABLE;
		if ((jnlpool_rndwn_required = (region && mu_star_specified)) || anticipatory_freeze_available) /* note:assigmnent */
		{
			if (DEBUG_ONLY(repl_inst_available = )REPL_INST_AVAILABLE) /* sets replpool_id/full_len; note: assignment */
			{
				instfilename = &replpool_id.instfilename[0];
				if (!mu_rndwn_repl_instance(&replpool_id, !anticipatory_freeze_available, TRUE,
						&jnlpool_sem_created))
				{
					assert(NULL == jnlpool_ctl);
					exit_status = ERR_MUNOTALLSEC;
				}
				else
					do_jnlpool_detach = (NULL != jnlpool_ctl);
				ENABLE_FREEZE_ON_ERROR;
			}
		}
		for ( ; NULL != rptr; rptr = rptr->fPtr)
		{
			if (region)
			{
				if (!mupfndfil(rptr->reg, NULL))
				{
					exit_status = ERR_MUNOTALLSEC;
					continue;
				}
				gv_cur_region = rptr->reg;
				if (NULL == gv_cur_region->dyn.addr->file_cntl)
				{
					gv_cur_region->dyn.addr->acc_meth = dba_bg;
					gv_cur_region->dyn.addr->file_cntl =
						(file_control *)malloc(SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					memset(gv_cur_region->dyn.addr->file_cntl, 0, SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
					gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(SIZEOF(GDS_INFO));
					memset(gv_cur_region->dyn.addr->file_cntl->file_info, 0, SIZEOF(GDS_INFO));
				}
			}
			if (TRUE == mu_rndwn_file(gv_cur_region, FALSE))
				gtm_putmsg(VARLSTCNT(4) ERR_MUFILRNDWNSUC, 2, DB_LEN_STR(gv_cur_region));
			else
				exit_status = ERR_MUNOTALLSEC;
		}
		if (do_jnlpool_detach)
		{
			assert(anticipatory_freeze_available && repl_inst_available);
			assert(NULL != jnlpool_ctl);
			/* Read the instance file to invalidate the journal pool semaphore ID and shared memory ID */
			assert(NULL != jnlpool.jnlpool_dummy_reg);
			assert(INVALID_SEMID != jnlpool.repl_inst_filehdr->jnlpool_semid);
			assert(INVALID_SHMID != jnlpool.repl_inst_filehdr->jnlpool_shmid);
			assert(0 != jnlpool.repl_inst_filehdr->jnlpool_semid_ctime);
			assert(0 != jnlpool.repl_inst_filehdr->jnlpool_shmid_ctime);
			semid = jnlpool.repl_inst_filehdr->jnlpool_semid;
			shmid = jnlpool.repl_inst_filehdr->jnlpool_shmid;
			/* Detach from the journal pool */
			JNLPOOL_SHMDT(status, save_errno);
			jnlpool.gtmsrc_lcl_array = NULL;
			jnlpool.repl_inst_filehdr = NULL;
			jnlpool.gtmsource_local_array = NULL;
			jnlpool.jnldata_base = NULL;
			if (0 > status)
			{
				ISSUE_REPLPOOLINST(save_errno, shmid, instfilename, "shmdt()");
				mupip_exit(ERR_MUNOTALLSEC);
			}
			/* Grab the ftok again */
			if (!ftok_sem_lock(jnlpool.jnlpool_dummy_reg, FALSE, FALSE))
			{	/* CRITSEMFAIL is issued in case of an error */
				assert(FALSE);
				mupip_exit(ERR_MUNOTALLSEC);
			}
			if (jnlpool_rndwn_required)
			{	/* User requested for running down the journal pool as well. So, regrab the access control semaphore
				 * before proceeding to remove the shared memory. We should already have incremented the counter
				 * semaphore. But, before that read the instance file since we need to invalidate the semid/shmid
				 * below.
				 */
				repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
				assert(semid == repl_instance.jnlpool_semid);	/* Should still be valid */
				assert(shmid == repl_instance.jnlpool_shmid);	/* Should still be valid */
				assert(holds_sem[SOURCE][SRC_SERV_COUNT_SEM]);
				if (SS_NORMAL != grab_sem(SOURCE, JNL_POOL_ACCESS_SEM))
				{
					save_errno = errno;
					ISSUE_REPLPOOLINST(save_errno, shmid, instfilename, "semop()");
					mupip_exit(ERR_MUNOTALLSEC);
				}
				if (-1 == shmctl(shmid, IPC_STAT, &shm_buf))
				{
					save_errno = errno;
					ISSUE_REPLPOOLINST(save_errno, shmid, instfilename, "shmctl(IPC_STAT)");
					mupip_exit(ERR_MUNOTALLSEC);
				}
				ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, shmid);
				*ipcs_ptr = '\0';
				if (0 == shm_buf.shm_nattch)
				{	/* No one else attached and no new process can attach (as we hold the ftok and access
					 * control semaphore on the journal pool)
					 */
					if (SS_NORMAL != shm_rmid(shmid))
					{
						save_errno = errno;
						ISSUE_REPLPOOLINST(save_errno, shmid, instfilename, "shmctl(IPC_RMID)");
						mupip_exit(ERR_MUNOTALLSEC);
					} else
					{
						gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNSUC, 4, LEN_AND_STR(ipcs_buff),
								LEN_AND_STR(instfilename));
						repl_instance.jnlpool_shmid = INVALID_SHMID;
						repl_instance.jnlpool_shmid_ctime = 0;
						shm_removed = TRUE;
					}
				} else
				{
					util_out_print("Replpool segment (id = !UL) for replication instance !AD is in"
							" use by another process.", TRUE, shmid, LEN_AND_STR(instfilename));
					gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNFL, 4, LEN_AND_STR(ipcs_buff),
								LEN_AND_STR(instfilename));
				}
				/* Now, go ahead and release/remove the access control semaphores */
				ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, semid);
				*ipcs_ptr = '\0';
				assert(INVALID_SEMID != repl_instance.jnlpool_semid);
				if (SS_NORMAL == mu_replpool_release_sem(&repl_instance, JNLPOOL_SEGMENT, shm_removed))
				{
					if (INVALID_SEMID == repl_instance.jnlpool_semid)
					{	/* Successfully removed the semaphore */
						assert(0 == repl_instance.jnlpool_semid_ctime);
						if (!jnlpool_sem_created)
						{
							gtm_putmsg(VARLSTCNT(9) ERR_MUJPOOLRNDWNSUC, 4, LEN_AND_STR(ipcs_buff),
									LEN_AND_STR(instfilename), ERR_SEMREMOVED, 1, semid);
						}
						repl_instance.crash = FALSE; /* No more semaphore IDs. Reset crash bit */
					}
				} else
				{	/* REPLACCESSSEM is issued from within mu_replpool_release_sem */
					assert(FALSE);
					mupip_exit(ERR_MUNOTALLSEC);
				}
				assert(shm_removed || (INVALID_SEMID != repl_instance.jnlpool_semid));
				assert(!shm_removed || (INVALID_SEMID == repl_instance.jnlpool_semid));
				repl_inst_write(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
			} else
			{	/* User did not request to rundown the journal pool as well. So, just decrement the counter
				 * semaphore. We've already released the access control semaphore.
				 */
				if (SS_NORMAL != decr_sem(SOURCE, SRC_SERV_COUNT_SEM))
				{
					save_errno = errno;
					ISSUE_REPLPOOLINST(save_errno, shmid, instfilename, "semop()");
					mupip_exit(ERR_MUNOTALLSEC);
				}
			}
			if (!ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, FALSE))
			{	/* CRITSEMFAIL is issued in case of an error */
				assert(FALSE);
				mupip_exit(ERR_MUNOTALLSEC);
			}
		}
	} else
	{
		argumentless_rundown = TRUE;
		/* Both "mu_rndwn_all" and "mu_rndwn_sem_all" do POPEN which opens an input stream (of type "FILE *").
		 * We have noticed that on HPUX, a call to "fflush NULL" (done inside gtm_putmsg which is called from
		 * the above two functions at various places) causes unread (but buffered) data from the input stream
		 * to be cleared/consumed resulting in incomplete processing of the input list of ipcs. To avoid this
		 * we set this global variable. That causes gtm_putmsg to skip the fflush NULL. We dont have an issue
		 * with out-of-order mixing of stdout and stderr streams (like is there with replication server logfiles)
		 * and so it is okay for this global variable to be set to TRUE for the entire lifetime of the argumentless
		 * rundown command. See <C9J02_003091_mu_rndwn_all_premature_termination_on_HPUX>.
		 */
		donot_fflush_NULL = TRUE;
		exit_status = mu_rndwn_all();
		if (SS_NORMAL == exit_status)
			exit_status = mu_rndwn_sem_all();
		else
			mu_rndwn_sem_all();
	}
	mupip_exit(exit_status);
}
