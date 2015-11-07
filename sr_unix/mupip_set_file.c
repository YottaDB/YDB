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

GBLREF	tp_region		*grlist;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	bool			region;
GBLREF	bool			in_backup;

LITREF char			*gtm_dbversion_table[];

error_def(ERR_BADTAG);
error_def(ERR_CRYPTNOMM);
error_def(ERR_DBPREMATEOF);
error_def(ERR_DBRDERR);
error_def(ERR_DBRDONLY);
error_def(ERR_INVACCMETHOD);
error_def(ERR_MMNODYNDWNGRD);
error_def(ERR_MUNOACTION);
error_def(ERR_RBWRNNOTCHG);
error_def(ERR_TEXT);
error_def(ERR_WCERRNOTCHG);
error_def(ERR_WCWRNNOTCHG);

#define MAX_ACC_METH_LEN	2
#define MAX_DB_VER_LEN		2
#define MIN_MAX_KEY_SZ		3

#define DO_CLNUP_AND_SET_EXIT_STAT(EXIT_STAT, EXIT_WRN_ERR_MASK)		\
{										\
	exit_stat |= EXIT_WRN_ERR_MASK;						\
	db_ipcs_reset(gv_cur_region);						\
	mu_gv_cur_reg_free();							\
}

#define CLOSE_AND_RETURN							\
{										\
	CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */		\
	db_ipcs_reset(gv_cur_region);						\
	return (int4)ERR_WCWRNNOTCHG;						\
}

int4 mupip_set_file(int db_fn_len, char *db_fn)
{
	bool			got_standalone;
	boolean_t		bypass_partial_recov, need_standalone = FALSE;
	char			acc_spec[MAX_ACC_METH_LEN], ver_spec[MAX_DB_VER_LEN], exit_stat, *fn;
	unsigned short		acc_spec_len = MAX_ACC_METH_LEN, ver_spec_len = MAX_DB_VER_LEN;
	int			fd, fn_len;
	int4			status;
	int4			status1;
	int			glbl_buff_status, defer_status, rsrvd_bytes_status,
				extn_count_status, lock_space_status, disk_wait_status,
				inst_freeze_on_error_status, qdbrundown_status, mutex_space_status;
	int4			new_disk_wait, new_cache_size, new_extn_count, new_lock_space, reserved_bytes, defer_time,
				new_mutex_space;
	int			key_size_status, rec_size_status;
	int4			new_key_size, new_rec_size;
	sgmnt_data_ptr_t	csd;
	tp_region		*rptr, single;
	enum db_acc_method	access, access_new;
	enum db_ver		desired_dbver;
	gd_region		*temp_cur_region;
	char			*errptr, *command = "MUPIP SET VERSION";
	int			save_errno;
	int			rc;

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
	if (disk_wait_status = cli_present("WAIT_DISK"))
	{
		if (cli_get_int("WAIT_DISK", &new_disk_wait))
		{
			if (new_disk_wait < 0)
			{
				util_out_print("!UL negative, minimum WAIT_DISK allowed is 0.", TRUE, new_disk_wait);
				return (int4)ERR_WCWRNNOTCHG;
			}
			need_standalone = TRUE;
		} else
		{
			util_out_print("Error getting WAIT_DISK qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
	}
	if (glbl_buff_status = cli_present("GLOBAL_BUFFERS"))
	{
		if (cli_get_int("GLOBAL_BUFFERS", &new_cache_size))
		{
			if (new_cache_size > GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS))
			{
				util_out_print("!UL too large, maximum write cache buffers allowed is !UL", TRUE, new_cache_size,
						GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS));
				return (int4)ERR_WCWRNNOTCHG;
			}
			if (new_cache_size < WC_MIN_BUFFS)
			{
				util_out_print("!UL too small, minimum cache buffers allowed is !UL", TRUE, new_cache_size,
						WC_MIN_BUFFS);
				return (int4)ERR_WCWRNNOTCHG;
			}
		} else
		{
			util_out_print("Error getting GLOBAL BUFFER qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	/* EXTENSION_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if (extn_count_status = cli_present("EXTENSION_COUNT"))
	{
		if (cli_get_int("EXTENSION_COUNT", &new_extn_count))
		{
			if (new_extn_count > MAX_EXTN_COUNT)
			{
				util_out_print("!UL too large, maximum extension count allowed is !UL", TRUE, new_extn_count,
						MAX_EXTN_COUNT);
				return (int4)ERR_WCWRNNOTCHG;
			}
			if (new_extn_count < MIN_EXTN_COUNT)
			{
				util_out_print("!UL too small, minimum extension count allowed is !UL", TRUE, new_extn_count,
						MIN_EXTN_COUNT);
				return (int4)ERR_WCWRNNOTCHG;
			}
		} else
		{
			util_out_print("Error getting EXTENSION COUNT qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
	}
	if (lock_space_status = cli_present("LOCK_SPACE"))
	{
		if (cli_get_int("LOCK_SPACE", &new_lock_space))
		{
			if (new_lock_space > MAX_LOCK_SPACE)
			{
				util_out_print("!UL too large, maximum lock space allowed is !UL", TRUE,
						new_lock_space, MAX_LOCK_SPACE);
				return (int4)ERR_WCWRNNOTCHG;
			}
			else if (new_lock_space < MIN_LOCK_SPACE)
			{
				util_out_print("!UL too small, minimum lock space allowed is !UL", TRUE,
						new_lock_space, MIN_LOCK_SPACE);
				return (int4)ERR_WCWRNNOTCHG;
			}
		} else
		{
			util_out_print("Error getting LOCK_SPACE qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	if (mutex_space_status = cli_present("MUTEX_SLOTS"))
	{
		if (cli_get_int("MUTEX_SLOTS", &new_mutex_space))
		{
			if (new_mutex_space > MAX_CRIT_ENTRY)
			{
				util_out_print("!UL too large, maximum number of mutex slots allowed is !UL", TRUE,
						new_mutex_space, MAX_CRIT_ENTRY);
				return (int4)ERR_WCWRNNOTCHG;
			} else if (new_mutex_space < MIN_CRIT_ENTRY)
			{
				util_out_print("!UL too small, minimum number of mutex slots allowed is !UL", TRUE,
						new_mutex_space, MIN_CRIT_ENTRY);
				return (int4)ERR_WCWRNNOTCHG;
			}
		} else
		{
			util_out_print("Error getting MUTEX_SPACE qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	if (rsrvd_bytes_status = cli_present("RESERVED_BYTES"))
	{
		if (!cli_get_int("RESERVED_BYTES", &reserved_bytes))
		{
			util_out_print("Error getting RESERVED BYTES qualifier value", TRUE);
			return (int4)ERR_RBWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	if (cli_present("ACCESS_METHOD"))
	{
		cli_get_str("ACCESS_METHOD", acc_spec, &acc_spec_len);
		cli_strupper(acc_spec);
		if (0 == memcmp(acc_spec, "MM", acc_spec_len))
			access = dba_mm;
		else  if (0 == memcmp(acc_spec, "BG", acc_spec_len))
			access = dba_bg;
		else
			mupip_exit(ERR_INVACCMETHOD);
		need_standalone = TRUE;
	} else
		access = n_dba;		/* really want to keep current method,
					    which has not yet been read */
	if (key_size_status = cli_present("KEY_SIZE"))
	{
		if (!cli_get_int("KEY_SIZE", &new_key_size))
		{
			util_out_print("Error getting KEY_SIZE qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	if (rec_size_status = cli_present("RECORD_SIZE"))
	{
		if (!cli_get_int("RECORD_SIZE", &new_rec_size))
		{
			util_out_print("Error getting RECORD_SIZE qualifier value", TRUE);
			return (int4)ERR_WCWRNNOTCHG;
		}
		need_standalone = TRUE;
	}
	if (cli_present("VERSION"))
	{
		assert(!need_standalone);
		cli_get_str("VERSION", ver_spec, &ver_spec_len);
		cli_strupper(ver_spec);
		if (0 == memcmp(ver_spec, "V4", ver_spec_len))
			desired_dbver = GDSV4;
		else  if (0 == memcmp(ver_spec, "V6", ver_spec_len))
			desired_dbver = GDSV6;
		else
			GTMASSERT;		/* CLI should prevent us ever getting here */
	} else
		desired_dbver = GDSVLAST;	/* really want to keep version, which has not yet been read */
	inst_freeze_on_error_status = cli_present("INST_FREEZE_ON_ERROR");
	qdbrundown_status = cli_present("QDBRUNDOWN");
	if (region)
		rptr = grlist;
	else
	{
		rptr = &single;
		memset(&single, 0, SIZEOF(single));
	}

	csd = (sgmnt_data *)malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	in_backup = FALSE;		/* Only want yes/no from mupfndfil, not an address */
	for (;  rptr != NULL;  rptr = rptr->fPtr)
	{
		if (region)
		{
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				util_out_print("!/Region !AD is not a GDS access type", TRUE, REG_LEN_STR(rptr->reg));
				exit_stat |= EXIT_WRN;
				continue;
			}
			if (!mupfndfil(rptr->reg, NULL))
				continue;
			fn = (char *)rptr->reg->dyn.addr->fname;
			fn_len = rptr->reg->dyn.addr->fname_len;
		} else
		{
			fn = db_fn;
			fn_len = db_fn_len;
		}
		mu_gv_cur_reg_init();
		strcpy((char *)gv_cur_region->dyn.addr->fname, fn);
		gv_cur_region->dyn.addr->fname_len = fn_len;
		if (!need_standalone)
		{
			gvcst_init(gv_cur_region);
			change_reg();	/* sets cs_addrs and cs_data */
			if (gv_cur_region->read_only)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
				exit_stat |= EXIT_ERR;
				gds_rundown();
				mu_gv_cur_reg_free();
				continue;
			}
			assert(!cs_addrs->hold_onto_crit); /* this ensures we can safely do unconditional grab_crit and rel_crit */
			grab_crit(gv_cur_region);
			status = EXIT_NRM;
			access_new = (n_dba == access ? cs_data->acc_meth : access);
							/* recalculate; n_dba is a proxy for no change */
			change_fhead_timer("FLUSH_TIME", cs_data->flush_time,
					   (dba_bg == access_new ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM),
					   FALSE);
			if (GDSVLAST != desired_dbver)
			{
				if ((dba_mm != access_new) || (GDSV4 != desired_dbver))
					status1 = desired_db_format_set(gv_cur_region, desired_dbver, command);
				else
				{
					status1 = ERR_MMNODYNDWNGRD;
					gtm_putmsg(VARLSTCNT(4) status1, 2, REG_LEN_STR(gv_cur_region));
				}
				if (SS_NORMAL != status1)
				{	/* "desired_db_format_set" would have printed appropriate error messages */
					if (ERR_MUNOACTION != status1)
					{	/* real error occurred while setting the db format. skip to next region */
						status = EXIT_ERR;
					}
				}
			}
			if (EXIT_NRM == status)
			{
				if (extn_count_status)
					cs_data->extension_size = (uint4)new_extn_count;
				wcs_flu(WCSFLU_FLUSH_HDR);
				if (extn_count_status)
					util_out_print("Database file !AD now has extension count !UL",
						TRUE, fn_len, fn, cs_data->extension_size);
				if (GDSVLAST != desired_dbver)
					util_out_print("Database file !AD now has desired DB format !AD", TRUE,
						fn_len, fn, LEN_AND_STR(gtm_dbversion_table[cs_data->desired_db_format]));
				if (CLI_NEGATED == inst_freeze_on_error_status)
				{
					cs_data->freeze_on_fail = FALSE;
					util_out_print("Database file !AD now has inst freeze on fail flag set to FALSE",
							TRUE, fn_len, fn);
				}
				else if (CLI_PRESENT == inst_freeze_on_error_status)
				{
					cs_data->freeze_on_fail = TRUE;
					util_out_print("Database file !AD now has inst freeze on fail flag set to TRUE",
							TRUE, fn_len, fn);
				}
				if (qdbrundown_status)
				{
					cs_data->mumps_can_bypass = CLI_PRESENT == qdbrundown_status;
					util_out_print("Database file !AD now has quick database rundown flag set to !AD", TRUE,
						       fn_len, fn, 5, (cs_data->mumps_can_bypass ? " TRUE" : "FALSE"));
				}
			} else
				exit_stat |= status;
			rel_crit(gv_cur_region);
			UNIX_ONLY(exit_stat |=)gds_rundown();
		} else
		{	/* Following part needs standalone access */
			assert(GDSVLAST == desired_dbver);
			got_standalone = STANDALONE(gv_cur_region);
			if (FALSE == got_standalone)
				return (int4)ERR_WCERRNOTCHG;
			/* we should open it (for changing) after mu_rndwn_file, since mu_rndwn_file changes the file header too */
			if (FD_INVALID == (fd = OPEN(fn, O_RDWR)))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("open : !AZ", TRUE, errptr);
				DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_ERR);
				continue;
			}
#ifdef __MVS__
			if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
				TAG_POLICY_GTM_PUTMSG(fn, realfiletag, TAG_BINARY, errno);
#endif
			LSEEKREAD(fd, 0, csd, SIZEOF(sgmnt_data), status);
			if (0 != status)
			{
				save_errno = errno;
				PERROR("Error reading header of file");
				errptr = (char *)STRERROR(save_errno);
				util_out_print("read : !AZ", TRUE, errptr);
				util_out_print("Error reading header of file", TRUE);
				util_out_print("Database file !AD not changed:  ", TRUE, fn_len, fn);
				if (-1 != status)
					rts_error(VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
				else
					rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
			}
			if (rsrvd_bytes_status)
			{
				if (reserved_bytes > MAX_RESERVE_B(csd))
				{
					util_out_print("!UL too large, maximum reserved bytes allowed is !UL for database file !AD",
							TRUE, reserved_bytes, MAX_RESERVE_B(csd), fn_len, fn);
					CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
					db_ipcs_reset(gv_cur_region);
					return (int4)ERR_RBWRNNOTCHG;
				}
				csd->reserved_bytes = reserved_bytes;
			}
			if (key_size_status)
			{
				if (MAX_KEY_SZ < new_key_size)
				{	/* bigger than 1023 not supported */
					util_out_print("!UL too large, highest maximum key size allowed is !UL", TRUE,
							new_key_size, MAX_KEY_SZ);
					CLOSE_AND_RETURN;
				} else if (MIN_MAX_KEY_SZ > new_key_size)
				{	/* less than 3 not supported */
					util_out_print("!UL too small, lowest maximum key size allowed is !UL", TRUE,
							new_key_size, MIN_MAX_KEY_SZ);
					CLOSE_AND_RETURN;
				} else if (csd->blk_size < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + new_key_size + SIZEOF(block_id)
								 + BSTAR_REC_SIZE + csd->reserved_bytes))
				{	/* too big for block */
					util_out_print("!UL too large, lowest maximum key size allowed (given block size) is !UL",
							TRUE, new_key_size, csd->blk_size - SIZEOF(blk_hdr) - SIZEOF(rec_hdr)
							 - SIZEOF(block_id) - csd->reserved_bytes + BSTAR_REC_SIZE);
					CLOSE_AND_RETURN;
				} else if (csd->max_key_size > new_key_size)
				{	/* lowering the maximum key size can cause problems if large keys already exist in db */
					util_out_print("!UL smaller than current maximum key size !UL", TRUE,
							new_key_size, csd->max_key_size);
					CLOSE_AND_RETURN;
				}
				csd->max_key_size = new_key_size;
			}
			if (rec_size_status)
			{
				if (MAX_STRLEN < new_rec_size)
				{
					util_out_print("!UL too large, highest maximum record size allowed is !UL", TRUE,
							new_rec_size, MAX_STRLEN);
					CLOSE_AND_RETURN;
				} else if (0 > new_key_size)
				{
					util_out_print("!UL too small, lowest maximum record size allowed is !UL", TRUE,
							new_rec_size, 0);
					CLOSE_AND_RETURN;
				} else if (csd->max_rec_size > new_rec_size)
				{
					util_out_print("!UL smaller than current maximum record size !UL", TRUE,
							new_rec_size, csd->max_rec_size);
					CLOSE_AND_RETURN;
				}
				csd->max_rec_size = new_rec_size;
			}
			access_new = (n_dba == access ? csd->acc_meth : access);
							/* recalculate; n_dba is a proxy for no change */
			change_fhead_timer("FLUSH_TIME", csd->flush_time,
					   (dba_bg == access_new ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM),
					   FALSE);
			if ((n_dba != access) && (csd->acc_meth != access))	/* n_dba is a proxy for no change */
			{
#				ifdef GTM_CRYPT
				if (dba_mm == access && (csd->is_encrypted))
				{
					gtm_putmsg(VARLSTCNT(4) ERR_CRYPTNOMM, 2, DB_LEN_STR(gv_cur_region));
					mupip_exit(ERR_RBWRNNOTCHG);
				}
#				endif
				if (dba_mm == access)
					csd->defer_time = 1;			/* defer defaults to 1 */
				csd->acc_meth = access;
				if (0 == csd->n_bts)
				{
					csd->n_bts = WC_DEF_BUFFS;
					csd->bt_buckets = getprime(csd->n_bts);
				}
			}
			if (glbl_buff_status)
			{
				csd->n_bts = BT_FACTOR(new_cache_size);
				csd->bt_buckets = getprime(csd->n_bts);
				csd->n_wrt_per_flu = 7;
				csd->flush_trigger = FLUSH_FACTOR(csd->n_bts);
			}
			if (disk_wait_status)
				csd->wait_disk_space = new_disk_wait;
			if (extn_count_status)
				csd->extension_size = (uint4)new_extn_count;
			if (lock_space_status)
				csd->lock_space_size = (uint4)new_lock_space * OS_PAGELET_SIZE;
			if (mutex_space_status)
				NUM_CRIT_ENTRY(csd) = new_mutex_space;
			if (qdbrundown_status)
				csd->mumps_can_bypass = CLI_PRESENT == qdbrundown_status;
			if (bypass_partial_recov)
			{
				csd->file_corrupt = FALSE;
				util_out_print("Database file !AD now has partial recovery flag set to  !UL(FALSE) ",
						TRUE, fn_len, fn, csd->file_corrupt);
			}
			if (dba_mm == access_new)
			{
				if (CLI_NEGATED == defer_status)
					csd->defer_time = 0;
				else  if (CLI_PRESENT == defer_status)
				{
					if (!cli_get_num("DEFER_TIME", &defer_time))
					{
						util_out_print("Error getting DEFER_TIME qualifier value", TRUE);
						db_ipcs_reset(gv_cur_region);
						return (int4)ERR_RBWRNNOTCHG;
					}
					if (-1 > defer_time)
					{
						util_out_print("DEFER_TIME cannot take negative values less than -1", TRUE);
						util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
						DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_WRN);
						continue;
					}
					csd->defer_time = defer_time;
				}
				if (csd->blks_to_upgrd)
				{
					util_out_print("MM access method cannot be set if there are blocks to upgrade",	TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_WRN);
					continue;
				}
				if (GDSVCURR != csd->desired_db_format)
				{
					util_out_print("MM access method cannot be set in DB compatibility mode",
						TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_WRN);
					continue;
				}
				if (JNL_ENABLED(csd) && csd->jnl_before_image)
				{
					util_out_print("MM access method cannot be set with BEFORE image journaling", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_WRN);
					continue;
				}
				csd->jnl_before_image = FALSE;
			} else
			{
				if (defer_status)
				{
					util_out_print("DEFER cannot be specified with BG access method.", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					DO_CLNUP_AND_SET_EXIT_STAT(exit_stat, EXIT_WRN);
					continue;
				}
			}
			if (CLI_NEGATED == inst_freeze_on_error_status)
				csd->freeze_on_fail = FALSE;
			else if (CLI_PRESENT == inst_freeze_on_error_status)
				csd->freeze_on_fail = TRUE;
			DB_LSEEKWRITE(NULL, NULL, fd, 0, csd, SIZEOF(sgmnt_data), status);
			if (0 != status)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				util_out_print("Error writing header of file", TRUE);
				util_out_print("Database file !AD not changed: ", TRUE, fn_len, fn);
				rts_error(VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
			}
			CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
			/* --------------------- report results ------------------------- */
			if (glbl_buff_status)
				util_out_print("Database file !AD now has !UL global buffers",
						TRUE, fn_len, fn, csd->n_bts);
			if (defer_status && (dba_mm == csd->acc_meth))
				util_out_print("Database file !AD now has defer_time set to !SL",
						TRUE, fn_len, fn, csd->defer_time);
			if (rsrvd_bytes_status)
				util_out_print("Database file !AD now has !UL reserved bytes",
						TRUE, fn_len, fn, csd->reserved_bytes);
			if (key_size_status)
				util_out_print("Database file !AD now has maximum key size !UL",
						TRUE, fn_len, fn, csd->max_key_size);
			if (rec_size_status)
				util_out_print("Database file !AD now has maximum record size !UL",
						TRUE, fn_len, fn, csd->max_rec_size);
			if (extn_count_status)
				util_out_print("Database file !AD now has extension count !UL",
						TRUE, fn_len, fn, csd->extension_size);
			if (lock_space_status)
				util_out_print("Database file !AD now has lock space !UL pages",
						TRUE, fn_len, fn, csd->lock_space_size/OS_PAGELET_SIZE);
			if (mutex_space_status)
				util_out_print("Database file !AD now has !UL mutex queue slots",
						TRUE, fn_len, fn, NUM_CRIT_ENTRY(csd));
			if (qdbrundown_status)
				util_out_print("Database file !AD now has quick database rundown flag set to !AD", TRUE,
					       fn_len, fn, 5, (csd->mumps_can_bypass ? " TRUE" : "FALSE"));
			if (disk_wait_status)
				util_out_print("Database file !AD now has wait disk set to !UL seconds",
						TRUE, fn_len, fn, csd->wait_disk_space);
			if (CLI_NEGATED == inst_freeze_on_error_status)
				util_out_print("Database file !AD now has inst freeze on fail flag set to FALSE",
						TRUE, fn_len, fn);
			else if (CLI_PRESENT == inst_freeze_on_error_status)
				util_out_print("Database file !AD now has inst freeze on fail flag set to TRUE",
						TRUE, fn_len, fn);
			db_ipcs_reset(gv_cur_region);
		} /* end of else part if (!need_standalone) */
		mu_gv_cur_reg_free();
	}
	free(csd);
	assert(!(exit_stat & EXIT_INF));
	return (exit_stat & EXIT_ERR ? (int4)ERR_WCERRNOTCHG :
		(exit_stat & EXIT_WRN ? (int4)ERR_WCWRNNOTCHG : SS_NORMAL));
}
