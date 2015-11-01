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

#include <unistd.h>
#include "gtm_fcntl.h"
#include <sys/ipc.h>
#include <sys/sem.h>
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_string.h"

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
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "util.h"
#include "mupip_set.h"
#include "mu_rndwn_file.h"
#include "mupip_exit.h"
#include "ipcrmid.h"
#include "mu_gv_cur_reg_init.h"
#include "timers.h"
#include "ftok_sems.h"


GBLREF tp_region	*grlist;
GBLREF gd_region        *gv_cur_region;
GBLREF bool		region;
GBLREF bool		in_backup;

#define MAX_ACC_METH_LEN	2

CONDITION_HANDLER(mupip_set_file_ch)
{
	START_CH
	db_ipcs_reset(gv_cur_region, TRUE);
	CONTINUE
}

int4 mupip_set_file(int db_fn_len, char *db_fn)
{
	bool			standalone;
	boolean_t		bypass_partial_recov;
	char			acc_spec[MAX_ACC_METH_LEN], exit_stat, *fn;
	unsigned short		acc_spec_len = MAX_ACC_METH_LEN;
	int			fd, fn_len, new_cache_size, reserved_bytes, new_extn_count,
				new_lock_space, new_disk_wait, status;
	int			glbl_buff_status, defer_status, rsrvd_bytes_status, defer_time,
				extn_count_status, lock_space_status, disk_wait_status;
	sgmnt_data_ptr_t	csd;
	tp_region		*rptr, single;
	enum db_acc_method	access, access_new;
	gd_region		*temp_cur_region;
	char			*errptr;
	int			save_errno;

	error_def(ERR_WCERRNOTCHG);
	error_def(ERR_WCWRNNOTCHG);
	error_def(ERR_INVACCMETHOD);
	error_def(ERR_DBRDERR);
	error_def(ERR_RBWRNNOTCHG);
	error_def(ERR_DBPREMATEOF);

	exit_stat = EXIT_NRM;
	defer_status = cli_present("DEFER_TIME");
	bypass_partial_recov = cli_present("PARTIAL_RECOV_BYPASS") == CLI_PRESENT;
	if (disk_wait_status = cli_present("WAIT_DISK"))
	{
		if (cli_get_int("WAIT_DISK", &new_disk_wait))
		{
			if (new_disk_wait < 0)
			{
				util_out_print("!UL negative, minimum WAIT_DISK allowed is 0.", TRUE, new_disk_wait);
				return (int4)ERR_WCWRNNOTCHG;
			}
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
			if (new_cache_size > WC_MAX_BUFFS)
			{
				util_out_print("!UL too large, maximum write cache buffers allowed is !UL", TRUE, new_cache_size,
						WC_MAX_BUFFS);
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
	}
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
	}
	if (rsrvd_bytes_status = cli_present("RESERVED_BYTES"))
	{
		if (!cli_get_int("RESERVED_BYTES", &reserved_bytes))
		{
			util_out_print("Error getting RESERVED BYTES qualifier value", TRUE);
			return (int4)ERR_RBWRNNOTCHG;
		}
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
	} else
		access = n_dba;		/* really want to keep current method,
					    which has not yet been read */
	if (region)
		rptr = grlist;
	else
	{
		rptr = &single;
		memset(&single, 0, sizeof(single));
	}
	/* We should not establish mupip_set_file_ch before this point, because above is just parsing */
	ESTABLISH_RET(mupip_set_file_ch, (int4)ERR_WCWRNNOTCHG);
	csd = (sgmnt_data *)malloc(ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));
	in_backup = FALSE;		/* Only want yes/no from mupfndfil, not an address */
	for (;  rptr != NULL;  rptr = rptr->fPtr)
	{
		if (region)
		{
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				util_out_print("!/Region !AD is not a GTC access type", TRUE, REG_LEN_STR(rptr->reg));
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
		standalone = mu_rndwn_file(gv_cur_region, TRUE);
		if (FALSE == standalone)
		{
			REVERT;
			return (int4)ERR_WCERRNOTCHG;
		}
		/* we should open it (for changing) after mu_rndwn_file, since mu_rndwn_file changes the file header too */
		if (-1 == (fd = OPEN(fn, O_RDWR)))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("open : !AZ", TRUE, errptr);
			exit_stat |= EXIT_ERR;
			db_ipcs_reset(gv_cur_region, FALSE);
			continue;
		}
		LSEEKREAD(fd, 0, csd, sizeof(sgmnt_data), status);
		if (0 != status)
		{	PERROR("Error reading header of file");
			save_errno = errno;
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
						TRUE,reserved_bytes, MAX_RESERVE_B(csd), fn_len, fn);
				close(fd);
				REVERT;
				db_ipcs_reset(gv_cur_region, FALSE);
				return (int4)ERR_RBWRNNOTCHG;
			}
			csd->reserved_bytes = reserved_bytes;
		}
		access_new = (n_dba == access ? csd->acc_meth : access);
							/* recalculate; n_dba is a proxy for no change */
		change_fhead_timer("FLUSH_TIME", csd->flush_time,
				   (dba_bg == access_new ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM),
				   FALSE);
		if ((n_dba != access) && (csd->acc_meth != access))	/* n_dba is a proxy for no change */
		{
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
			csd->extension_size = new_extn_count;
		if (lock_space_status)
			csd->lock_space_size = new_lock_space * OS_PAGELET_SIZE;
		if(bypass_partial_recov)
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
				if (!cli_get_int("DEFER_TIME", &defer_time))
				{
					util_out_print("Error getting DEFER_TIME qualifier value", TRUE);
					REVERT;
					db_ipcs_reset(gv_cur_region, FALSE);
					return (int4)ERR_RBWRNNOTCHG;
				}
				if (-1 > defer_time)
				{
					util_out_print("DEFER_TIME cannot take negative values less than -1", TRUE);
					util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
					exit_stat |= EXIT_WRN;
					db_ipcs_reset(gv_cur_region, FALSE);
					continue;
				}
				csd->defer_time = defer_time;
			}

		} else
		{
			if (defer_status)
			{
				util_out_print("DEFER cannot be specified with BG access method.", TRUE);
				util_out_print("Database file !AD not changed", TRUE, fn_len, fn);
				exit_stat |= EXIT_WRN;
				db_ipcs_reset(gv_cur_region, FALSE);
				continue;
			}
		}
		LSEEKWRITE(fd, 0, csd, sizeof(sgmnt_data), status);
		if (0 != status)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			util_out_print("Error writing header of file", TRUE);
			util_out_print("Database file !AD not changed: ", TRUE, fn_len, fn);
			rts_error(VARLSTCNT(4) ERR_DBRDERR, 2, fn_len, fn);
		}
		close(fd);
		/* --------------------- report results ------------------------- */
		if (glbl_buff_status)
			util_out_print("Database file !AD now has !UL global buffers",
					TRUE, fn_len, fn, csd->n_bts);
		if (defer_status && (dba_mm == csd->acc_meth))
			util_out_print("Database file !AD now has defer_time set to !ZL",
					TRUE, fn_len, fn, csd->defer_time);
		if (rsrvd_bytes_status)
			util_out_print("Database file !AD now has !UL reserved bytes",
					TRUE, fn_len, fn, csd->reserved_bytes);
		if (extn_count_status)
			util_out_print("Database file !AD now has extension count !UL",
					TRUE, fn_len, fn, csd->extension_size);
		if (lock_space_status)
			util_out_print("Database file !AD now has lock space !UL pages",
					TRUE, fn_len, fn, csd->lock_space_size/OS_PAGELET_SIZE);
		if (disk_wait_status)
			util_out_print("Database file !AD now has wait disk set to !UL seconds",
					TRUE, fn_len, fn, csd->wait_disk_space);
		db_ipcs_reset(gv_cur_region, FALSE);
		mu_gv_cur_reg_free();
	}
	free(csd);
	REVERT;
	assert(!(exit_stat & EXIT_INF));
	return (exit_stat & EXIT_ERR ? (int4)ERR_WCERRNOTCHG :
		(exit_stat & EXIT_WRN ? (int4)ERR_WCWRNNOTCHG : SS_NORMAL));
}
