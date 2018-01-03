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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "mupip_set.h"
#include "mu_rndwn_file.h"
#include "mupip_exit.h"
#include "ipcrmid.h"
#include "mu_gv_cur_reg_init.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "timers.h"
#include "db_ipcs_reset.h"
#include "wcs_flu.h"
#include "gds_rundown.h"
#include "change_reg.h"
#include "desired_db_format_set.h"
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "gtmcrypt.h"
#include "anticipatory_freeze.h"
#include "get_fs_block_size.h"
#include "interlock.h"

GBLREF	bool			in_backup;
GBLREF	bool			region;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	tp_region		*grlist;

LITREF char			*gtm_dbversion_table[];

error_def(ERR_ASYNCIONOV4);
error_def(ERR_ASYNCIONOMM);
error_def(ERR_CRYPTNOMM);
error_def(ERR_DBBLKSIZEALIGN);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_DBRDERR);
error_def(ERR_DBRDONLY);
error_def(ERR_INVACCMETHOD);
error_def(ERR_MMNODYNDWNGRD);
error_def(ERR_MUPIPSET2BIG);
error_def(ERR_MUPIPSET2SML);
error_def(ERR_MUREENCRYPTV4NOALLOW);
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
	boolean_t		bypass_partial_recov, got_standalone, need_standalone = FALSE, acc_meth_changing;
	char			acc_spec[MAX_ACC_METH_LEN + 1], *command = "MUPIP SET VERSION", *errptr, exit_stat, *fn,
				ver_spec[MAX_DB_VER_LEN + 1];
	enum db_acc_method	access, access_new;
	enum db_ver		desired_dbver;
	gd_region		*temp_cur_region;
	int			asyncio_status, defer_allocate_status, defer_status, disk_wait_status, encryptable_status,
				encryption_complete_status, epoch_taper_status, extn_count_status, fd, fn_len, glbl_buff_status,
				hard_spin_status, inst_freeze_on_error_status, key_size_status, locksharesdbcrit,
				lock_space_status, mutex_space_status, null_subs_status, qdbrundown_status, rec_size_status,
				reg_exit_stat, rc, rsrvd_bytes_status, sleep_cnt_status, save_errno, stats_status, status,
				status1, stdnullcoll_status;
	int4			defer_time, new_cache_size, new_disk_wait, new_extn_count, new_hard_spin, new_key_size,
				new_lock_space, new_mutex_space, new_null_subs, new_rec_size, new_sleep_cnt, new_spin_sleep,
				new_stdnullcoll, reserved_bytes, spin_sleep_status, read_only_status;
	sgmnt_data_ptr_t	csd, pvt_csd;
	tp_region		*rptr, single;
	unsigned short		acc_spec_len = MAX_ACC_METH_LEN, ver_spec_len = MAX_DB_VER_LEN;
	gd_segment		*seg;
	uint4			fsb_size, reservedDBFlags;
	ZOS_ONLY(int 		realfiletag;)
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
		}
		need_standalone = TRUE;
	} else
		access = n_dba;		/* really want to keep current method,
					    which has not yet been read */
	if (asyncio_status = cli_present("ASYNCIO"))
		need_standalone = TRUE;
	defer_allocate_status = cli_present("DEFER_ALLOCATE");
	if (encryptable_status = cli_present("ENCRYPTABLE"))
		need_standalone = TRUE;
	if (read_only_status = cli_present("READ_ONLY")) /* Note assignment */
		need_standalone = TRUE;
	encryption_complete_status = cli_present("ENCRYPTIONCOMPLETE");
	epoch_taper_status = cli_present("EPOCHTAPER");
	/* EXTENSION_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if (extn_count_status = cli_present("EXTENSION_COUNT"))
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
	}
	if (glbl_buff_status = cli_present("GLOBAL_BUFFERS"))
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
	if (hard_spin_status = cli_present("HARD_SPIN_COUNT"))
        {	/* No min or max tests needed because mupip_cmd enforces min of 0 and no max requirement is documented*/
                if (!cli_get_int("HARD_SPIN_COUNT", &new_hard_spin))
                {
                        gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("HARD_SPIN_COUNT"));
                        exit_stat |= EXIT_ERR;
                }
        }
	inst_freeze_on_error_status = cli_present("INST_FREEZE_ON_ERROR");
	if (key_size_status = cli_present("KEY_SIZE"))
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
	if (locksharesdbcrit = cli_present("LCK_SHARES_DB_CRIT"))
		need_standalone = TRUE;
	if (lock_space_status = cli_present("LOCK_SPACE"))
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
	if (mutex_space_status = cli_present("MUTEX_SLOTS"))
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
	if (null_subs_status = cli_present("NULL_SUBSCRIPTS"))
	{
		if (-1 == (new_null_subs = cli_n_a_e("NULL_SUBSCRIPTS")))
                        exit_stat |= EXIT_ERR;
		need_standalone = TRUE;
	}
	if (qdbrundown_status = cli_present("QDBRUNDOWN"))
		need_standalone = TRUE;
	if (rec_size_status = cli_present("RECORD_SIZE"))
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
	if (rsrvd_bytes_status = cli_present("RESERVED_BYTES"))
	{
		if (!cli_get_int("RESERVED_BYTES", &reserved_bytes))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETQUALPROB, 2, LEN_AND_LIT("RESERVED_BYTES"));
			exit_stat |= EXIT_ERR;
		}
		need_standalone = TRUE;
	}
	/* SLEEP_SPIN_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if (sleep_cnt_status = cli_present("SLEEP_SPIN_COUNT"))
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
	if (spin_sleep_status = cli_present("SPIN_SLEEP_MASK"))
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
	if (stats_status = cli_present("STATS"))
		need_standalone = TRUE;
	if (stdnullcoll_status = cli_present("STDNULLCOLL"))
		need_standalone = TRUE;
	if (cli_present("VERSION"))
	{
		cli_get_str("VERSION", ver_spec, &ver_spec_len);
		ver_spec[ver_spec_len] = '\0';
		cli_strupper(ver_spec);
		if (0 == memcmp(ver_spec, "V4", ver_spec_len + 1))
			desired_dbver = GDSV4;
		else  if (0 == memcmp(ver_spec, "V6", ver_spec_len + 1))
			desired_dbver = GDSV6;
		else
			assertpro(FALSE);		/* CLI should prevent us ever getting here */
	} else
		desired_dbver = GDSVLAST;	/* really want to keep version, which has not yet been read */
	if (disk_wait_status = cli_present("WAIT_DISK"))
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
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_NOUSERDB, 4, LEN_AND_LIT("MUPIP SET"),
					REG_LEN_STR(rptr->reg));
				exit_stat |= EXIT_WRN;
				continue;
			}
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
				}
				if(dba_mm != access)
				{
					if (pvt_csd->read_only && (CLI_NEGATED != read_only_status))
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
				if (pvt_csd->blks_to_upgrd)
				{
					util_out_print("MM access method cannot be set if there are blocks to upgrade",	TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				}
				if (GDSVCURR != pvt_csd->desired_db_format)
				{
					util_out_print("MM access method cannot be set in DB compatibility mode",
						TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				}
				if (JNL_ENABLED(pvt_csd) && pvt_csd->jnl_before_image)
				{
					util_out_print("MM access method cannot be set with BEFORE image journaling", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				}
				pvt_csd->jnl_before_image = FALSE;
			} else
			{
				if (defer_status)
				{
					util_out_print("DEFER cannot be specified with BG access method.", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
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
				if (!pvt_csd->fully_upgraded)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUREENCRYPTV4NOALLOW, 2, fn_len, fn);
					reg_exit_stat |= EXIT_WRN;
				} else if (dba_mm == access_new)
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
				pvt_csd->n_wrt_per_flu = 7;
				pvt_csd->flush_trigger = FLUSH_FACTOR(pvt_csd->n_bts);
			}
			if (key_size_status)
			{
				key_size_status = pvt_csd->blk_size - SIZEOF(blk_hdr) - SIZEOF(rec_hdr) - SIZEOF(block_id)
					- BSTAR_REC_SIZE - pvt_csd->reserved_bytes;
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
				gv_cur_region->null_subs = pvt_csd->null_subs = (unsigned char)new_null_subs;
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
			if (rsrvd_bytes_status)
			{
				if (reserved_bytes > MAX_RESERVE_B(pvt_csd))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUPIPSET2BIG, 4, reserved_bytes,
						LEN_AND_LIT("RESERVED_BYTES"), MAX_RESERVE_B(pvt_csd));
					reg_exit_stat |= EXIT_WRN;
				}
				pvt_csd->reserved_bytes = reserved_bytes;
			}
			if (stats_status)
			{
				reservedDBFlags = pvt_csd->reservedDBFlags & ~RDBF_NOSTATS;
				if (CLI_NEGATED == stats_status)
					reservedDBFlags |= RDBF_NOSTATS;
				pvt_csd->reservedDBFlags = reservedDBFlags;
			}
			if (read_only_status)
			{
				if (dba_mm != pvt_csd->acc_meth)
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_READONLYNOBG);
					reg_exit_stat |= EXIT_WRN;
				} else
				{
					pvt_csd->read_only = !(CLI_NEGATED == read_only_status);
				}
			}
			if (CLI_NEGATED == stdnullcoll_status)
				gv_cur_region->std_null_coll = pvt_csd->std_null_coll = FALSE;
			else if (CLI_PRESENT == stdnullcoll_status)
				gv_cur_region->std_null_coll = pvt_csd->std_null_coll = TRUE;
			if (EXIT_NRM != reg_exit_stat)
			{
				DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, reg_exit_stat);
				continue;
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
			gvcst_init(gv_cur_region, NULL);
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
			grab_crit(gv_cur_region);
			if (FROZEN_CHILLED(cs_addrs))
			{
				DO_CHILLED_AUTORELEASE(cs_addrs, cs_data);
				if (FROZEN_CHILLED(cs_addrs))
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_OFRZACTIVE, 2,
								DB_LEN_STR(gv_cur_region));
					exit_stat |= EXIT_WRN;
					exit_stat |= gds_rundown(CLEANUP_UDI_TRUE);
					mu_gv_cur_reg_free();
					continue;
				}
			}
		}
		access_new = (n_dba == access ? csd->acc_meth : access);
		if (GDSVLAST != desired_dbver)
		{
			if ((dba_mm != access_new) || (GDSV4 != desired_dbver))
				(void)desired_db_format_set(gv_cur_region, desired_dbver, command);
			else	/* for other errors desired_db_format_set prints appropriate error messages */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MMNODYNDWNGRD, 2, REG_LEN_STR(gv_cur_region));
			if (csd->desired_db_format != desired_dbver)
				reg_exit_stat |= EXIT_WRN;
			else
				util_out_print("Database file !AD now has desired DB format !AD", TRUE,
					fn_len, fn, LEN_AND_STR(gtm_dbversion_table[csd->desired_db_format]));
		}
		if (encryption_complete_status)
		{
			if (!csd->fully_upgraded)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUREENCRYPTV4NOALLOW, 2, fn_len, fn);
				reg_exit_stat |= EXIT_WRN;
			} else if (dba_mm == access_new)
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
			if (!csd->fully_upgraded)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ASYNCIONOV4, 6, DB_LEN_STR(gv_cur_region),
					LEN_AND_LIT("V4 format"), LEN_AND_LIT("enable ASYNCIO"));
				reg_exit_stat |= EXIT_WRN;
			}
			if (!acc_meth_changing && (dba_bg != access_new))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ASYNCIONOMM, 6, DB_LEN_STR(gv_cur_region),
					LEN_AND_LIT(" has MM access method;"), LEN_AND_LIT("enable ASYNCIO"));
				reg_exit_stat |= EXIT_WRN;
			}
			seg = gv_cur_region->dyn.addr;
			/* AIO = ON, implies we need to use O_DIRECT. Check for db vs fs blksize alignment issues. */
			fsb_size = get_fs_block_size(got_standalone ? fd : FILE_INFO(gv_cur_region)->fd);
			if (0 != (csd->blk_size % fsb_size))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBBLKSIZEALIGN, 4,
							DB_LEN_STR(gv_cur_region), csd->blk_size, fsb_size);
				reg_exit_stat |= EXIT_WRN;
			}
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
			if (extn_count_status)
				csd->extension_size = (uint4)new_extn_count;
			change_fhead_timer("FLUSH_TIME", csd->flush_time,
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
			if (got_standalone)
			{
				DB_LSEEKWRITE(NULL, ((unix_db_info *)NULL), NULL, fd, 0, pvt_csd, SIZEOF(sgmnt_data), status);
				if (0 != status)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					util_out_print("write : !AZ", TRUE, errptr);
					util_out_print("Error writing header of file", TRUE);
					util_out_print("Database file !AD not changed: ", TRUE, fn_len, fn);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
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
				if (rsrvd_bytes_status)
					util_out_print("Database file !AD now has !UL reserved bytes",
							TRUE, fn_len, fn, pvt_csd->reserved_bytes);
				if (stats_status)
					util_out_print("Database file !AD now has sharing of gvstats set to !AD", TRUE,
						       fn_len, fn, 5, (CLI_PRESENT == stats_status) ? " TRUE" : "FALSE");
				if (stdnullcoll_status)
					util_out_print("Database file !AD is now using !AD", TRUE, fn_len, fn,
							strlen("M standard null collation"),
							(CLI_PRESENT == stdnullcoll_status) ?
							"M standard null collation" : "GT.M null collation      ");
			} else
				wcs_flu(WCSFLU_FLUSH_HDR);
		} else
			exit_stat |= reg_exit_stat;
		if (got_standalone)
		{
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
