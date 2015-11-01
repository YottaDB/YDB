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

#include "mdef.h"

#include <sys/ipc.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_ctype.h"
#include "gtm_stat.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"
#include "mupipset.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "util.h"
#include "gvcst_init.h"
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "gds_rundown.h"
#include "ipcrmid.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "tp_change_reg.h"
#include "ftok_sems.h"

error_def(ERR_BFRQUALREQ);
error_def(ERR_DBFILERR);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPRIVERR);
error_def(ERR_DBRDERR);
error_def(ERR_JNLRDONLY);
error_def(ERR_JNLBUFFTOOLG);
error_def(ERR_JNLBUFFTOOSM);
error_def(ERR_JNLERRNOCRE);
error_def(ERR_JNLERRNOTCHG);
error_def(ERR_JNLINVALLOC);
error_def(ERR_JNLINVEXT);
error_def(ERR_JNLNAMLEN);
error_def(ERR_JNLWRNNOCRE);
error_def(ERR_JNLWRNNOTCHG);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUSTANDALONE);
error_def(ERR_JNLDSKALIGN);
error_def(ERR_JNLMINALIGN);
error_def(ERR_JNLINVSWITCHLMT);

#define EXIT_NRM	0
#define EXIT_WRN	2
#define EXIT_ERR	4
#define EXIT_RDONLY	8

#ifdef VMS
#define STANDALONE(x) mu_rndwn_file(TRUE)
#elif defined(UNIX)
#define STANDALONE(x) mu_rndwn_file(x, TRUE)
#else
#error Unsupported platform
#endif

/* This is used only to communicate a useful error exit condition to mupip_set_ch(): */
GBLDEF	uint4			mupip_set_jnl_exit_error;

GBLREF	bool			region;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	mu_set_rlist		*grlist;
GBLREF	seq_num			seq_num_one;
GBLREF  boolean_t               rename_changes_jnllink;

#define IS_UNDERBAR(X) (((X) == '_')? 1:0)

static	char	* const perror_format = "Error %s database file %s",
		* const jnl_parms[] =
		{
			/* Make sure these stay in alphabetical order */
			"ALIGNSIZE",
			"ALLOCATION",
			"AUTOSWITCHLIMIT",
			"BEFORE_IMAGES",
			"BUFFER_SIZE",
			"DISABLE",
			"ENABLE",
			"EPOCH_INTERVAL",
			"EXTENSION",
			"FILENAME",
			"NOBEFORE_IMAGES",
			"NOPREVJNLFILE",
			"NOSYNC_IO",
			"OFF",
			"ON",
			"PREVJNLFILE",
			"SYNC_IO",
			"YIELD_LIMIT"
		};

static char	* const rep_parms[] =
		{
			"OFF",
			"ON"
		};

enum
{
	rep_off,
	rep_on,
	rep_end_of_list
};
enum
{
	/* Make sure these are in the same order as the above initializers */
	jnl_alignsize,
	jnl_allocation,
	jnl_autoswitchlimit,
	jnl_before_images,
	jnl_buffer_size,
	jnl_disable,
	jnl_enable,
	jnl_epoch_interval,
	jnl_extension,
	jnl_filename,
	jnl_nobefore_images,
	jnl_noprevjnlfile,
	jnl_nosync_io,
	jnl_off,
	jnl_on,
	jnl_prevjnlfile,
	jnl_sync_io,
	jnl_yield_limit,
	jnl_end_of_list
};



uint4	mupip_set_journal(bool journal, bool replication, unsigned short db_fn_len, char *db_fn)
{
	jnl_create_info	jnl_info;
	GDS_INFO	*gds_info;
	file_control	*fc;
	int		fd, tmpfd;
	int4		yield_limit;
	mu_set_rlist	*rptr, dummy_rlist;
	gd_segment	*save_dyn_addr;
	sgmnt_data_ptr_t sd;
	bool		alignsize_specified = FALSE, allocation_specified = FALSE, before_images_specified = FALSE,
			buffer_size_specified = FALSE, disable_specified = FALSE, enable_specified = FALSE,
			epoch_interval_specified = FALSE, extension_specified = FALSE, filename_specified = FALSE,
			autoswitchlimit_specified = FALSE, nobefore_images_specified = FALSE, noprevjnlfile_specified = FALSE,
			off_specified = FALSE, on_specified = FALSE, prevjnlfile_specified = FALSE,
			rep_off_specified = FALSE, rep_on_specified = FALSE, yield_limit_specified = FALSE,
			nosync_io_specified = FALSE, sync_io_specified = FALSE, exclusive;
	char		*fn, *jnl_fn, *cptr, *ctop, *c1, rep_str[256], jnl_str[256], entry[256],
			full_jnl_fn[JNL_NAME_SIZE], prev_jnl_fn[JNL_NAME_SIZE];
	int		ccnt, fn_len, jnl_fn_len, prev_jnl_fn_len, index, comparison, num;
	unsigned short	jnl_slen;
	unsigned short	rep_slen;
	uint4		create_status, status,
			exit_status = EXIT_NRM, jnl_status;
	seq_num		max_reg_seqno;
	struct stat     stat_buf;
	int             stat_res;
	unsigned int	full_len, length;

	ESTABLISH_RET(mupip_set_jnl_ch, (uint4)ERR_JNLWRNNOCRE);
	/* First, collect and validate any JOURNAL options present on the command line */
	jnl_status = 0;
	memset(&jnl_info, 0, sizeof(jnl_info));
	jnl_slen = sizeof(jnl_str);
	rep_slen = sizeof(rep_str);

	QWASSIGN(max_reg_seqno, seq_num_one);


	if (journal)
	{
		if (!cli_get_str("JOURNAL", jnl_str, &jnl_slen))
		{
			util_out_print("The -JOURNAL option must have a value", TRUE);
			return (uint4)ERR_MUPCLIERR;
		}
		for (cptr = jnl_str, ctop = &jnl_str[jnl_slen];  cptr < ctop;  ++cptr)
		{
			for (c1 = cptr;  ISALPHA(*cptr) || IS_UNDERBAR(*cptr);  ++cptr)
				;
			length = cptr - c1;
			memcpy(entry, c1, length);
			entry[length] = '\0';
			cli_strupper(entry);
			for (index = 0;  index < jnl_end_of_list;  ++index)
				if ((comparison = strncmp(jnl_parms[index], entry, length)) >= 0)
					break;
			if ((index == jnl_end_of_list)  || (comparison > 0))
			{
				util_out_print("Unrecognized -JOURNAL option: !AD", TRUE, length, entry);
				return (uint4)ERR_MUPCLIERR;
			}
			switch (index)
			{
				case jnl_alignsize:
				case jnl_allocation:
				case jnl_autoswitchlimit:
				case jnl_buffer_size:
				case jnl_epoch_interval:
				case jnl_extension:
				case jnl_yield_limit:
					if ('=' == *cptr++)
					{
						for (c1 = cptr, num = 0;  ISDIGIT(*cptr);  ++cptr)
							num = (num * 10) + (*cptr - '0');
						if (cptr > c1)
						{
							switch (index)
							{
							case jnl_allocation:
								jnl_info.alloc = num;
								allocation_specified = TRUE;
								continue;
							case jnl_buffer_size:
								jnl_info.buffer = num;
								buffer_size_specified = TRUE;
          			                                continue;
							case jnl_extension:
								jnl_info.extend = num;
								extension_specified = TRUE;
            		                                        continue;
							case jnl_alignsize:
								jnl_info.alignsize = num;
								alignsize_specified = TRUE;
              		                                        continue;
							case jnl_epoch_interval:
								jnl_info.epoch_interval = num;
								epoch_interval_specified = TRUE;
								continue;
							case jnl_autoswitchlimit:
								jnl_info.autoswitchlimit = num;
								autoswitchlimit_specified = TRUE;
              		                                        continue;
							case jnl_yield_limit:
								yield_limit = num;
								yield_limit_specified = TRUE;
								continue;
							}
						}
					}
					util_out_print("The !AD option must have a numeric value", TRUE,
						RTS_ERROR_STRING(jnl_parms[index]));
					return (uint4)ERR_MUPCLIERR;
				case jnl_filename:
					if ('=' == *cptr++)
					{
						for (jnl_fn = cptr;  cptr < ctop  &&  ',' != *cptr;  ++cptr)
							;
						if ((jnl_fn_len = cptr - jnl_fn) > 0)
						{
							filename_specified = TRUE;
							continue;
						}
					}
					util_out_print("The FILENAME option must have a value", TRUE);
					return (uint4)ERR_MUPCLIERR;
				case jnl_before_images:
					before_images_specified = TRUE;
					continue;
				case jnl_prevjnlfile:
					if ('=' == *cptr++)
                                        {
                                                for (c1 = cptr;  cptr < ctop  &&  ',' != *cptr;  ++cptr)
                                                        ;
                                                if ((prev_jnl_fn_len = cptr - c1) > 0)
                                                {
                                                        prevjnlfile_specified = TRUE;
                                                        memcpy(prev_jnl_fn, c1, prev_jnl_fn_len);
                                                        prev_jnl_fn[prev_jnl_fn_len] = '\0';
                                                        continue;
                                                }
                                        }
                                        util_out_print("The PREVJNLFILE option must have a value", TRUE);
                                        return (uint4)ERR_MUPCLIERR;
					continue;
				case jnl_noprevjnlfile:
					noprevjnlfile_specified = TRUE;
					continue;
				case jnl_nobefore_images:
					nobefore_images_specified = TRUE;
					continue;
				case jnl_off:
					off_specified = TRUE;
					continue;
				case jnl_on:
					on_specified = TRUE;
					continue;
				case jnl_disable:
					disable_specified = TRUE;
					continue;
				case jnl_enable:
					enable_specified = TRUE;
					continue;
				case jnl_nosync_io:
					nosync_io_specified = TRUE;
					continue;
				case jnl_sync_io:
					sync_io_specified = TRUE;
					continue;
			}
		}
		/* Check for incompatible or invalid options */
		if (noprevjnlfile_specified && prevjnlfile_specified)
		{
			util_out_print("PREVJNLFILE may not be specified with NOPREVJNLFILE option", TRUE);
			return (uint4)ERR_MUPCLIERR;
		}
		if (disable_specified)
		{
			if (enable_specified || on_specified || off_specified || before_images_specified
				|| nobefore_images_specified || filename_specified || allocation_specified
				|| extension_specified || alignsize_specified || buffer_size_specified
				|| epoch_interval_specified || yield_limit_specified || sync_io_specified
				|| nosync_io_specified || autoswitchlimit_specified)
			{
				util_out_print("DISABLE may not be specified with any other options", TRUE);
				return (uint4)ERR_MUPCLIERR;
			}
			journal = FALSE;
		} else
		{
			if (off_specified  &&  on_specified)
			{
				util_out_print("OFF and ON may not both be specified", TRUE);
				return (uint4)ERR_MUPCLIERR;
			}
			if (nosync_io_specified  &&  sync_io_specified)
			{
				util_out_print("NOSYNC_IO and SYNC_IO may not both be specified", TRUE);
				return (uint4)ERR_MUPCLIERR;
			}
			if (before_images_specified)
			{
				if (nobefore_images_specified)
				{
					util_out_print("BEFORE_IMAGES and NOBEFORE_IMAGES may not both be specified", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				jnl_info.before_images = TRUE;
			} else  if (!nobefore_images_specified  &&  !off_specified)
				return (uint4)ERR_BFRQUALREQ;
			if (allocation_specified  &&  (jnl_info.alloc < JNL_ALLOC_MIN  ||  jnl_info.alloc > JNL_ALLOC_MAX))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_JNLINVALLOC, 3, jnl_info.alloc, JNL_ALLOC_MIN, JNL_ALLOC_MAX);
				if (off_specified)
					return (uint4)ERR_JNLWRNNOTCHG;
				return (uint4)ERR_JNLWRNNOCRE;
			}
			if (buffer_size_specified  &&  jnl_info.buffer > JNL_BUFFER_MAX)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_JNLBUFFTOOLG, 2, jnl_info.buffer, JNL_BUFFER_MAX);
				if (off_specified)
					return (uint4)ERR_JNLWRNNOTCHG;
				return (uint4)ERR_JNLWRNNOCRE;
			}
			if (extension_specified  &&  jnl_info.extend > JNL_EXTEND_MAX)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_JNLINVEXT, 2, jnl_info.extend, JNL_EXTEND_MAX);
				if (off_specified)
					return (uint4)ERR_JNLWRNNOTCHG;
				return (uint4)ERR_JNLWRNNOCRE;
			}
			if (alignsize_specified)
			{
				if (off_specified)
				{
					util_out_print("ALIGNSIZE may not be specified with the OFF qualifier", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				if (jnl_info.alignsize < JNL_MIN_ALIGNSIZE)
				{
					gtm_putmsg(VARLSTCNT(4) ERR_JNLMINALIGN, 2, jnl_info.alignsize, JNL_MIN_ALIGNSIZE);
					return (uint4)ERR_JNLWRNNOCRE;
				}
				if (jnl_info.alignsize % DISK_BLOCK_SIZE != 0)
				{
					gtm_putmsg(VARLSTCNT(3) ERR_JNLDSKALIGN, 1, jnl_info.alignsize);
					return (uint4)ERR_JNLWRNNOCRE;
				}
			}
			if (epoch_interval_specified)
			{
				if (nobefore_images_specified)
				{
					util_out_print("NOBEFORE_IMAGES and EPOCH_INTERVAL may not both be specified", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				if (off_specified)
				{
					util_out_print("EPOCH_INTERVAL may not be specified with the OFF qualifier", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				if (jnl_info.epoch_interval <= 0)
				{
					util_out_print("EPOCH_INTERVAL cannot be ZERO (or negative)", TRUE);
					return (uint4)ERR_JNLWRNNOCRE;
				}
				if (jnl_info.epoch_interval > MAX_EPOCH_INTERVAL)
				{
					util_out_print("EPOCH_INTERVAL cannot be greater than !UL", TRUE, MAX_EPOCH_INTERVAL);
					return (uint4)ERR_JNLWRNNOCRE;
				}
			}
			if (autoswitchlimit_specified)
			{
				if (off_specified)
				{
					util_out_print("AUTOSWITCHLIMIT may not be specified with the OFF qualifier", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				if (JNL_AUTOSWITCHLIMIT_MIN > jnl_info.autoswitchlimit || JNL_ALLOC_MAX < jnl_info.autoswitchlimit)
				{
					gtm_putmsg(VARLSTCNT(5) ERR_JNLINVSWITCHLMT, 3, jnl_info.autoswitchlimit,
										JNL_AUTOSWITCHLIMIT_MIN, JNL_ALLOC_MAX);
					return (uint4)ERR_JNLWRNNOCRE;
				}
			}
			if (yield_limit_specified)
			{
				if (off_specified)
				{
					util_out_print("YIELD_LIMIT may not be specified with the OFF qualifier", TRUE);
					return (uint4)ERR_MUPCLIERR;
				}
				if (yield_limit < 0)
				{
					util_out_print("YIELD_LIMIT cannot be NEGATIVE", TRUE);
					return (uint4)ERR_JNLWRNNOCRE;
				}
				if (yield_limit > MAX_YIELD_LIMIT)
				{
					util_out_print("YIELD_LIMIT cannot be greater than !UL", TRUE, MAX_YIELD_LIMIT);
					return (uint4)ERR_JNLWRNNOCRE;
				}
			}
		}
	} else
	{
		if (cli_get_str("JOURNAL", jnl_str, &jnl_slen))
		{
			util_out_print("The -NOJOURNAL option may not specify a value", TRUE);
			return (uint4)ERR_MUPCLIERR;
		}
	}
	if (replication)
	{
		if (CLI_NEGATED == cli_present("JOURNAL"))
		{
			util_out_print("The -NOJOURNAL option can not be specified with -REPLICATION ", TRUE);
			return (uint4)ERR_MUPCLIERR;
		}
		if (!cli_get_str("REPLICATION", rep_str, &rep_slen))
		{
			util_out_print("The -REPLICATION option must have a value", TRUE);
			return (uint4)ERR_MUPCLIERR;
		}
		for (cptr = rep_str, ctop = &rep_str[rep_slen];  cptr < ctop;  ++cptr)
		{
			for (c1 = cptr;  ISALPHA(*cptr);  ++cptr)
				;
			length = cptr - c1;
			memcpy(entry, c1, length);
			entry[length] = '\0';
			cli_strupper(entry);
			for (index = 0;  index < rep_end_of_list;  ++index)
				if ((comparison = strncmp(rep_parms[index], entry, length)) >= 0)
					break;
			if ((index == rep_end_of_list)  || (comparison > 0))
			{
				util_out_print("Unrecognized -REPLICATION option: !AD", TRUE, length, entry);
				return (uint4)ERR_MUPCLIERR;
			}
			switch (index)
			{
				case rep_off:
					rep_off_specified = TRUE;
					continue;
				case rep_on:
					rep_on_specified = TRUE;
					if (!before_images_specified)
					{
						before_images_specified = TRUE;
						jnl_info.before_images = TRUE;
					}
					enable_specified = TRUE;
					continue;
			}
		}
		if (rep_on_specified && off_specified)
		{
			util_out_print("!/Replication On and Journal Off are incompatible");
			return (uint4)ERR_MUPCLIERR;
		}
	}
	mupip_set_jnl_exit_error = rep_off_specified || off_specified || disable_specified ? ERR_JNLERRNOTCHG : ERR_JNLERRNOCRE;

	if (region  &&  NULL == grlist)
	{
		util_out_print("Invalid region name specified on command line. Can't continue any further", TRUE);
		return mupip_set_jnl_exit_error;
	}

	/* Now process the database file or region(s) */
	if (region)
	{
		/* The command line specified one or more regions */
		if ((NULL != grlist->fPtr)  &&  filename_specified)
		{
			util_out_print("!/Multiple database regions cannot be journalled in a single file", TRUE);
			return mupip_set_jnl_exit_error;
		}
	} else
	{
		/* The command line specified a single database file;
		   force the following do-loop to be one-trip */
		dummy_rlist.fPtr= NULL;
		grlist = &dummy_rlist;
	}

	for (rptr = grlist;  EXIT_ERR != exit_status && rptr != NULL; rptr = rptr->fPtr)
	{
		if (region)
		{
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				util_out_print(journal ? "!/Region !AD is not a GTC access type, and cannot be journaled"
						       : "!/Region !AD is not a GTC access type",
					       TRUE, REG_LEN_STR(rptr->reg));
				exit_status |= EXIT_WRN;
				continue;
			}
			if (NULL == mupfndfil(rptr->reg))
			{
				exit_status |= EXIT_ERR;
				continue;
			}
			gv_cur_region = rptr->reg;
			if (NULL == gv_cur_region->dyn.addr->file_cntl)
			{
				gv_cur_region->dyn.addr->acc_meth = dba_bg;
				gv_cur_region->dyn.addr->file_cntl =
					(file_control *)malloc(sizeof(*gv_cur_region->dyn.addr->file_cntl));
				memset(gv_cur_region->dyn.addr->file_cntl, 0, sizeof(*gv_cur_region->dyn.addr->file_cntl));
				gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
				gds_info =
				gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(sizeof(GDS_INFO));
				memset(gds_info, 0, sizeof(GDS_INFO));
			}
		} else
		{
			mu_gv_cur_reg_init();
			rptr->reg = gv_cur_region;
			gv_cur_region->dyn.addr->fname_len = db_fn_len;
			memcpy(gv_cur_region->dyn.addr->fname, db_fn, db_fn_len);
		}
		gds_info = FILE_INFO(gv_cur_region);
		fc = gv_cur_region->dyn.addr->file_cntl;
		/* open shared to see what's possible */
		gvcst_init(gv_cur_region);
		tp_change_reg();
		sd = cs_data;
		rptr->sd = sd;
		if (gv_cur_region->was_open)	/* ??? */
		{
			gds_rundown();
			rptr->sd = NULL;
			continue;
		}
		if (TRUE == gv_cur_region->read_only)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBPRIVERR, 2,
				DB_LEN_STR(gv_cur_region));
			exit_status |= EXIT_RDONLY;
			gds_rundown();
			rptr->sd = NULL;
			continue;
		}
		grab_crit(gv_cur_region);
		if (((jnl_notallowed == sd->jnl_state) && (TRUE == enable_specified)) || ((jnl_notallowed != sd->jnl_state)
			&& ((FALSE == journal && FALSE == replication) || (buffer_size_specified
			&& (jnl_info.buffer != sd->jnl_buffer_size)))) || (repl_open == sd->repl_state && rep_off_specified))
		{
			save_dyn_addr = gv_cur_region->dyn.addr;
			exclusive = TRUE;
			gds_rundown();
			rptr->sd = sd = NULL;
			gv_cur_region->dyn.addr = save_dyn_addr;
			/* WARNING: the remaining code uses the gv_cur_region and co
			 * on the assumption that gds_rundown does not deallocate the space when it closes the file */
			if (TRUE == STANDALONE(gv_cur_region))
			{
				fc->op = FC_OPEN;
				fc->file_type = dba_bg;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(9) ERR_DBOPNERR, 2,
						DB_LEN_STR(gv_cur_region), 0, status, 0);
					exit_status |= EXIT_ERR;
					continue;
				}
				sd = (sgmnt_data_ptr_t)malloc(ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));
				fc->op = FC_READ;
				fc->op_buff = (sm_uc_ptr_t)sd;
				fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(9) ERR_DBRDERR, 2,
						DB_LEN_STR(gv_cur_region), 0, status, 0);
					exit_status |= EXIT_ERR;
					close(gds_info->fd);
					db_ipcs_reset(gv_cur_region, TRUE);
					continue;
				}
			} else
			{
				gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
				exit_status |= EXIT_ERR;
				continue;
			}
		} else
			exclusive = FALSE;

		rptr->fd = gds_info->fd;
		rptr->sd = sd;
		rptr->exclusive = exclusive;
		rptr->state = ALLOCATED;
		if (QWLT(max_reg_seqno, sd->reg_seqno))
			QWASSIGN(max_reg_seqno, sd->reg_seqno);
	}
	for (rptr = grlist; (EXIT_NRM == exit_status || exit_status == EXIT_RDONLY)  &&  rptr != NULL; rptr = rptr->fPtr)
	{
		rename_changes_jnllink = FALSE;
		gv_cur_region = rptr->reg;
		tp_change_reg();
		exclusive = rptr->exclusive;
		fd = rptr->fd;
		sd = rptr->sd;
		if (sd == NULL)
			continue;
		fc = gv_cur_region->dyn.addr->file_cntl;
		assert(-1 != fd);

		/* Get the full database path */
		jnl_info.fn = (char *)gv_cur_region->dyn.addr->fname;
		fn = (char *)gv_cur_region->dyn.addr->fname;
		jnl_info.fn_len = fn_len = gv_cur_region->dyn.addr->fname_len;
		create_status = EXIT_NRM;
		if (journal || replication)
		{
			if ((jnl_notallowed == sd->jnl_state) && !enable_specified && !replication)
			{
				util_out_print("!/Journaling is not enabled for ", FALSE);
				if (region)
					util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
				else
					util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
				continue;
			}
			if ((jnl_closed == sd->jnl_state) && enable_specified && !on_specified && !replication)
			{
				sd->jnl_state = jnl_open;
				util_out_print("!/Journaling is enabled for ", FALSE);
				if (region)
					util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
				else
					util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
				continue;
			}
			/* Fill in any unspecified information */
			if (FALSE == allocation_specified)
				jnl_info.alloc = (0 == sd->jnl_alq) ? JNL_ALLOC_DEF : sd->jnl_alq;

			if (FALSE == extension_specified)
				jnl_info.extend = (0 == sd->jnl_deq) ? jnl_info.alloc * JNL_EXTEND_DEF_PERC : sd->jnl_deq;

			if (FALSE == buffer_size_specified)
				jnl_info.buffer = (0 == sd->jnl_buffer_size) ? JNL_BUFFER_DEF : sd->jnl_buffer_size;

			if (FALSE == alignsize_specified)
				jnl_info.alignsize = JNL_MIN_ALIGNSIZE;

			if (FALSE == autoswitchlimit_specified)
				jnl_info.autoswitchlimit = JNL_ALLOC_MAX;

			if (FALSE == epoch_interval_specified)
				jnl_info.epoch_interval = DEFAULT_EPOCH_INTERVAL;

			if (filename_specified)
			{
				if (!get_full_path(jnl_fn, jnl_fn_len, full_jnl_fn, &full_len, JNL_NAME_SIZE))
				{
					gtm_putmsg(VARLSTCNT(7) ERR_JNLNAMLEN, 5, jnl_fn_len, jnl_fn, fn_len, fn, JNL_NAME_SIZE);
					exit_status |= EXIT_ERR;
					continue;
				}
				jnl_info.jnl_len = full_len;
			} else
			{
				if (0 == sd->jnl_file_len)
				{
					for (cptr = fn, ccnt = 0;
					     ('.' != *cptr)  &&  ('\0' != *cptr)  &&  (ccnt < JNL_NAME_SIZE - 5);
					     ++cptr, ++ccnt)
						;
					if (!get_full_path(fn, cptr - fn, full_jnl_fn, &length, JNL_NAME_SIZE - 5))
					{
						gtm_putmsg(VARLSTCNT(7) ERR_JNLNAMLEN, 5, jnl_fn_len, jnl_fn, fn_len, fn,
							  JNL_NAME_SIZE - 5);
						exit_status |= EXIT_ERR;
						continue;
					}
					strcpy(full_jnl_fn + length, ".mjl");
					jnl_info.jnl_len = length + sizeof(".mjl") - 1;
				} else
				{
					memcpy(full_jnl_fn, sd->jnl_file_name, sd->jnl_file_len);
					full_jnl_fn[sd->jnl_file_len] = '\0';
					jnl_info.jnl_len = sd->jnl_file_len;
				}
			}
			jnl_info.jnl = full_jnl_fn;
			if (jnl_notallowed == sd->jnl_state)
			{
				assert(0 == sd->jnl_file_len);
				sd->jnl_file_len = 0;
			}
			jnl_info.prev_jnl = prev_jnl_fn;
			if (prevjnlfile_specified)
                                jnl_info.prev_jnl_len = prev_jnl_fn_len;
			else if (!noprevjnlfile_specified && 0 != sd->jnl_file_len)
			/* User does not want to cut journal link and current journal exists */
			{
				if (!off_specified && !rep_off_specified)
				{
					/* We will create a new journal. Make sure current one is not removed,
					   for any reason. If removed, cut the journal file link for the new one to create */
					STAT_FILE((char *)sd->jnl_file_name, &stat_buf, stat_res);
					if (-1 == stat_res && ENOENT == errno)
					{
						jnl_info.prev_jnl_len = 0;
						util_out_print("prev_jnl_file name changed from !AD to NULL",
							TRUE, JNL_LEN_STR(sd));
					}
					else
					{
						memcpy(prev_jnl_fn, sd->jnl_file_name, sd->jnl_file_len);
						prev_jnl_fn[sd->jnl_file_len] = '\0';
						jnl_info.prev_jnl_len = sd->jnl_file_len;
					}
				}
				else /* do not worry about existence of current journal,
				        if you do not create a new journal */
				{
					memcpy(prev_jnl_fn, sd->jnl_file_name, sd->jnl_file_len);
					prev_jnl_fn[sd->jnl_file_len] = '\0';
					jnl_info.prev_jnl_len = sd->jnl_file_len;
				}
			}
			else
				jnl_info.prev_jnl_len = 0;
			jnl_info.rsize = sd->blk_size + DISK_BLOCK_SIZE;
			jnl_info.tn = sd->trans_hist.curr_tn;
			jnl_info.repl_state = sd->repl_state;
			if (rep_on_specified)
				jnl_info.repl_state = repl_open;
			if (rep_off_specified)
				jnl_info.repl_state = repl_closed;
			QWASSIGN(jnl_info.reg_seqno, max_reg_seqno);
			if (jnl_info.buffer < sd->blk_size / DISK_BLOCK_SIZE + 1)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_JNLBUFFTOOSM, 2, jnl_info.buffer, sd->blk_size / DISK_BLOCK_SIZE + 1);
				assert(exclusive);
				exit_status |= EXIT_WRN;
				jnl_info.buffer = sd->jnl_buffer_size;
				if (region)
					util_out_print("!/Journal file buffer size for region !AD not changed:", TRUE,
						REG_LEN_STR(gv_cur_region));
				else
					util_out_print("!/Journal file buffer size for database !AD not changed:", TRUE,
						jnl_info.fn_len, jnl_info.fn);
			}
			if ((jnl_closed == sd->jnl_state) && on_specified)
				sd->jnl_state = jnl_open;

			if (!rep_off_specified && jnl_open == sd->jnl_state && sd->jnl_file.u.inode != 0)
			{
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
					if (0 != cs_addrs->jnl->pini_addr)
						jnl_put_jrt_pfin(cs_addrs);
					jnl_file_close(gv_cur_region, TRUE, TRUE);
				} else
				{
					gtm_putmsg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data),
						DB_LEN_STR(gv_cur_region));
					exit_status |= EXIT_ERR;
					continue;
				}
			}

			if (!off_specified && !rep_off_specified)
			{
				if (-1 != (tmpfd=OPEN(jnl_info.jnl, O_RDONLY))) /* if file exists */
				{
					close(tmpfd);

					if (-1 == (tmpfd=OPEN(jnl_info.jnl, O_RDWR)))
					{
						gtm_putmsg(VARLSTCNT(4) ERR_JNLRDONLY, 2,
							jnl_info.jnl_len, jnl_info.jnl);
						exit_status |= EXIT_RDONLY;
						gds_rundown();
						if (rptr->exclusive)
							free(rptr->sd);	/* We malloced rptr->sd earlier in this function */
						rptr->sd = NULL;	/* Smokey says only you can prevent spurious rundowns */
						continue;
					}
					close(tmpfd);
				}
			}
			if (off_specified && sd->repl_state == repl_open)
			{
				util_out_print("!/Replication is enabled for file !AD", FALSE, jnl_info.fn_len, jnl_info.fn);
				util_out_print("!/Journaling can not be turned off for file !AD", TRUE,
					jnl_info.jnl_len, jnl_info.jnl);
				continue;
			}
			if (!off_specified && !rep_off_specified)
                        {
                                if (jnl_info.jnl_len > JNL_NAME_SIZE)
                                {
                                        jnl_info.status = ENAMETOOLONG;
                                        util_out_print("!/Journal file !AD not created:", TRUE, jnl_info.jnl_len, jnl_info.jnl);
                                        gtm_putmsg(VARLSTCNT(1) jnl_info.status);
                                        exit_status |= EXIT_ERR;
                                        continue;
                                }
                                if (!prevjnlfile_specified && !noprevjnlfile_specified &&
                                        0 == memcmp(sd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len))
                                        rename_changes_jnllink = TRUE;
                                create_status = cre_jnl_file(&jnl_info);
                        }
			if (off_specified || rep_off_specified || EXIT_NRM == create_status)
			{
				if (off_specified || rep_off_specified)
				{
					if (sd->repl_state == repl_open)
					{
						sd->repl_state = repl_closed;
						util_out_print("!/Replication is closed for ", FALSE);
						if (region)
							util_out_print("  region !AD", FALSE, REG_LEN_STR(gv_cur_region));
						else
							util_out_print("  database !AD", FALSE, jnl_info.fn_len, jnl_info.fn);
					}
					else if(rep_off_specified)
					{
						sd->jnl_state = jnl_closed;
						util_out_print("!/Replication is currently inactive for ", FALSE);
						if (region)
							util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
						else
							util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
					}

					switch (sd->jnl_state)
					{
					case jnl_open:
						/* Notify any active GT.M processes that they must close the journal file */
						sd->jnl_state = jnl_closed;
						util_out_print("!/Journal file !AD closed;  transactions for", TRUE,
							jnl_info.prev_jnl_len, jnl_info.prev_jnl);
						if (region)
							util_out_print("  region !AD", FALSE, REG_LEN_STR(gv_cur_region));
						else
							util_out_print("  database !AD", FALSE, jnl_info.fn_len, jnl_info.fn);
						util_out_print(" are no longer journalled", TRUE);
						break;
					case jnl_closed:
						util_out_print("!/Journaling is currently inactive for ", FALSE);
						if (region)
							util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
						else
							util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
						util_out_print("  - no new journal file created", TRUE);
						break;
					case jnl_notallowed:
						sd->jnl_state = jnl_closed;
						util_out_print("!/Journaling is now enabled but inactive for ", FALSE);
						if (region)
							util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
						else
							util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
						util_out_print("  - no journal file created", TRUE);
						break;
					default:
						assert(FALSE);
					}
				} else
				{
					/* Journaling is to be turned (or left) on;  a new journal file has been created */
					if (!off_specified && jnl_open == sd->jnl_state)
						util_out_print("Previous journal file !AD closed", TRUE,
							jnl_info.prev_jnl_len, jnl_info.prev_jnl);
					if (sd->repl_state == repl_open)
						util_out_print("Replication is already enabled for file !AD", TRUE,
							jnl_info.fn_len, jnl_info.fn);
					if (rep_on_specified)
					{
						sd->repl_state = repl_open;
						util_out_print("Replication is enabled for file !AD", TRUE,
							jnl_info.fn_len, jnl_info.fn);
					}

					util_out_print("!/Journal file !AD created for", TRUE, jnl_info.jnl_len, jnl_info.jnl);
					if (region)
						util_out_print("  region !AD", FALSE, REG_LEN_STR(gv_cur_region));
					else
						util_out_print("  database !AD", FALSE, jnl_info.fn_len, jnl_info.fn);
					util_out_print(jnl_info.before_images ? " (Before-images enabled)"
									      : " (Before-images disabled)",
						       TRUE);
					if (jnl_open != sd->jnl_state)
						sd->jnl_state = jnl_open;
				}
				sd->trans_hist.header_open_tn = jnl_info.tn;
				strcpy((char *)sd->jnl_file_name, jnl_info.jnl);
				sd->jnl_file_len = jnl_info.jnl_len;
				sd->jnl_buffer_size = ROUND_UP(jnl_info.buffer, IO_BLOCK_SIZE / DISK_BLOCK_SIZE);
				sd->jnl_alq = jnl_info.alloc;
				sd->jnl_deq = jnl_info.extend;
				sd->jnl_before_image = jnl_info.before_images;
				if (yield_limit_specified)
					sd->yield_lmt = yield_limit;
				if (sync_io_specified)
					sd->jnl_sync_io = TRUE;
				else if (nosync_io_specified)
					sd->jnl_sync_io = FALSE;
			} else
			{	/* There was an error attempting to create the journal file */
				exit_status |= create_status;
				util_out_print("!/Journal file !AD not created:", TRUE, jnl_info.jnl_len, jnl_info.jnl);
				gtm_putmsg(VARLSTCNT(1) jnl_info.status);
			}
		} else
		{	/* Journaling is to be disabled */
			if (jnl_notallowed == sd->jnl_state && !replication)
				util_out_print("!/Journaling is not enabled for ", FALSE);
			else
			{
				if (sd->repl_state == repl_open)
					util_out_print("!/Replication is on journaling can not be disabled for ", FALSE);
				else
				{
					sd->jnl_state = jnl_notallowed;
					sd->jnl_file_len = 0;
					util_out_print("!/Journaling is now disabled for ", FALSE);
				}
			}
			if (region)
				util_out_print("region !AD", TRUE, REG_LEN_STR(gv_cur_region));
			else
				util_out_print("database !AD", TRUE, jnl_info.fn_len, jnl_info.fn);
		}
		if (EXIT_NRM == create_status)
		{	/* Write the updated information back to the database file */
			fc->op = FC_WRITE;
			fc->op_buff = (sm_uc_ptr_t)sd;
			status = dbfilop(fc);
			if (SS_NORMAL != status)
			{
				gtm_putmsg(VARLSTCNT(9) ERR_DBFILERR, 2,
					DB_LEN_STR(gv_cur_region), 0, status, 0);
				exit_status |= EXIT_ERR;
			}
		}
	}
	mupip_set_jnl_cleanup();
	if (EXIT_NRM == exit_status)
		return (uint4)SS_NORMAL;
	if (exit_status & EXIT_ERR)
		return mupip_set_jnl_exit_error;
	if (off_specified  ||  disable_specified)
		return (uint4)ERR_JNLWRNNOTCHG;
	return (uint4)ERR_JNLWRNNOCRE;
}
