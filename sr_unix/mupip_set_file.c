/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_ipc.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_string.h"

#include <sys/sem.h>

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "db_header_conversion.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "cli.h"
#include "error.h"
#include "gtmio.h"
#include "iosp.h"
#include "jnl.h"
#include "mupipbckup.h"
#include "timersp.h"
#include "gt_timer.h"
#include "buddy_list.h"
#include "tp.h"
#include "util.h"
#include "mupip_set.h"
#include "mu_rndwn_file.h"
#include "mupip_exit.h"
#include "ipcrmid.h"
#include "mu_gv_cur_reg_init.h"
#include "gvcst_protos.h"	/* for "gvcst_init" prototype */
#include "timers.h"
#include "db_ipcs_reset.h"
#include "wcs_flu.h"
#include "gds_rundown.h"
#include "change_reg.h"
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "gtmcrypt.h"
#include "anticipatory_freeze.h"
#include "get_fs_block_size.h"
#include "interlock.h"
#include "min_max.h"
#include "mutex.h"		/* for "mutex_wakeup()" prototype */
#include "caller_id.h"		/* for "caller_id()" prototype (used in CRIT_TRACE macro) */

GBLREF	bool			in_backup;
GBLREF	bool			region;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	tp_region		*grlist;

LITREF char			*gtm_dbversion_table[];

#define GTMCRYPT_ERRLIT		"during GT.M startup"

error_def(ERR_ASYNCIONOMM);
error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTINIT2);
error_def(ERR_CRYPTNOMM);
error_def(ERR_DBBLKSIZEALIGN);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_DBRDERR);
error_def(ERR_DBRDONLY);
error_def(ERR_GTMCURUNSUPP);
error_def(ERR_INVACCMETHOD);
error_def(ERR_MUPIPSET2BIG);
error_def(ERR_MUPIPSET2SML);
error_def(ERR_NODFRALLOCSUPP);
error_def(ERR_NOUSERDB);
error_def(ERR_OFRZACTIVE);
error_def(ERR_READONLYNOBG);
error_def(ERR_SETQUALPROB);
error_def(ERR_TEXT);
error_def(ERR_WCERRNOTCHG);
error_def(ERR_WCWRNNOTCHG);

#define MAX_ACC_METH_LEN	2
#define MAX_KEY_SIZE		MAX_KEY_SZ - 4		/* internal and external maximums differ */
#define MIN_MAX_KEY_SZ		3

#define DO_CLNUP_AND_SET_EXIT_STAT(EXIT_STAT, EXIT_WRN_ERR_MASK)		\
MBSTART {									\
	exit_stat |= EXIT_WRN_ERR_MASK;							\
	db_ipcs_reset(gv_cur_region);						\
	mu_gv_cur_reg_free();							\
} MBEND

int4 mupip_set_file(int db_fn_len, char *db_fn)
{
	boolean_t		bypass_partial_recov, flush_buffers = FALSE, got_standalone, need_standalone = FALSE,
				acc_meth_changing, long_blk_id;
	char			acc_spec[MAX_ACC_METH_LEN + 1], *command = "MUPIP SET VERSION", *errptr, exit_stat, *fn,
				ver_spec[MAX_DB_VER_LEN + 1];
	enum db_acc_method	access, access_new;
	uint4			fbwsize;
	int4			dblksize;
	gd_region		*temp_cur_region;
	gd_segment		*seg;
	int			asyncio_status, defer_allocate_status, defer_status, disk_wait_status, d_rsrvd_bytes_status,
				encryptable_status,encryption_complete_status, epoch_taper_status, extn_count_status, fd,
				fn_len, glbl_buff_status, gtmcrypt_errno, hard_spin_status, inst_freeze_on_error_status,
				i_rsrvd_bytes_status, key_size_status, locksharesdbcrit,lock_space_status, mutex_space_status,
				null_subs_status, qdbrundown_status, rec_size_status, reg_exit_stat, rc, rsrvd_bytes_status,
				sleep_cnt_status, save_errno, stats_status, status, status1, stdb_alloc_status,
				stdnullcoll_status, trigger_flush_limit_status, wrt_per_flu_status, full_blkwrt_status,
				problksplit_status;
	int4			defer_time, d_reserved_bytes, i_reserved_bytes, new_cache_size, new_disk_wait, new_extn_count,
				new_flush_trigger, new_hard_spin, new_key_size, new_lock_space, new_mutex_space, new_null_subs = -1,
				new_rec_size, new_sleep_cnt, new_spin_sleep, new_statsdb_alloc, new_stdnullcoll, new_wrt_per_flu,
				reserved_bytes, spin_sleep_status, read_only_status, new_full_blkwrt, new_problksplit;
	int			reorg_sleep_nsec_status;
	sgmnt_data_ptr_t	csd, pvt_csd;
	tp_region		*rptr, single;
	uint4			reorg_sleep_nsec;
	uint4			fsb_size, reservedDBFlags;
	unsigned short		acc_spec_len = MAX_ACC_METH_LEN, ver_spec_len = MAX_DB_VER_LEN;
	ZOS_ONLY(int 		realfiletag;)
	int			mutex_type_status;
	mutex_type_t		old_mutex_type, new_mutex_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	exit_stat = EXIT_NRM;
	defer_status = cli_present("DEFER_TIME");
	if (defer_status)
		need_standalone = TRUE;
	/* If the user requested MUPIP SET -PARTIAL_RECOV_BYPASS, then skip the check in grab_crit, which triggers an rts_error, as
	 * this is one of the ways of turning off the file_corrupt flag in the file header
	 */
	TREF(skip_file_corrupt_check) = bypass_partial_recov = cli_present("PARTIAL_RECOV_BYPASS") == CLI_PRESENT;
	if (bypass_partial_recov)
		need_standalone = TRUE;
	if (cli_present("ACCESS_METHOD"))
	{
		cli_get_str("ACCESS_METHOD", acc_spec, &acc_spec_len);
		acc_spec[acc_spec_len] = '\0';
		cli_strupper(acc_spec);
		if (0 == memcmp(acc_spec, "MM", acc_spec_len))
			access = dba_mm;
		else  if (0 == memcmp(acc_spec, "BG", acc_spec_len))
			access = dba_bg;
		else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVACCMETHOD);
			exit_stat |= EXIT_ERR;
			access = dba_dummy;
		}
		need_standalone = TRUE;
	} else
		access = n_dba;		/* really want to keep current method,
					    which has not yet been read */
	if ((asyncio_status = cli_present("ASYNCIO")))
		need_standalone = TRUE;
	if ((defer_allocate_status = cli_present("DEFER_ALLOCATE")))
		flush_buffers = TRUE;
	if ((encryptable_status = cli_present("ENCRYPTABLE")))
	{
		need_standalone = TRUE;
		if (CLI_PRESENT == encryptable_status)
		{	/* When turning on encryption, validate the encryption setup */
			INIT_PROC_ENCRYPTION(gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				CLEAR_CRYPTERR_MASK(gtmcrypt_errno);
				assert(!IS_REPEAT_MSG_MASK(gtmcrypt_errno));
				assert((ERR_CRYPTDLNOOPEN == gtmcrypt_errno) || (ERR_CRYPTINIT == gtmcrypt_errno));
				if (ERR_CRYPTDLNOOPEN == gtmcrypt_errno)
					gtmcrypt_errno = ERR_CRYPTDLNOOPEN2;
				else if (ERR_CRYPTINIT == gtmcrypt_errno)
					gtmcrypt_errno = ERR_CRYPTINIT2;
				gtmcrypt_errno = SET_CRYPTERR_MASK(gtmcrypt_errno);
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, SIZEOF(GTMCRYPT_ERRLIT) - 1,
						GTMCRYPT_ERRLIT);
			}
		} else /* When disabling encryption ignore invalid encryption setup errors */
			TREF(mu_set_file_noencryptable) = TRUE;
	}
	if ((read_only_status = cli_present("READ_ONLY")) /* Note assignment */)
		need_standalone = TRUE;
	encryption_complete_status = cli_present("ENCRYPTIONCOMPLETE");
	epoch_taper_status = cli_present("EPOCHTAPER");
	if ((problksplit_status = cli_present("PROBLKSPLIT")))
		if (!cli_get_int("PROBLKSPLIT", &new_problksplit))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("PROBLKSPLIT"));
			exit_stat |= EXIT_ERR;
		}
	/* EXTENSION_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if ((extn_count_status = cli_present("EXTENSION_COUNT")))
	{
		if (cli_get_int("EXTENSION_COUNT", &new_extn_count))
		{	/* minimum is 0 & mupip_cmd defines this qualifier to not accept negative values, so no min check */
			if (new_extn_count > MAX_EXTN_COUNT)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_extn_count,
					LEN_AND_LIT("EXTENSION_COUNT"), MAX_EXTN_COUNT);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("EXTENSION COUNT"));
			exit_stat |= EXIT_ERR;
		}
		flush_buffers = TRUE;
	}
	if ((full_blkwrt_status = cli_present("FULLBLKWRT")))
	{
		if (cli_get_int("FULLBLKWRT", &new_full_blkwrt))
		{
			/* minimum is 0 & mupip_cmd defines this qualifier to not accept negative values, so no min check */
			if (new_full_blkwrt > FULL_DATABASE_WRITE)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_full_blkwrt,
					LEN_AND_LIT("FULLBLKWRT"), 2);
				exit_stat |= EXIT_ERR;
			}
			need_standalone = TRUE;
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("FULLBLKWRT"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((glbl_buff_status = cli_present("GLOBAL_BUFFERS")))
	{
		if (cli_get_int("GLOBAL_BUFFERS", &new_cache_size))
		{
			if (new_cache_size > GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_cache_size,
					LEN_AND_LIT("GLOBAL_BUFFERS"),GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS));
				exit_stat |= EXIT_ERR;
			}
			if (new_cache_size < WC_MIN_BUFFS)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_cache_size,
					LEN_AND_LIT("GLOBAL_BUFFERS"), WC_MIN_BUFFS);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("GLOBAL_BUFFERS"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	if ((hard_spin_status = cli_present("HARD_SPIN_COUNT")))
	{	/* No min or max tests needed because mupip_cmd enforces min of 0 and no max requirement is documented*/
		if (!cli_get_int("HARD_SPIN_COUNT", &new_hard_spin))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("HARD_SPIN_COUNT"));
			exit_stat |= EXIT_ERR;
		}
	}
	inst_freeze_on_error_status = cli_present("INST_FREEZE_ON_ERROR");
	if ((key_size_status = cli_present("KEY_SIZE")))
	{
		if (cli_get_int("KEY_SIZE", &new_key_size))
		{
			if (MAX_KEY_SIZE < new_key_size)
			{	/* bigger than 1019 not supported */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_key_size,
					LEN_AND_LIT("KEY_SIZE"), MAX_KEY_SIZE);
				exit_stat |= EXIT_ERR;
			}
			if (MIN_MAX_KEY_SZ > new_key_size)
			{	/* less than 3 not supported */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_key_size,
					LEN_AND_LIT("KEY_SIZE"), MIN_MAX_KEY_SZ);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("KEY_SIZE"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	if ((reorg_sleep_nsec_status = cli_present("REORG_SLEEP_NSEC")))
	{
		if (cli_get_int("REORG_SLEEP_NSEC", (int4 *)&reorg_sleep_nsec))
		{
			if (NANOSECS_IN_SEC <= reorg_sleep_nsec)
			{	/* >= NANOSECS_IN_SEC not supported */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, reorg_sleep_nsec,
					LEN_AND_LIT("REORG_SLEEP_NSEC"), NANOSECS_IN_SEC - 1);
				exit_stat |= EXIT_ERR;
			}
			/* Minimum value supported is 0 so no minimum checks needed since < 0 is a negative number
			 * which cannot currently be passed as input through the CLI anyways.
			 */
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("REORG_SLEEP_NSEC"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((locksharesdbcrit = cli_present("LCK_SHARES_DB_CRIT")))
		need_standalone = TRUE;
	if ((lock_space_status = cli_present("LOCK_SPACE")))
	{
		if (cli_get_int("LOCK_SPACE", &new_lock_space))
		{
			if (new_lock_space > MAX_LOCK_SPACE)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_lock_space,
					LEN_AND_LIT("LOCK_SPACE"), MAX_LOCK_SPACE);
				exit_stat |= EXIT_ERR;
			}
			else if (new_lock_space < MIN_LOCK_SPACE)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_lock_space,
					LEN_AND_LIT("LOCK_SPACE"), MIN_LOCK_SPACE);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("LOCK_SPACE"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	if ((mutex_space_status = cli_present("MUTEX_SLOTS")))
	{
		if (cli_get_int("MUTEX_SLOTS", &new_mutex_space))
		{
			if (new_mutex_space > MAX_CRIT_ENTRY)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_mutex_space,
					LEN_AND_LIT("MUTEX_SLOTS"), MAX_CRIT_ENTRY);
				exit_stat |= EXIT_ERR;
			} else if (new_mutex_space < MIN_CRIT_ENTRY)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_mutex_space,
					LEN_AND_LIT("MUTEX_SLOTS"), MIN_CRIT_ENTRY);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("MUTEX_SLOTS"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	if ((mutex_type_status = cli_present("MUTEX_TYPE")))	/* NOTE: assignment */
	{
		if (cli_present("MUTEX_TYPE.ADAPTIVE"))
			new_mutex_type = mutex_type_adaptive_ydb;
		else if (cli_present("MUTEX_TYPE.PTHREAD"))
			new_mutex_type = mutex_type_pthread;
		else
		{
			assert(cli_present("MUTEX_TYPE.YDB"));
			new_mutex_type = mutex_type_ydb;
		}
	}
	if ((null_subs_status = cli_present("NULL_SUBSCRIPTS")))
	{
		if (-1 == (new_null_subs = cli_n_a_e("NULL_SUBSCRIPTS")))
			exit_stat |= EXIT_ERR;
		need_standalone = TRUE;
	}
	if ((qdbrundown_status = cli_present("QDBRUNDOWN")))
		need_standalone = TRUE;
	if ((rec_size_status = cli_present("RECORD_SIZE")))
	{
		if (cli_get_int("RECORD_SIZE", &new_rec_size))
		{	/* minimum is 0 & mupip_cmd defines this qualifier to not accept negative values, so no min check */
			if (MAX_STRLEN < new_rec_size)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_rec_size,
					LEN_AND_LIT("RECORD_SIZE"), MAX_STRLEN);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("RECORD_SIZE"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	if ((rsrvd_bytes_status = cli_present("RESERVED_BYTES")))
	{
		if (!cli_get_int("RESERVED_BYTES", &reserved_bytes))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("RESERVED_BYTES"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((i_rsrvd_bytes_status = cli_present("INDEX_RESERVED_BYTES")))
	{
		if (!cli_get_int("INDEX_RESERVED_BYTES", &i_reserved_bytes))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("INDEX_RESERVED_BYTES"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((d_rsrvd_bytes_status = cli_present("DATA_RESERVED_BYTES")))
	{
		if (!cli_get_int("DATA_RESERVED_BYTES", &d_reserved_bytes))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("DATA_RESERVED_BYTES"));
			exit_stat |= EXIT_ERR;
		}
	}
	/* SLEEP_SPIN_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if ((sleep_cnt_status = cli_present("SLEEP_SPIN_COUNT")))
	{
		if (cli_get_int("SLEEP_SPIN_COUNT", &new_sleep_cnt))
		{	/* minimum is 0 & mupip_cmd defines this qualifier to not accept negative values, so no min check */
			if (new_sleep_cnt > MAX_SLEEP_CNT)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_sleep_cnt,
					LEN_AND_LIT("SLEEP_SPIN_COUNT"), MAX_SLEEP_CNT);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("SLEEP_SPIN_COUNT"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((spin_sleep_status = cli_present("SPIN_SLEEP_MASK")))
	{
		if (cli_get_hex("SPIN_SLEEP_MASK", (uint4 *) &new_spin_sleep))
		{
			/* minimum is 0 and mupip_cmd defines this qualifier to only accept hex values no min check*/
			if ((uint4) new_spin_sleep > MAX_SPIN_SLEEP_MASK)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_spin_sleep,
					LEN_AND_LIT("SPIN_SLEEP_MASK"), MAX_SPIN_SLEEP_MASK);
				exit_stat |= EXIT_ERR;
			}
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("SPIN_SLEEP_MASK"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((stats_status = cli_present("STATS")))
		need_standalone = TRUE;
	if ((stdb_alloc_status = cli_present("STATSDB_ALLOCATION")))
	{
		if (cli_get_int("STATSDB_ALLOCATION", &new_statsdb_alloc))
		{
			if (new_statsdb_alloc > STDB_ALLOC_MAX)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_statsdb_alloc,
					LEN_AND_LIT("STATSDB_ALLOCATION"), STDB_ALLOC_MAX);
				exit_stat |= EXIT_ERR;
			}
			if (new_statsdb_alloc < STDB_ALLOC_MIN)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_statsdb_alloc,
					LEN_AND_LIT("STATSDB_ALLOCATION"), STDB_ALLOC_MIN);
				exit_stat |= EXIT_ERR;
			}
			need_standalone = TRUE;
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("STATSDB_ALLOCATION"));
			exit_stat |= EXIT_ERR;
		}
	}
	if ((stdnullcoll_status = cli_present("STDNULLCOLL")))
		need_standalone = TRUE;
	if (cli_present("VERSION"))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMCURUNSUPP);
		exit_stat |= EXIT_ERR;
		flush_buffers = TRUE;
	}
	if ((disk_wait_status = cli_present("WAIT_DISK")))
	{
		if (cli_get_int("WAIT_DISK", &new_disk_wait))
		{
			if (new_disk_wait < 0)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_disk_wait,
					LEN_AND_LIT("WAIT_DISK"), 0);
				exit_stat |= EXIT_ERR;
			}
			need_standalone = TRUE;
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("WAIT_DISK"));
			exit_stat |= EXIT_ERR;
		}
	}
	if (exit_stat & EXIT_ERR)
		return (int4)ERR_WCERRNOTCHG;
	if (region)
		rptr = grlist;
	else
	{
		rptr = &single;
		memset(&single, 0, SIZEOF(single));
	}
	pvt_csd = (sgmnt_data *)malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	pvt_csd->read_only = FALSE;
	in_backup = FALSE;		/* Only want yes/no from mupfndfil, not an address */
	for (;  rptr != NULL;  rptr = rptr->fPtr)
	{
		reg_exit_stat = EXIT_NRM;
		if (region)
		{
			assert(dba_usr != REG_ACC_METH(rptr->reg));
			if (!mupfndfil(rptr->reg, NULL, LOG_ERROR_TRUE))
				continue;
			fn = (char *)rptr->reg->dyn.addr->fname;
			fn_len = rptr->reg->dyn.addr->fname_len;
		} else
		{
			fn = db_fn;
			fn_len = db_fn_len;
		}
		mu_gv_cur_reg_init();
		memcpy(gv_cur_region->dyn.addr->fname, fn, fn_len);
		gv_cur_region->dyn.addr->fname_len = fn_len;
		acc_meth_changing = FALSE;
		if (need_standalone)
		{	/* Following part needs standalone access */
			got_standalone = STANDALONE(gv_cur_region);
			if (FALSE == got_standalone)
			{
				exit_stat |= EXIT_WRN;
				mu_gv_cur_reg_free();
				continue;
			}
			/* we should open it (for changing) after mu_rndwn_file, since mu_rndwn_file changes the file header too */
			if (FD_INVALID == (fd = OPEN(fn, O_RDWR)))	/* udi not available so OPENFILE_DB not used */
			{
				save_errno = errno;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
				DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_ERR);
				continue;
			}
#			ifdef __MVS__
			if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
				TAG_POLICY_GTM_PUTMSG(fn, realfiletag, TAG_BINARY, errno);
#			endif
			LSEEKREAD(fd, 0, pvt_csd, SIZEOF(sgmnt_data), status);
			if (0 == memcmp(pvt_csd->label, V6_GDS_LABEL, GDS_LABEL_SZ -1))
				db_header_upconv(pvt_csd);
			if (0 != status)
			{
				save_errno = errno;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
				if (-1 != status)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
				else
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
				DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_ERR);
				continue;
			}
			assert(dba_dummy != access);
			if ((n_dba != access) && (pvt_csd->acc_meth != access))	/* n_dba is a proxy for no change */
			{
				acc_meth_changing = TRUE;
				if (dba_mm == access)
				{
					if (USES_ENCRYPTION(pvt_csd->is_encrypted))
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOMM, 2,
											DB_LEN_STR(gv_cur_region));
						DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_ERR);
						continue;
					}
					if (pvt_csd->asyncio && (CLI_NEGATED != asyncio_status))
					{	/* ASYNCIO=ON */
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ASYNCIONOMM, 6,
							DB_LEN_STR(gv_cur_region), LEN_AND_LIT(" has ASYNCIO enabled;"),
							LEN_AND_LIT("enable MM"));
					} else if (!pvt_csd->asyncio && (CLI_PRESENT == asyncio_status))
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ASYNCIONOMM, 6,
							DB_LEN_STR(gv_cur_region), LEN_AND_LIT(";"),
							LEN_AND_LIT("enable MM and ASYNCIO at the same time"));
						reg_exit_stat |= EXIT_WRN;
					}
					pvt_csd->defer_time = 1;			/* defer defaults to 1 */
				} else
				{	/* Setting access method to BG. Check for incompatibilities. */
					if (!read_only_status && pvt_csd->read_only)
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_READONLYNOBG);
						reg_exit_stat |= EXIT_WRN;
					}
				}
				pvt_csd->acc_meth = access;
				if (0 == pvt_csd->n_bts)
				{
					pvt_csd->n_bts = WC_DEF_BUFFS;
					pvt_csd->bt_buckets = getprime(pvt_csd->n_bts);
				}
			}
			access_new = (n_dba == access ? pvt_csd->acc_meth : access);
			if (dba_mm == access_new)
			{
				if (CLI_NEGATED == defer_status)
					pvt_csd->defer_time = 0;
				else  if (CLI_PRESENT == defer_status)
				{
					if (cli_get_defertime("DEFER_TIME", &defer_time))
					{
						if (-1 > defer_time)
						{
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, defer_time,
								LEN_AND_LIT("DEFER_TIME"), -1);
							reg_exit_stat |= EXIT_WRN;
						} else
							pvt_csd->defer_time = defer_time;
					} else
					{
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2,
							LEN_AND_LIT("DEFER_TIME"));
						reg_exit_stat |= EXIT_WRN;
					}
				}
				if (pvt_csd->offset && (GDSV6p == pvt_csd->desired_db_format))
				{	/* if we support downgrade, enhance the above conditions */
					util_out_print("MM access method cannot be set during DB upgrade",
						TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				}
				if (JNL_ENABLED(pvt_csd))
				{
					if (pvt_csd->jnl_before_image)
					{
						util_out_print("MM access method cannot be set with BEFORE image journaling", TRUE);
						util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
						reg_exit_stat |= EXIT_WRN;
					}
				} else if (acc_meth_changing)
				{	/* Journaling is not ON now. But access method is going to be change and be MM.
					 * Set default journal type to be NOBEFORE_IMAGE journaling as that is the
					 * only option compatible with MM (will be used by a later MUPIP SET JOURNAL command).
					 */
					pvt_csd->jnl_before_image = FALSE;
				}
			} else
			{
				if (defer_status)
				{
					util_out_print("DEFER cannot be specified with BG access method.", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				}
				if (!JNL_ENABLED(pvt_csd) && acc_meth_changing)
				{	/* Journaling is not ON now. But access method is going to change and be BG.
					 * Set default journal type to be BEFORE_IMAGE journaling (will be used by a
					 * later MUPIP SET JOURNAL command).
					 */
					pvt_csd->jnl_before_image = TRUE;
				}
			}
			if (bypass_partial_recov)
			{
				pvt_csd->file_corrupt = FALSE;
				util_out_print("Database file !AD now has partial recovery flag set to  !UL(FALSE) ",
					TRUE, fn_len, fn, pvt_csd->file_corrupt);
			}
			if (encryptable_status)
			{
				assert(pvt_csd->fully_upgraded || (0 == pvt_csd->blks_to_upgrd)
						|| (BLK_ID_32_VER < pvt_csd->desired_db_format));
				if (dba_mm == access_new)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOMM, 2, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				} else if (CLI_PRESENT == encryptable_status)
					MARK_AS_TO_BE_ENCRYPTED(pvt_csd->is_encrypted);
				else if (USES_NEW_KEY(pvt_csd))
				{
					util_out_print("Database file !AD is being (re)encrypted and must stay encryptable",
							TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				} else if (IS_ENCRYPTED(pvt_csd->is_encrypted))
					SET_AS_ENCRYPTED(pvt_csd->is_encrypted);
				else
					SET_AS_UNENCRYPTED(pvt_csd->is_encrypted);
			}
			if (glbl_buff_status)
			{
				pvt_csd->n_bts = BT_FACTOR(new_cache_size);
				pvt_csd->bt_buckets = getprime(pvt_csd->n_bts);
				pvt_csd->flush_trigger = FLUSH_FACTOR(pvt_csd->n_bts);
			}
			if (key_size_status)
			{
				long_blk_id = BLK_ID_32_VER < pvt_csd->desired_db_format;
				key_size_status = pvt_csd->blk_size - SIZEOF(blk_hdr) - SIZEOF(rec_hdr) -
					SIZEOF_BLK_ID(long_blk_id) - bstar_rec_size(long_blk_id) -
					(MAX(pvt_csd->reserved_bytes, pvt_csd->i_reserved_bytes));
				if (key_size_status < new_key_size)
				{	/* too big for block */
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_MUPIPSET2BIG, 4, new_key_size,
						LEN_AND_LIT("KEY_SIZE"), key_size_status,
						ERR_TEXT, 2, RTS_ERROR_TEXT("for current block size and reserved bytes"));
					reg_exit_stat |= EXIT_WRN;
				} else if (pvt_csd->max_key_size > new_key_size)
				{	/* lowering the maximum key size can cause problems if large keys already exist in db */
					util_out_print("!UL smaller than current maximum key size !UL", TRUE,
							new_key_size, pvt_csd->max_key_size);
					reg_exit_stat |= EXIT_WRN;
				}
				pvt_csd->max_key_size = new_key_size;
			}
			if (locksharesdbcrit)
				pvt_csd->lock_crit_with_db = CLI_PRESENT == locksharesdbcrit;
			if (lock_space_status)
				pvt_csd->lock_space_size = (uint4)new_lock_space * OS_PAGELET_SIZE;
			if (mutex_space_status)
				NUM_CRIT_ENTRY(pvt_csd) = new_mutex_space;
			if (null_subs_status)
			{
				assert(-1 != new_null_subs);
				gv_cur_region->null_subs = pvt_csd->null_subs = (unsigned char)new_null_subs;
			}
			if (qdbrundown_status)
				pvt_csd->mumps_can_bypass = (CLI_PRESENT == qdbrundown_status);
			if (rec_size_status)
			{
				if (pvt_csd->max_rec_size > new_rec_size)
				{
					util_out_print("!UL smaller than current maximum record size !UL", TRUE,
							new_rec_size, pvt_csd->max_rec_size);
					reg_exit_stat |= EXIT_WRN;
				}
				pvt_csd->max_rec_size = new_rec_size;
			}
			if (stats_status)
			{
				reservedDBFlags = pvt_csd->reservedDBFlags & ~RDBF_NOSTATS;
				if (CLI_NEGATED == stats_status)
					reservedDBFlags |= RDBF_NOSTATS;
				else if (!read_only_status && pvt_csd->read_only)
				{	/* READ_ONLY has not been specified on a database that is already READ_ONLY
					 * but STATS is now being enabled. Issue error as the two are incompatible.
					 */
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_READONLYNOSTATS);
					reg_exit_stat |= EXIT_WRN;
				}
				pvt_csd->reservedDBFlags = reservedDBFlags;
			}
			if (stdb_alloc_status)
			{
				pvt_csd->statsdb_allocation = new_statsdb_alloc;
			}
			/* Now that we know what the new STATS setting is going to be, check for READ_ONLY */
			if (CLI_PRESENT == read_only_status)
			{
				/* Check if new access method is MM. If so issue error */
				if (dba_mm != pvt_csd->acc_meth)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_READONLYNOBG);
					reg_exit_stat |= EXIT_WRN;
				}
				/* Check if new statistics sharing is turned ON. If so issue error */
				if (!(RDBF_NOSTATS & pvt_csd->reservedDBFlags))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_READONLYNOSTATS);
					reg_exit_stat |= EXIT_WRN;
				}
			}
			if (read_only_status)
				pvt_csd->read_only = !(CLI_NEGATED == read_only_status);
			if (CLI_NEGATED == stdnullcoll_status)
			{
				gv_cur_region->std_null_coll = FALSE;
				pvt_csd->std_null_coll = FALSE;
			} else if (CLI_PRESENT == stdnullcoll_status)
			{
				gv_cur_region->std_null_coll = TRUE;
				pvt_csd->std_null_coll = TRUE;
			}
			if (EXIT_NRM != reg_exit_stat)
			{
				DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, reg_exit_stat);
				continue;
			}
			if (full_blkwrt_status)
			{
				fbwsize = get_fs_block_size(fd);
				dblksize = pvt_csd->blk_size;
				if (0 != fbwsize && (0 == dblksize % fbwsize) &&
						(0 == (BLK_ZERO_OFF(pvt_csd->start_vbn)) % fbwsize))
					pvt_csd->write_fullblk = new_full_blkwrt;
			}
			csd = pvt_csd;
		} else /* if (!need_standalone) */
		{
			got_standalone = FALSE;
			if (region)
			{	/* We have a region from a gld file. Find out asyncio setting from there. And copy that
				 * over to the dummy region we created. This is to avoid DBGLDMISMATCH errors inside "gvcst_init".
				 */
				COPY_AIO_SETTINGS(gv_cur_region->dyn.addr, rptr->reg->dyn.addr); /* copies from rptr->reg->dyn.addr
												  * to gv_cur_region->dyn.addr
												  */
			} else
			{	/* We do not have a region from a gld file. All we have is the name of the db file.
				 * "db_init" (invoked by "gvcst_init") takes care of initializing the "asyncio" field
				 * as appropriate.
				 */
			}
			gvcst_init(gv_cur_region);
			change_reg();	/* sets cs_addrs and cs_data */
			if (gv_cur_region->read_only)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
				exit_stat |= EXIT_ERR;
				gds_rundown(CLEANUP_UDI_TRUE);
				mu_gv_cur_reg_free();
				continue;
			}
			csd = cs_data;
			assert(!cs_addrs->hold_onto_crit); /* this ensures we can safely do unconditional grab_crit and rel_crit */
			grab_crit(gv_cur_region, WS_95);
			if (FROZEN_CHILLED(cs_addrs))
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_OFRZACTIVE, 2,
							DB_LEN_STR(gv_cur_region));
				exit_stat |= EXIT_WRN;
				exit_stat |= gds_rundown(CLEANUP_UDI_TRUE);
				mu_gv_cur_reg_free();
				continue;
			}
			fd = FD_INVALID;
		}
		assert(dba_dummy != access);
		access_new = (n_dba == access ? csd->acc_meth : access);
		if (encryption_complete_status)
		{
			assert(csd->fully_upgraded || (0 == csd->blks_to_upgrd) || (BLK_ID_32_VER < csd->desired_db_format));
			if (dba_mm == access_new)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOMM, 2, fn_len, fn);
				reg_exit_stat |= EXIT_WRN;
			} else if (((NULL == cs_addrs->nl) || (!cs_addrs->nl->reorg_encrypt_pid)) && (!USES_NEW_KEY(csd)))
				csd->encryption_hash2_start_tn = 0;
			else
			{
				util_out_print("Cannot mark encryption complete on database file !AD due to"
						" an ongoing MUPIP REORG -ENCRYPT operation", TRUE, fn_len, fn);
				reg_exit_stat |= EXIT_WRN;
			}
		}
		if (asyncio_status && (CLI_PRESENT == asyncio_status))
		{
			assert(csd->fully_upgraded || (0 == csd->blks_to_upgrd) || (BLK_ID_32_VER < csd->desired_db_format));
			if (!acc_meth_changing && (dba_bg != access_new))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ASYNCIONOMM, 6, DB_LEN_STR(gv_cur_region),
					LEN_AND_LIT(" has MM access method;"), LEN_AND_LIT("enable ASYNCIO"));
				reg_exit_stat |= EXIT_WRN;
			}
			/* AIO = ON, implies we need to use O_DIRECT. Check for db vs fs blksize alignment issues. */
			assert(!got_standalone || (FD_INVALID != fd));
			fsb_size = get_fs_block_size(got_standalone ? fd : FILE_INFO(gv_cur_region)->fd);
			if (0 != (csd->blk_size % fsb_size))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBBLKSIZEALIGN, 4,
							DB_LEN_STR(gv_cur_region), csd->blk_size, fsb_size);
				reg_exit_stat |= EXIT_WRN;
			}
		}
		if ((wrt_per_flu_status = (CLI_PRESENT == cli_present("WRITES_PER_FLUSH"))))
		{
			if (cli_get_int("WRITES_PER_FLUSH", &new_wrt_per_flu))
			{
				if (0 >= new_wrt_per_flu)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_wrt_per_flu,
						       LEN_AND_LIT("WRITES_PER_FLUSH"), 1);
					reg_exit_stat |= EXIT_ERR;
				} else if (MAX_WRT_PER_FLU < new_wrt_per_flu)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_wrt_per_flu,
						       LEN_AND_LIT("WRITES_PER_FLUSH"), MAX_WRT_PER_FLU);
					reg_exit_stat |= EXIT_ERR;
				}
			} else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("WRITES_PER_FLUSH"));
				reg_exit_stat |= EXIT_ERR;
			}
		}
		if ((trigger_flush_limit_status = (CLI_PRESENT == cli_present("TRIGGER_FLUSH_LIMIT"))))
		{
			if (cli_get_int("TRIGGER_FLUSH_LIMIT", &new_flush_trigger))
			{
				if (glbl_buff_status ? (MIN_FLUSH_TRIGGER(BT_FACTOR(new_cache_size)) > new_flush_trigger)
					: (MIN_FLUSH_TRIGGER(cs_data->n_bts) > new_flush_trigger))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2SML, 4, new_flush_trigger,
						LEN_AND_LIT("TRIGGER_FLUSH_LIMIT"), glbl_buff_status
						? MIN_FLUSH_TRIGGER(BT_FACTOR(new_cache_size)) : MIN_FLUSH_TRIGGER(cs_data->n_bts));
					reg_exit_stat |= EXIT_ERR;
				} else if (glbl_buff_status ? (FLUSH_FACTOR(BT_FACTOR(new_cache_size)) < new_flush_trigger)
					: (FLUSH_FACTOR(cs_data->n_bts) < new_flush_trigger))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, new_flush_trigger,
						       LEN_AND_LIT("TRIGGER_FLUSH_LIMIT"), glbl_buff_status
						       ? FLUSH_FACTOR(BT_FACTOR(new_cache_size)) : FLUSH_FACTOR(cs_data->n_bts));
					reg_exit_stat |= EXIT_ERR;
				}
			} else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("TRIGGER_FLUSH_LIMIT"));
				reg_exit_stat |= EXIT_ERR;
			}
		}
		if (rsrvd_bytes_status)
		{
			long_blk_id = BLK_ID_32_VER < csd->desired_db_format;
			if (reserved_bytes > MAX_RESERVE_B(csd, long_blk_id))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, reserved_bytes,
						LEN_AND_LIT("RESERVED_BYTES"), MAX_RESERVE_B(csd, long_blk_id));
				reg_exit_stat |= EXIT_WRN;
			} else
			{
				csd->reserved_bytes = reserved_bytes;
				csd->i_reserved_bytes = reserved_bytes;
			}
		}
		if (i_rsrvd_bytes_status)
		{
			long_blk_id = BLK_ID_32_VER < csd->desired_db_format;
			if (i_reserved_bytes > MAX_RESERVE_B(csd, long_blk_id))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, i_reserved_bytes,
						LEN_AND_LIT("INDEX_RESERVED_BYTES"), MAX_RESERVE_B(csd, long_blk_id));
				reg_exit_stat |= EXIT_WRN;
			} else
				csd->i_reserved_bytes = i_reserved_bytes;
		}
		if (d_rsrvd_bytes_status)
		{
			long_blk_id = BLK_ID_32_VER < csd->desired_db_format;
			if (d_reserved_bytes > MAX_RESERVE_B(csd, long_blk_id))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, d_reserved_bytes,
						LEN_AND_LIT("DATA_RESERVED_BYTES"), MAX_RESERVE_B(csd,long_blk_id));
				reg_exit_stat |= EXIT_WRN;
			} else
				csd->reserved_bytes = d_reserved_bytes;
		}
		if (EXIT_NRM == reg_exit_stat)
		{
			if (n_dba != access)
				util_out_print("Database file !AD now has !AD access method", TRUE,
					       fn_len, fn, 2, ((dba_bg == csd->acc_meth) ? "BG" : "MM"));
			if (defer_allocate_status)
			{
#				if defined(__sun) || defined(__hpux)
				if (CLI_NEGATED == defer_allocate_status)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NODFRALLOCSUPP);
#				else
				csd->defer_allocate = (CLI_PRESENT == defer_allocate_status);
				util_out_print("Database file !AD now has defer allocation flag set to !AD", TRUE,
					fn_len, fn, 5, (csd->defer_allocate ? " TRUE" : "FALSE"));
#				endif
			}
			if (disk_wait_status)
				csd->wait_disk_space = new_disk_wait;
			if (epoch_taper_status)
				csd->epoch_taper = (CLI_PRESENT == epoch_taper_status);
			if (asyncio_status)
				csd->asyncio = (CLI_PRESENT == asyncio_status);
			if (problksplit_status)
				csd->problksplit = (uint4)new_problksplit;
			if (extn_count_status)
				csd->extension_size = (uint4)new_extn_count;
			change_fhead_timer_ns("FLUSH_TIME", &csd->flush_time,
					   (dba_bg == access_new ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM),
					   FALSE);
			if (CLI_NEGATED == inst_freeze_on_error_status)
				csd->freeze_on_fail = FALSE;
			else if (CLI_PRESENT == inst_freeze_on_error_status)
				csd->freeze_on_fail = TRUE;
			if (hard_spin_status)
				HARD_SPIN_COUNT(csd) = new_hard_spin;
			if (sleep_cnt_status)
				SLEEP_SPIN_CNT(csd) = new_sleep_cnt;
			if (spin_sleep_status)
				SPIN_SLEEP_MASK(csd) = new_spin_sleep;
			if (trigger_flush_limit_status)
				csd->flush_trigger = csd->flush_trigger_top = new_flush_trigger;
			if (wrt_per_flu_status)
				csd->n_wrt_per_flu = new_wrt_per_flu;
			if (reorg_sleep_nsec_status)
				csd->reorg_sleep_nsec = reorg_sleep_nsec;
			if (mutex_type_status)
			{
				mutex_struct_ptr_t	addr;
				node_local_ptr_t	cnl;

				/* We can change the mutex type in the file header safely now.
				 * But if we do not have standalone access, we need to change the mutex type in
				 * the database shared memory as well. And that needs to be done carefully so
				 * a later "rel_crit()" call will release the right mutex lock.
				 */
				old_mutex_type = csd->mutex_type;
				csd->mutex_type = new_mutex_type;

				if (!got_standalone)
				{
					/* Get the new type of mutex lock if it is different than the old/current type */
					assert(cs_addrs->now_crit);
					assert(process_id == cs_addrs->nl->in_crit);
					addr = cs_addrs->critical;
					if (IS_MUTEX_TYPE_PTHREAD(old_mutex_type))
					{
						if (!IS_MUTEX_TYPE_PTHREAD(new_mutex_type))
						{	/* Grab the YDB type mutex lock. We cannot use "grab_crit()"
							 * since we have not yet changed addr->curr_mutex_type. Note that
							 * it is possible some other process is holding this lock for a
							 * short duration (because it has still not seen the transition from
							 * ydb mutex to pthread mutex that already happened either automatically
							 * or through a MUPIP SET -MUTEX_TYPE=PTHREAD command). Therefore we need
							 * to wait for this lock (i.e. one GET_SWAPLOCK() call is not enough).
							 */
							grab_latch(&addr->semaphore,
								GRAB_LATCH_INDEFINITE_WAIT, NOT_APPLICABLE, cs_addrs);
							assert(process_id == addr->semaphore.u.parts.latch_pid);
							CRIT_TRACE(cs_addrs, crit_ops_gw_ydb_mutex);
						}
					} else
					{
						if (!IS_MUTEX_TYPE_YDB(new_mutex_type))
						{	/* Grab the PTHREAD type mutex lock. We cannot use "grab_crit()"
							 * since we have not yet changed addr->curr_mutex_type. Note that
							 * it is possible some other process is holding this lock for a
							 * short duration (because it has still not seen the transition from
							 * pthread mutex to ydb mutex that already happened either automatically
							 * or through a MUPIP SET -MUTEX_TYPE=YDB command). Therefore we need
							 * to wait for this lock (i.e. pthread_mutex_trylock() is not enough).
							 */
							status = pthread_mutex_lock(&addr->mutex);
							assert(0 == status);
							CRIT_TRACE(cs_addrs, crit_ops_gw_pthread_mutex);
						}
					}
					/* Change the mutex type now that we hold both types of locks */
					addr->curr_mutex_type = new_mutex_type;
					/* Reset counters that the adaptive ydb and adaptive pthread algorithms rely on */
					cnl = cs_addrs->nl;
					cnl->prev_n_crit_que_slps = cnl->gvstats_rec.n_crit_que_slps;
					cnl->prev_n_crit_failed = cnl->gvstats_rec.n_crit_failed;
					cnl->switch_streak = 0;
					/* Release the old mutex type of lock */
					/* A later "rel_crit()" will take care of releasing the new mutex type of lock */
					if (IS_MUTEX_TYPE_PTHREAD(old_mutex_type))
					{
						if (!IS_MUTEX_TYPE_PTHREAD(new_mutex_type))
						{
							CRIT_TRACE(cs_addrs, crit_ops_rw_pthread_mutex);
							status = pthread_mutex_unlock(&addr->mutex);
							assert(0 == status);
						}
					} else
					{
						if (!IS_MUTEX_TYPE_YDB(new_mutex_type))
						{
							CRIT_TRACE(cs_addrs, crit_ops_rw_ydb_mutex);
							assert(process_id == addr->semaphore.u.parts.latch_pid);
							RELEASE_SWAPLOCK(&addr->semaphore);
							/* Wake up all processes in msem wait queue
							 * so they can switch to pthread mutex.
							 */
							for ( ; 0 != addr->prochead.que.fl; )
							{
								boolean_t	woke_self_or_none;

								mutex_wakeup(addr, &woke_self_or_none);
							}
						}
					}
				}
			}
			/* --------------------- report results ------------------------- */
			if (asyncio_status)
			{
				if (csd->asyncio)
					util_out_print("Database file !AD now has asyncio !AD", TRUE,
						       fn_len, fn, LEN_AND_LIT("enabled"));
				else
					util_out_print("Database file !AD now has asyncio !AD", TRUE,
						       fn_len, fn, LEN_AND_LIT("disabled"));
			}
			if (CLI_NEGATED == read_only_status)
				util_out_print("Database file !AD is no longer read-only",
					TRUE, fn_len, fn);
			else if (CLI_PRESENT == read_only_status)
				util_out_print("Database file !AD is now read-only",
					TRUE, fn_len, fn);
			if (disk_wait_status)
				util_out_print("Database file !AD now has wait disk set to !UL seconds",
					TRUE, fn_len, fn, csd->wait_disk_space);
			if (encryption_complete_status)
				util_out_print("Database file !AD now has encryption marked complete", TRUE, fn_len, fn);
			if (epoch_taper_status)
				util_out_print("Database file !AD now has epoch taper flag set to !AD", TRUE,
					fn_len, fn, 5, (csd->epoch_taper ? " TRUE" : "FALSE"));
			if (extn_count_status)
				util_out_print("Database file !AD now has extension count !UL",
					TRUE, fn_len, fn, csd->extension_size);
			if (CLI_NEGATED == inst_freeze_on_error_status)
				util_out_print("Database file !AD now has inst freeze on fail flag set to FALSE",
					TRUE, fn_len, fn);
			else if (CLI_PRESENT == inst_freeze_on_error_status)
				util_out_print("Database file !AD now has inst freeze on fail flag set to TRUE",
					TRUE, fn_len, fn);
			if (hard_spin_status)
				util_out_print("Database file !AD now has hard spin count !UL",
					TRUE, fn_len, fn, HARD_SPIN_COUNT(csd));
			if (sleep_cnt_status)
				util_out_print("Database file !AD now has sleep spin count !UL",
					TRUE, fn_len, fn, SLEEP_SPIN_CNT(csd));
			if (spin_sleep_status)
				util_out_print("Database file !AD now has spin sleep mask !UL",
					TRUE, fn_len, fn, SPIN_SLEEP_MASK(csd));
			if (trigger_flush_limit_status)
				util_out_print("Database file !AD now has trigger_flush_limit !UL",
					TRUE, fn_len, fn, csd->flush_trigger_top);
			if (wrt_per_flu_status)
				util_out_print("Database file !AD now has writes per flush !UL",
					TRUE, fn_len, fn, csd->n_wrt_per_flu);
                        if (reorg_sleep_nsec_status)
				util_out_print("Database file !AD now has reorg sleep nanoseconds !UL",
                                        TRUE, fn_len, fn, csd->reorg_sleep_nsec);
			if (full_blkwrt_status)
			{
				switch(csd->write_fullblk)
				{
					case 0:
						util_out_print("Database file !AD now has full blk writes: !AD",
							TRUE, fn_len, fn, LEN_AND_LIT("disabled"));
						break;
					case 1:
						util_out_print("Database file !AD now has full blk writes: !AD",
							TRUE, fn_len, fn, LEN_AND_LIT("file system block writes"));
						break;
					case 2:
						util_out_print("Database file !AD now has full blk writes: !AD",
							TRUE, fn_len, fn, LEN_AND_LIT("full DB block writes"));
						break;
				}
			}
			if (problksplit_status)
				util_out_print("Database file !AD now has proactive block split flag set to !UL", TRUE,
					fn_len, fn, csd->problksplit);
			if (rsrvd_bytes_status)
				util_out_print("Database file !AD now has !UL reserved bytes",
						TRUE, fn_len, fn, csd->reserved_bytes);
			if (i_rsrvd_bytes_status)
				util_out_print("Database file !AD now has !UL index reserved bytes",
						TRUE, fn_len, fn, csd->i_reserved_bytes);
			if (d_rsrvd_bytes_status)
				util_out_print("Database file !AD now has !UL data reserved bytes",
						TRUE, fn_len, fn, csd->reserved_bytes);
			if (mutex_type_status)
				util_out_print("Database file !AD now has mutex type set to !AZ",
						TRUE, fn_len, fn,
						((mutex_type_adaptive_ydb == csd->mutex_type)
							? "ADAPTIVE"
							: ((mutex_type_pthread == csd->mutex_type) ? "PTHREAD" : "YDB")));
			if (got_standalone)
			{
				assert(FD_INVALID != fd);
				if (0 == memcmp(pvt_csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
					db_header_dwnconv(pvt_csd);
				DB_LSEEKWRITE(NULL,((unix_db_info *)NULL),NULL,fd,0,pvt_csd,SIZEOF(sgmnt_data),status);
				if (0 != status)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					util_out_print("write : !AZ", TRUE, errptr);
					util_out_print("Error writing header of file", TRUE);
					util_out_print("Database file !AD not changed: ", TRUE, fn_len, fn);
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
				}

				if (defer_status && (dba_mm == pvt_csd->acc_meth))
					util_out_print("Database file !AD now has defer_time set to !SL",
							TRUE, fn_len, fn, pvt_csd->defer_time);
				if (glbl_buff_status)
					util_out_print("Database file !AD now has !UL global buffers",
							TRUE, fn_len, fn, pvt_csd->n_bts);
				if (key_size_status)
					util_out_print("Database file !AD now has maximum key size !UL",
							TRUE, fn_len, fn, pvt_csd->max_key_size);
				if (encryptable_status)
					util_out_print("Database file !AD now has encryptable flag set to !AD", TRUE,
							fn_len, fn, 5,
							(TO_BE_ENCRYPTED(pvt_csd->is_encrypted) ? " TRUE" : "FALSE"));
				if (locksharesdbcrit)
					util_out_print("Database file !AD now has LOCK sharing crit with DB !AD", TRUE,
							fn_len, fn, 5, (pvt_csd->lock_crit_with_db ? " TRUE" : "FALSE"));
				if (lock_space_status)
					util_out_print("Database file !AD now has lock space !UL pages",
							TRUE, fn_len, fn, pvt_csd->lock_space_size/OS_PAGELET_SIZE);
				if (mutex_space_status)
					util_out_print("Database file !AD now has !UL mutex queue slots",
							TRUE, fn_len, fn, NUM_CRIT_ENTRY(pvt_csd));
				if (null_subs_status)
					util_out_print("Database file !AD now has null subscripts set to !AD",
							TRUE, fn_len, fn, strlen("EXISTING"), (pvt_csd->null_subs == ALWAYS) ?
							"ALWAYS  " : (pvt_csd->null_subs == ALLOWEXISTING) ?
							"EXISTING" : "NEVER   ");
				if (qdbrundown_status)
					util_out_print("Database file !AD now has quick database rundown flag set to !AD", TRUE,
							fn_len, fn, 5, (pvt_csd->mumps_can_bypass ? " TRUE" : "FALSE"));
				if (rec_size_status)
					util_out_print("Database file !AD now has maximum record size !UL",
							TRUE, fn_len, fn, pvt_csd->max_rec_size);
				if (stats_status)
					util_out_print("Database file !AD now has sharing of gvstats set to !AD", TRUE,
							fn_len, fn, 5, (CLI_PRESENT == stats_status) ? " TRUE" : "FALSE");
				if (stdb_alloc_status)
					util_out_print("Database file !AD now has !UL statsdb allocation",
							TRUE, fn_len, fn, pvt_csd->statsdb_allocation);
				if (stdnullcoll_status)
					util_out_print("Database file !AD is now using !AD", TRUE, fn_len, fn,
							strlen("M standard null collation"),
							(CLI_PRESENT == stdnullcoll_status) ?
							"M standard null collation" : "GT.M null collation      ");
			} else
			{
				if (0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
					db_header_dwnconv(csd);
				if (flush_buffers)
					wcs_flu(WCSFLU_FLUSH_HDR);
				else
					fileheader_sync(gv_cur_region);
			}
		} else
			exit_stat |= reg_exit_stat;
		if (got_standalone)
		{
			assert(FD_INVALID != fd);
			CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
			db_ipcs_reset(gv_cur_region);
		} else
		{
			rel_crit(gv_cur_region);
			exit_stat |= gds_rundown(CLEANUP_UDI_TRUE);
		}
		mu_gv_cur_reg_free();
	}
	free(pvt_csd);
	assert(!(exit_stat & EXIT_INF));
	return (exit_stat & EXIT_ERR ? (int4)ERR_WCERRNOTCHG :
		(exit_stat & EXIT_WRN ? (int4)ERR_WCWRNNOTCHG : SS_NORMAL));
}
