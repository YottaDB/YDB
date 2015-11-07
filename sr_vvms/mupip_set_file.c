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

#include <climsgdef.h>
#include <descrip.h>
#include <fab.h>
#include <iodef.h>
#include <lckdef.h>
#include <psldef.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <syidef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "efn.h"
#include "gdsblk.h"
#include "iosp.h"
#include "mupipbckup.h"
#include "vmsdtype.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "timers.h"
#include "gt_timer.h"
#include "util.h"
#include "mupip_set.h"
#include "locks.h"
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#include "dbcx_ref.h"
#include "mu_gv_cur_reg_init.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "gds_rundown.h"
#include "change_reg.h"
#include "desired_db_format_set.h"

GBLREF tp_region	*grlist;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
LITREF char		*gtm_dbversion_table[];

error_def(ERR_DBFILERR);
error_def(ERR_DBOPNERR);
error_def(ERR_DBRDERR);
error_def(ERR_DBRDONLY);
error_def(ERR_MUNOACTION);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUSTANDALONE);
error_def(ERR_WCERRNOTCHG);
error_def(ERR_WCWRNNOTCHG);

#define	CHANGE_FLUSH_TIME_IF_NEEDED(csd)									\
{														\
	if (flush_time_specified)										\
	{	/* Do not invoke change_fhead_timer("FLUSH_TIME"...) more than once in this function		\
		 * as that uses a function CLI$GET_VALUE which returns CLI$_ABSENT if called with the		\
		 * same qualifier more than once. To work around this, invoke "change_fhead_timer" once		\
		 * (for the first region in this loop) and store the "flush_time" that it calculated		\
		 * into a temporary variable that is used for the other regions. The only thing that might	\
		 * affect this is if a mix of regions with BG and MM access methods is specified in this	\
		 * region list. That might present a problem since BG and MM have different default times	\
		 * (TIM_FLU_MOD_BG and TIM_FLU_MOD_MM). But default time is used only if "NOFLUSH_TIME"		\
		 * is specified which is not possible since FLUSH_TIME is NON-NEGATABLE.			\
		 */												\
		assert(SIZEOF(save_flush_time) == SIZEOF(csd->flush_time));					\
		if (!flush_time_processed)									\
		{												\
			change_fhead_timer("FLUSH_TIME", csd->flush_time,					\
					   (dba_bg == (n_dba == access ? csd->acc_meth : access)		\
					    ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM), FALSE);				\
			flush_time_processed = TRUE;								\
			save_flush_time[0] = csd->flush_time[0];						\
			save_flush_time[1] = csd->flush_time[1];						\
		} else												\
		{												\
			csd->flush_time[0] = save_flush_time[0];						\
			csd->flush_time[1] = save_flush_time[1];						\
		}												\
	}													\
}

int4 mupip_set_file(int db_fn_len, char *db_fn)
{
	boolean_t		bypass_partial_recov, need_standalone = FALSE, flush_time_specified, flush_time_processed;
	char			exit_status, *command = "MUPIP SET VERSION";
	enum db_acc_method	access;
	enum db_ver		desired_dbver;
	file_control		*fc;
	int			defer_status, new_extn_count, new_lock_space, new_wait_disk, new_wc_size,
				reserved_bytes, size, temp_new_wc_size, wait_disk_status, new_mutex_space;
	sgmnt_addrs		*csa;
	sgmnt_data		*sd, *sd1;
	short			new_defer_time;
	tp_region		*rptr, single;
	uint4			space_available, space_needed, status, save_flush_time[2];
	int4			status1;
	vms_gds_info		*gds_info;

	$DESCRIPTOR(mm_qualifier,"MM");
	$DESCRIPTOR(bg_qualifier,"BG");
	$DESCRIPTOR(access_qualifier, "ACCESS_METHOD");
	$DESCRIPTOR(dbver_v4, "V4");
	$DESCRIPTOR(dbver_v6, "V6");
	$DESCRIPTOR(dbver_qualifier, "VERSION");

	exit_status = EXIT_NRM;
	bypass_partial_recov = cli_present("PARTIAL_RECOV_BYPASS") == CLI_PRESENT;
	if (bypass_partial_recov)
		need_standalone = TRUE;
	if (CLI_PRESENT == (wait_disk_status = cli_present("WAIT_DISK")))
	{
		if (!cli_get_int("WAIT_DISK", &new_wait_disk))
		{
			util_out_print("Error getting WAIT_DISK qualifier value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		need_standalone = TRUE;
	}
	flush_time_specified = (CLI_PRESENT == cli_present("FLUSH_TIME")) ? TRUE : FALSE;
	flush_time_processed = FALSE;
	if (CLI_PRESENT == (defer_status = cli_present("DEFER_TIME")))
	{
		if (!cli_get_num("DEFER_TIME", &new_defer_time))
		{
			util_out_print("Error getting DEFER_TIME qualifier value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (-1 > new_defer_time)
		{
			util_out_print("DEFER_TIME cannot take negative values other than -1", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		need_standalone = TRUE;
	} else
		defer_status = 0;
	if (cli_get_int("GLOBAL_BUFFERS", &new_wc_size))
	{
		if (new_wc_size > WC_MAX_BUFFS)
		{
			util_out_print("!UL too large, maximum cache buffers allowed is !UL",TRUE,new_wc_size,WC_MAX_BUFFS);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (new_wc_size < WC_MIN_BUFFS)
		{
			util_out_print("!UL too small, minimum cache buffers allowed is !UL",TRUE,new_wc_size,WC_MIN_BUFFS);
			mupip_exit(ERR_MUPCLIERR);
		}
		need_standalone = TRUE;
	} else
		new_wc_size = 0;
	/* EXTENSION_COUNT does not require standalone access and hence need_standalone will not be set to TRUE for this. */
	if (cli_get_int("EXTENSION_COUNT", &new_extn_count))
	{
		if (new_extn_count > MAX_EXTN_COUNT)
		{
			util_out_print("!UL too large, maximum extension count allowed is !UL",TRUE,new_extn_count,MAX_EXTN_COUNT);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (new_extn_count < MIN_EXTN_COUNT)
		{
			util_out_print("!UL too small, minimum extension count allowed is !UL",TRUE,new_extn_count,MIN_EXTN_COUNT);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		new_extn_count = 0;
	if (cli_get_int("LOCK_SPACE", &new_lock_space))
	{
		if (new_lock_space > MAX_LOCK_SPACE)
		{
			util_out_print("!UL too large, maximum lock space allowed is !UL",TRUE,new_lock_space, MAX_LOCK_SPACE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (new_lock_space < MIN_LOCK_SPACE)
		{
			util_out_print("!UL too small, minimum lock space allowed is !UL",TRUE,new_lock_space, MIN_LOCK_SPACE);
			mupip_exit(ERR_MUPCLIERR);
		}
		need_standalone = TRUE;
	} else
		new_lock_space = 0;
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
		need_standalone = TRUE;
	} else
		new_mutex_space = 0;
	if (0 == cli_get_num("RESERVED_BYTES" ,&reserved_bytes))
		reserved_bytes = -1;
	else
		need_standalone = TRUE;
	if (CLI$_ABSENT != cli$present(&access_qualifier))
	{
		if (CLI$_PRESENT == cli$present(&mm_qualifier))
			access = dba_mm;
		else  if (CLI$_PRESENT == cli$present(&bg_qualifier))
			access = dba_bg;
		else
			/* ??? */
			mupip_exit(ERR_MUPCLIERR);
		need_standalone = TRUE;
	} else
		access = n_dba;		/* really want to keep current method, which has not yet been read */
	if (CLI$_ABSENT != cli$present(&dbver_qualifier))
	{
		assert(!need_standalone);
		if (CLI$_PRESENT == cli$present(&dbver_v4))
			desired_dbver = GDSV4;
		else  if (CLI$_PRESENT == cli$present(&dbver_v6))
			desired_dbver = GDSV6;
		else
			GTMASSERT;	/* CLI should prevent us ever getting here */
	} else
		desired_dbver = GDSVLAST;	/* really want to keep current format, which has not yet been read */
	if (region)
		rptr = grlist;
	else
	{
		rptr = &single;
		memset(&single, 0, SIZEOF(single));
		mu_gv_cur_reg_init();
	}
	sd = malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	for (;rptr;  rptr = rptr->fPtr)
	{
		if (region)
		{
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				util_out_print("!/Region !AD is not a GDS access type",TRUE, REG_LEN_STR(rptr->reg));
				exit_status |= EXIT_WRN;
				continue;
			}
			if (!mupfndfil(rptr->reg, NULL))
			{
				exit_status |= EXIT_ERR;
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
				gds_info =
				gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(SIZEOF(GDS_INFO));
				memset(gds_info, 0, SIZEOF(GDS_INFO));
			}
		} else
		{
			gv_cur_region->dyn.addr->fname_len = db_fn_len;
			memcpy(gv_cur_region->dyn.addr->fname, db_fn, db_fn_len);
		}
		if (!need_standalone)
		{
			gvcst_init(gv_cur_region);
			change_reg();
			if (gv_cur_region->read_only)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
				exit_status |= EXIT_ERR;
				gds_rundown();
				continue;
			}
			grab_crit(gv_cur_region);
			status = EXIT_NRM;
			CHANGE_FLUSH_TIME_IF_NEEDED(cs_data);
			if (GDSVLAST != desired_dbver)
			{
				status1 = desired_db_format_set(gv_cur_region, desired_dbver, command);
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
				if (new_extn_count)
					cs_data->extension_size = new_extn_count;
				wcs_flu(WCSFLU_FLUSH_HDR);
				if (new_extn_count)
					util_out_print("Database file !AD now has extension count !UL",
							TRUE, db_fn_len, db_fn, cs_data->extension_size);
				if (GDSVLAST != desired_dbver)
					util_out_print("Database file !AD now has desired DB format !AD", TRUE,
						db_fn_len, db_fn, LEN_AND_STR(gtm_dbversion_table[cs_data->desired_db_format]));
			} else
				exit_status |= status;
			rel_crit(gv_cur_region);
			gds_rundown();
		} else
		{	/* Following part needs standalone access */
			assert(GDSVLAST == desired_dbver);
			if (!mu_rndwn_file(TRUE))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
				exit_status |= EXIT_ERR;
				continue;
			}
			gds_info = FILE_INFO(gv_cur_region);
			fc = gv_cur_region->dyn.addr->file_cntl;
			fc->op = FC_OPEN;
			fc->file_type = dba_bg;
			status = dbfilop(fc);
			if (SS$_NORMAL != status)
			{
				gtm_putmsg(VARLSTCNT(6) ERR_DBOPNERR, 2,
					DB_LEN_STR(gv_cur_region), status, gds_info->fab->fab$l_stv);
				exit_status |= EXIT_ERR;
				continue;
			}
			if (gv_cur_region->read_only)
			{
				gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), RMS$_PRV);
				exit_status |= EXIT_ERR;
				sys$dassgn(gds_info->fab->fab$l_stv);
				continue;
			}
			fc->op = FC_READ;
			fc->op_buff = sd;
			fc->op_len = SGMNT_HDR_LEN;
			fc->op_pos = 1;
			status = dbfilop(fc);
			if (SS_NORMAL != status)
			{
				gtm_putmsg(VARLSTCNT(6) ERR_DBRDERR, 2,
					DB_LEN_STR(gv_cur_region), status, gds_info->fab->fab$l_stv);
				exit_status |= EXIT_ERR;
				sys$dassgn(gds_info->fab->fab$l_stv);
				continue;
			}
			if (-1 != reserved_bytes)
			{
				if (reserved_bytes < 0 || (reserved_bytes > MAX_RESERVE_B(sd)))
				{
					util_out_print("!UL too large, maximum reserved bytes allowed is !UL for database file !AD",
							TRUE, reserved_bytes, MAX_RESERVE_B(sd), gv_cur_region->dyn.addr->fname_len,
							gv_cur_region->dyn.addr->fname);
					exit_status |= EXIT_WRN;
				} else
					sd->reserved_bytes = reserved_bytes;
			}
			CHANGE_FLUSH_TIME_IF_NEEDED(sd);
			if (new_extn_count)
				sd->extension_size = new_extn_count;
			if (CLI_PRESENT == wait_disk_status)
				sd->wait_disk_space = new_wait_disk;
			if (new_lock_space)
				sd->lock_space_size = new_lock_space * OS_PAGELET_SIZE;
			if (new_mutex_space)
				NUM_CRIT_ENTRY(sd) = new_mutex_space;
			if (bypass_partial_recov)
				sd->file_corrupt = FALSE;
			if (dba_mm == (n_dba == access ? sd->acc_meth : access))
				/* always recalculate; n_dba is a proxy for no change */
			{
				if (CLI_NEGATED == defer_status)
					sd->defer_time = 1;
					/* default defer_time = 1 => defer time is 1*flush_time[0] */
				else  if (CLI_PRESENT == defer_status)
				{
					sd->defer_time = new_defer_time;
				}

				if (dba_bg == sd->acc_meth)
				{
					if (FALSE == sd->unbacked_cache)
						sd->free_space += (sd->n_bts + sd->bt_buckets + 1) * SIZEOF(bt_rec);
#ifdef GT_CX_DEF
					sd->free_space += sd->lock_space_size;
#endif
				}
				sd->n_bts = sd->bt_buckets = 0;
				if (n_dba != access)		/* n_dba is a proxy for no change */
				{
					if (dba_mm == access)
					{
						if (0 != sd->blks_to_upgrd)
						{	/* changing to MM and blocks to upgrade */
							util_out_print("MM access method cannot be set if there are blocks"
								       " to upgrade", TRUE);
							util_out_print("Database file !AD not changed", TRUE,
								       DB_LEN_STR(gv_cur_region));
							exit_status |= EXIT_WRN;
							continue;
						} else if (GDSVCURR != sd->desired_db_format)
						{	/* changing to MM and DB not current format */
							util_out_print("MM access method cannot be set in DB compatibility mode",
								       TRUE);
							util_out_print("Database file !AD not changed", TRUE,
								       DB_LEN_STR(gv_cur_region));
							exit_status |= EXIT_WRN;
							continue;
						} else if (JNL_ENABLED(sd) && (sd->jnl_before_image))
						{	/* changing to MM and BEFORE image journaling set */
							util_out_print("MM access cannot be used with BEFORE image journaling",
								       TRUE);
							util_out_print("Database file !AD not changed", TRUE,
								       DB_LEN_STR(gv_cur_region));
							exit_status |= EXIT_WRN;
							continue;
						} else
						{
							if (!JNL_ENABLED(sd))
								sd->jnl_before_image = 0; /* default to NO_BEFORE journal imaging */
							sd->acc_meth = access;
						}
					}
				}
				sd->clustered = FALSE;
			} else
			{
				if (defer_status)
				{
					util_out_print("DEFER cannot be specified with BG access method, file - !AD not changed",
							TRUE, DB_LEN_STR(gv_cur_region));
					exit_status |= EXIT_WRN;
					continue;
				}

				if (dba_mm == sd->acc_meth)
					space_available = sd->free_space;
				else
				{
					space_available = sd->free_space + sd->lock_space_size;
					if (FALSE == sd->unbacked_cache)
						space_available += ((sd->n_bts + sd->bt_buckets + 1) * SIZEOF(bt_rec));
				}

				temp_new_wc_size = new_wc_size;
				if (0 == new_wc_size)
					if (sd->n_bts)
						temp_new_wc_size = sd->n_bts;
					else
						temp_new_wc_size = WC_DEF_BUFFS;
				if (sd->clustered)
				{
				/* this code needs to be maintained to account for the new bt allocation algorithm - rprp */
					space_needed = (temp_new_wc_size + getprime(temp_new_wc_size) + 1) * SIZEOF(bt_rec)
						+ sd->lock_space_size;
					if (space_needed > space_available)
					{
						if (space_available < (sd->lock_space_size +
							(WC_MIN_BUFFS + getprime(WC_MIN_BUFFS) + 1) * SIZEOF(bt_rec)))
						{
							util_out_print("!/File !AD does not have enough space to be converted to BG"
									, TRUE, DB_LEN_STR(gv_cur_region));
						} else
						{
							util_out_print("!/File !AD does not have enough space for !UL cache "
									"records." , TRUE, DB_LEN_STR(gv_cur_region),
									temp_new_wc_size);
							util_out_print("The maximum it will support is !UL",TRUE,
									sd->n_bts + (sd->free_space / (2 * SIZEOF(bt_rec))));
						}
						exit_status |= EXIT_WRN;
					} else
						sd->free_space = space_available - space_needed;
				} else
				{
					sd->unbacked_cache = TRUE;
#ifdef GT_CX_DEF
					sd->free_space = space_available - sd->lock_space_size;
#endif
				}

				/* On Unix, following block was moved out of this 'if' check, as part of 'targetted msync' changes,
			   	 * which is not relevant in VMS context */
				sd->n_bts = BT_FACTOR(temp_new_wc_size);
				sd->bt_buckets = getprime(sd->n_bts);
				sd->n_wrt_per_flu = 7;
				sd->flush_trigger = FLUSH_FACTOR(sd->n_bts);
				if (n_dba != access)		/* n_dba is a proxy for no change */
					sd->acc_meth = access;
				if (sd->clustered)
				{
					size = LOCK_BLOCK(sd) + ROUND_UP(sd->lock_space_size, DISK_BLOCK_SIZE);
					sd1 = malloc(size);
					memcpy(sd1,sd,SIZEOF(sgmnt_data));
					status = dbcx_ref(sd1, gds_info->fab->fab$l_stv);
					if (0 == (status & 1))
					{
						if (SS$_NORMAL != gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0))
							GTMASSERT;
						gds_info->file_cntl_lsb.lockid = 0;
						sys$dassgn(gds_info->fab->fab$l_stv);
						free(sd1);
						gtm_putmsg(VARLSTCNT(6) ERR_DBFILERR, 2,
							DB_LEN_STR(gv_cur_region), status, gds_info->fab->fab$l_stv);
						exit_status |= EXIT_ERR;
						continue;
					}
					free(sd1);
				}
			}
			if (FALSE == sd->clustered)
			{
				fc->op = FC_WRITE;
				fc->op_buff = sd;
				fc->op_len = SGMNT_HDR_LEN;
				fc->op_pos = 1;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_DBFILERR, 2,
						DB_LEN_STR(gv_cur_region), status, gds_info->fab->fab$l_stv);
					exit_status |= EXIT_ERR;
				}
			}
			status = gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
			assert(SS$_NORMAL == status);
			gds_info->file_cntl_lsb.lockid = 0;
			sys$dassgn(gds_info->fab->fab$l_stv);
			util_out_print("!/File !AD updated.", TRUE, DB_LEN_STR(gv_cur_region));
		} /* end of else part if (!need_standalone) */
	}
	free(sd);
	assert(!(exit_status & EXIT_INF));
	if (exit_status & EXIT_ERR)
		mupip_exit(ERR_WCERRNOTCHG);
	else if (exit_status & EXIT_WRN)
		mupip_exit(ERR_WCWRNNOTCHG);
	return SS_NORMAL; /* for prototype compatibility with Unix */
}
