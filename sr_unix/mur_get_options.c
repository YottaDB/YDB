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
#include "gtm_unistd.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_string.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "cli.h"
#include "min_max.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"
#include "gtm_bintim.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	seq_num		seq_num_zero;
GBLREF  seq_num		resync_jnl_seqno;

error_def(ERR_MUPCLIERR);
error_def(ERR_DBFILERR);

static	char	default_since_time[] = "0 0:0:30",
		default_lookback_time[] = "0 0:5:00";

void	mur_get_options(void)
{
	int4		status;
	unsigned short	length, buf_len;
	char		*c, *ctop, *c1, qual_buffer[MAX_LINE], full_db_fn[MAX_FN_LEN + 1], buf[32];
	char		bool_buff[8]; /* To hold string TRUE or, FALSE */
	int		top;
	unsigned int	full_len;
	bool		exclude;
	fi_type		*extr_file_info;
	fi_type		*losttrans_file_info;
	long_list	*ll_ptr, *ll_ptr1;
	redirect_list	*rl_ptr, *rl_ptr1;
	select_list	*sl_ptr, *sl_ptr1;

	memset(&mur_options, 0, sizeof mur_options);
	/*----- JOURNAL ACTION QUALIFIERS -----*/
	/*
	 *	-RECOVER
	 */
	mur_options.update = cli_present("RECOVER") == CLI_PRESENT;
	/*----- JOURNAL ACTION QUALIFIERS -----*/
	/*
	 *	-ROLLBACK
	 */
	mur_options.rollback = cli_present("ROLLBACK") == CLI_PRESENT;
	mur_options.losttrans = cli_present("LOSTTRANS") == CLI_PRESENT;
	if (mur_options.rollback)
		mur_options.update = TRUE;
	if (CLI_PRESENT == cli_present("RESYNC"))
	{
		if (CLI_PRESENT == !cli_present("LOSTTRANS"))
		{
			util_out_print("-LOSTTRANS must be specified with -RESYNC", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		buf_len = sizeof(buf);
		cli_get_str("RESYNC", buf, &buf_len);
		QWASSIGN(resync_jnl_seqno, asc2l((uchar_ptr_t)buf, strlen(buf)));
		if (QWEQ(resync_jnl_seqno, seq_num_zero))
		{
			util_out_print("-RESYNC qualifier must be given a value greater than zero", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	if ((status  = cli_present("FETCHRESYNC")) == CLI_PRESENT)
	{
		if (!cli_present("LOSTTRANS") == CLI_PRESENT)
		{
			util_out_print("-LOSTTRANS must be specified with -FETCHRESYNC", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (!cli_get_num("FETCHRESYNC", &mur_options.fetchresync))
			mupip_exit(ERR_MUPCLIERR);
	}
	if ((status  = cli_present("EPOCHLIMIT")) == CLI_PRESENT)
	{
		if (!cli_get_num("EPOCHLIMIT", &mur_options.epoch_limit))
			mupip_exit(ERR_MUPCLIERR);
	} else if (status == CLI_NEGATED)
		mur_options.epoch_limit = 0;
	/*
	 *	-[NO]VERIFY
	 */
	mur_options.verify = cli_present("VERIFY") != CLI_NEGATED;
	if (mur_options.rollback)
		mur_options.verify = TRUE;
	/*
	 *	-SHOW[=(ALL|HEADER|PROCESSES|ACTIVE_PROCESSES|BROKEN_TRANSACTIONS|STATISTICS)]
	 */
	if (cli_present("SHOW") == CLI_PRESENT)
	{
		if (!cli_get_value("SHOW", qual_buffer)  ||  (top = strlen(qual_buffer)) == 0)
			mur_options.show = SHOW_ALL;
		else
		{
			lower_to_upper((uchar_ptr_t)qual_buffer, (uchar_ptr_t)qual_buffer, top);
			for (c1 = c = qual_buffer, ctop = &qual_buffer[top];  c < ctop;  c1 = ++c)
			{
				while (*c != '\0'  &&  *c != ',')
					++c;
				length = c - c1;
				if	(memcmp(c1, "HEADER", MIN(length, 6)) == 0)
					mur_options.show |= SHOW_HEADER;
				else if	(memcmp(c1, "PROCESSES", MIN(length, 9)) == 0)
					mur_options.show |= SHOW_ALL_PROCESSES;
				else if	(memcmp(c1, "ACTIVE_PROCESSES", MIN(length, 16)) == 0)
					mur_options.show |= SHOW_ACTIVE_PROCESSES;
				else if	(memcmp(c1, "BROKEN_TRANSACTIONS", MIN(length, 19)) == 0)
					mur_options.show |= SHOW_BROKEN;
				else if	(memcmp(c1, "STATISTICS", MIN(length, 10)) == 0)
					mur_options.show |= SHOW_STATISTICS;
				else if	(memcmp(c1, "ALL", MIN(length, 3)) == 0)
					mur_options.show |= SHOW_ALL;
				else
				{
					util_out_print("Invalid -SHOW qualifier value;  specify one or more of:  HEADER,", TRUE);
					util_out_print("  PROCESSES, ACTIVE_PROCESSES, BROKEN_TRANSACTIONS, STATISTICS, or ALL",
							TRUE);
					mupip_exit(ERR_MUPCLIERR);
				}
			}
		}
	} else if (mur_options.update)
		mur_options.show = SHOW_BROKEN;
	mur_options.detail = cli_present("DETAIL") == CLI_PRESENT;
	/*
	 *	-LOSTTRANS=[file-name]
	 */
	if (mur_options.rollback && !mur_options.losttrans)
	{
		util_out_print("-LOSTTRANS must be specified with -ROLLBACK", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	if (!mur_options.rollback && mur_options.losttrans)
	{
		util_out_print("-ROLLBACK must be specified with -LOSTTRANS", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	if (mur_options.losttrans)
	{
		length = sizeof(qual_buffer);
		if (cli_get_str("LOSTTRANS", qual_buffer, &length))
		{
			losttrans_file_info = (fi_type *)(mur_options.losttrans_file_info = (void *)malloc(sizeof(fi_type)));
			losttrans_file_info->fn_len = strlen(qual_buffer);
			losttrans_file_info->fn = (char *)malloc(losttrans_file_info->fn_len + 1);
			memcpy(losttrans_file_info->fn, qual_buffer, losttrans_file_info->fn_len);
		}
		/* Let mur_open_files() figure out the file name */
		/* This is added to take the default option for extract file name */
                if ((strcmp(losttrans_file_info->fn, "losttrans.mlt") == 0))
			losttrans_file_info->fn_len = 0;
	}
	/*
	 *	-EXTRACT[=file-name]
	 */
	if (cli_present("EXTRACT") == CLI_PRESENT)
	{
		extr_file_info = (fi_type *)(mur_options.extr_file_info = (void *)malloc(sizeof(fi_type)));
		if (cli_get_value("EXTRACT", qual_buffer))
		{
			extr_file_info->fn_len = strlen(qual_buffer);
			extr_file_info->fn = (char *)malloc(extr_file_info->fn_len + 1);
			strcpy(extr_file_info->fn, qual_buffer);
		}
		/* Let mur_open_files() figure out the file name */
		/* This is added to take the default option for extract file name */
                if ((strcmp(extr_file_info->fn, "extract.mjf") == 0))
			extr_file_info->fn_len = 0;
	}
	if (!mur_options.rollback && !mur_options.update  &&  !mur_options.verify
			&&  mur_options.show == 0  &&  mur_options.extr_file_info == NULL)
	{
		util_out_print("One or more of -ROLLBACK, -RECOVER, -VERIFY, -SHOW, or -EXTRACT must be specified", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	/*----- JOURNAL DIRECTION QUALIFIERS -----*/
	/*
	 *	-FORWARD
	 */
	mur_options.forward = cli_present("FORWARD") == CLI_PRESENT;
	/*
	 *	-BACKWARD
	 */
	if (mur_options.forward == (cli_present("BACKWARD") == CLI_PRESENT))
	{
		util_out_print("Either -FORWARD or -BACKWARD (but not both) must be specified", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	 /* Rollback is currently supported for backward qualifier only */
	if (mur_options.forward && mur_options.rollback)
        {
        	util_out_print(" -FORWARD is currently not supported with -ROLLBACK", TRUE);
                mupip_exit(ERR_MUPCLIERR);
        }
	/*----- JOURNAL TIME QUALIFIERS -----*/
	/*
	 *	-AFTER=delta_or_absolute_time
	 */
	if (mur_options.since = (cli_present("AFTER") == CLI_PRESENT))
	{
		if (!mur_options.forward  ||  mur_options.extr_file_info == NULL)
		{
			util_out_print("-AFTER may only be specified with -EXTRACT and -FORWARD", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (!cli_get_value("AFTER", qual_buffer)  ||  gtm_bintim(qual_buffer, &mur_options.since_time) == -1)
		{
			util_out_print("Invalid -AFTER qualifier value;  specify -AFTER=delta_or_absolute_time", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	/*
	 *	-SINCE=delta_or_absolute_time
	 */
	else if (mur_options.since = (cli_present("SINCE") == CLI_PRESENT))
	{
		if (mur_options.forward)
		{
			util_out_print("-SINCE may only be specified with -BACKWARD", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (!cli_get_value("SINCE", qual_buffer)  ||  gtm_bintim(qual_buffer, &mur_options.since_time) == -1)
		{
			util_out_print("Invalid -SINCE qualifier value;  specify -SINCE=delta_or_absolute_time", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else if (!mur_options.forward)
		(void)gtm_bintim(default_since_time, &mur_options.since_time);
	/*
	 *	-BEFORE=delta_or_absolute_time
	 */
	if ((mur_options.before = (cli_present("BEFORE") == CLI_PRESENT))  &&
	    (!cli_get_value("BEFORE", qual_buffer)  ||  gtm_bintim(qual_buffer, &mur_options.before_time) == -1))
	{
		util_out_print("Invalid -BEFORE qualifier value;  specify -BEFORE=delta_or_absolute_time", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	/*
	 *	-[NO]LOOKBACK_LIMIT[=("TIME=delta_or_absolute_time","OPERATIONS=integer")]
	 */
	if ((status = cli_present("LOOKBACK_LIMIT")) == CLI_PRESENT)
	{
		mur_options.lookback_time_specified = TRUE;
		if (mur_options.forward)
		{
			util_out_print("-LOOKBACK_LIMIT may only be specified with -BACKWARD", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (!cli_get_value("LOOKBACK_LIMIT", qual_buffer)  ||  (top = strlen(qual_buffer)) == 0)
		{
			util_out_print("-LOOKBACK_LIMIT requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		lower_to_upper((uchar_ptr_t)qual_buffer, (uchar_ptr_t)qual_buffer, top);
		for (c1 = c = qual_buffer, ctop = &qual_buffer[top];  c < ctop;  c1 = ++c)
		{
			while (*c != '='  &&  *c != '\0'  &&  *c != ',')
				++c;
			if (*c == '=')
			{
				length = c - c1;
				if (memcmp(c1, "OPERATIONS", MIN(length, 10)) == 0)
				{
					mur_options.lookback_opers_specified = TRUE;
					c1 = ++c;
					while (*c != '\0'  &&  *c != ',')
						++c;
					if ((mur_options.lookback_opers = asc2i((uchar_ptr_t)c1, c - c1)) != -1)
						continue;
				} else if (memcmp(c1, "TIME", MIN(length, 4)) == 0)
				{
					c1 = ++c;
					while (*c != '\0'  &&  *c != ',')
						++c;
					*c = '\0';
					if (gtm_bintim(c1, &mur_options.lookback_time) == 0)
						continue;
				}
			}
			util_out_print("Invalid -LOOKBACK_LIMIT qualifier value;  specify as quoted string(s):", TRUE);
			util_out_print("  \"OPERATIONS=integer\" and/or \"TIME=delta_or_absolute_time\"", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else if (status != CLI_NEGATED  &&  !mur_options.forward)
		(void)gtm_bintim(default_lookback_time, &mur_options.lookback_time);
	/*----- JOURNAL CONTROL QUALIFIERS -----*/
	/*
	 *	-REDIRECT=(old-file-name=new-file-name,...)
	 */
	if (cli_present("REDIRECT") == CLI_PRESENT)
	{
		if (!mur_options.update)
		{
			util_out_print("-REDIRECT may only be specified with -RECOVER");
			mupip_exit(ERR_MUPCLIERR);
		}
		if (!cli_get_value("REDIRECT", qual_buffer))
			mupip_exit(ERR_MUPCLIERR);

		if ((top = strlen(qual_buffer)) == 0)
		{
			util_out_print("-REDIRECT requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (*qual_buffer != '('  ||  qual_buffer[top-1] != ')')
		{
			util_out_print("-REDIRECT requires the value within parantheses", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		qual_buffer[top-1] = '\0';
		for (c1 = c = &qual_buffer[1], ctop = &qual_buffer[top-1];  c < ctop;  c1 = ++c)
		{
			while (*c != '\0'  &&  *c != ','  &&  *c != '=')
				++c;
			if (*c != '=')
			{
				util_out_print("Invalid -REDIRECT qualifier value;  specify as:", TRUE);
				util_out_print("  (old-file-name=new-file-name,...)", TRUE);
				mupip_exit(ERR_MUPCLIERR);
			}
			rl_ptr1 = (redirect_list *)malloc(sizeof(redirect_list));
			rl_ptr1->next = NULL;
			if (mur_options.redirect == NULL)
				mur_options.redirect = rl_ptr1;
			else
				rl_ptr->next = rl_ptr1;
			rl_ptr = rl_ptr1;
			*c = '\0';
			if (!get_full_path(c1, c - c1, full_db_fn, &full_len, sizeof(full_db_fn)))
			{
				util_out_print("Invalid -REDIRECT qualifier value: Unable to find full pathname", TRUE);
				rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, c - c1, c1);
			}
			rl_ptr->org_name_len = full_len;
			rl_ptr->org_name = (char *)malloc(rl_ptr->org_name_len + 1);
			strcpy(rl_ptr->org_name, full_db_fn);
			rl_ptr->org_name[rl_ptr->org_name_len] = '\0';
			for (c1 = ++c;  *c != '\0'  &&  *c != ',';  ++c)
				;
			*c = '\0';
			if (!get_full_path(c1, c - c1, full_db_fn, &full_len, sizeof(full_db_fn)))
			{
				util_out_print("Invalid -REDIRECT qualifier value: Unable to find full pathname", TRUE);
				rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, c - c1, c1);
			}
			rl_ptr->new_name_len = full_len;
			rl_ptr->new_name = (char *)malloc(rl_ptr->new_name_len + 1);
			strcpy(rl_ptr->new_name, full_db_fn);
			rl_ptr->new_name[rl_ptr->new_name_len] = '\0';
		}
	}
	/*
	 *	-FENCES=NONE|ALWAYS|PROCESS
	 */
	if (cli_present("FENCES") == CLI_PRESENT)
	{
		length = sizeof qual_buffer;
		if (!cli_get_str("FENCES", qual_buffer, &length))
			mupip_exit(ERR_MUPCLIERR);

		lower_to_upper((uchar_ptr_t)qual_buffer, (uchar_ptr_t)qual_buffer, length);

		if (memcmp(qual_buffer, "NONE", MIN(length, 4)) == 0)
			mur_options.fences = FENCE_NONE;
		else if	(memcmp(qual_buffer, "ALWAYS", MIN(length, 6)) == 0)
			mur_options.fences = FENCE_ALWAYS;
		else if	(memcmp(qual_buffer, "PROCESS", MIN(length, 7)) == 0)
			mur_options.fences = FENCE_PROCESS;
		else
		{
			util_out_print("Invalid -FENCES qualifier value;  specify one of:  ALWAYS, PROCESS or NONE", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		mur_options.fences = FENCE_PROCESS;
	/*
	 *	-[NO]INTERACTIVE
	 */
	mur_options.interactive = (CLI_PRESENT == cli_present("INTERACTIVE")) && isatty(0);
	/*
	 *	-[NO]CHAIN
	 */
	if ((status = cli_present("CHAIN")) == CLI_NEGATED)
	{
		if (mur_options.rollback)
		{
			util_out_print("NOCHAIN can't be specified with Rollback", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		} else
			mur_options.chain = FALSE;
	} else
		mur_options.chain = TRUE; /* By Default or specified without negation */
	/*
	 *	-[NO]ERROR_LIMIT[=integer]
	 */
	if ((status = cli_present("ERROR_LIMIT")) == CLI_PRESENT)
	{
		if (!cli_get_num("ERROR_LIMIT", &mur_options.error_limit))
			mupip_exit(ERR_MUPCLIERR);

		if (mur_options.error_limit < 0)
		{
			util_out_print("Invalid -ERROR_LIMIT qualifier value;  must be at least zero", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else if (status == CLI_NEGATED)
		mur_options.error_limit = 1000000;
	/*
	 *	-[NO]CHECKTN
	 */
	if ((mur_options.notncheck = (cli_present("CHECKTN") == CLI_NEGATED))  &&  !mur_options.forward)
	{
		util_out_print("-NOCHECKTN may only be specified with -FORWARD", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	/*----- JOURNAL SELECTION QUALIFIERS -----*/
	/*
	 *	-GLOBAL=(list of global names)
	 */
	if (cli_present("GLOBAL") == CLI_PRESENT)
	{
		if (!cli_get_value("GLOBAL", qual_buffer))
			mupip_exit(ERR_MUPCLIERR);
		if ((top = strlen(qual_buffer)) == 0)
		{
			util_out_print("-GLOBAL requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		c = qual_buffer;
		if (*c == '(')
			++c;
		if (qual_buffer[top - 1] == ')')
			qual_buffer[--top] = '\0';
		for (c1 = c, ctop = &qual_buffer[top];  c < ctop;  c1 = ++c)
		{
			sl_ptr1 = (select_list *)malloc(sizeof(select_list));
			sl_ptr1->next = NULL;
			if (mur_options.global == NULL)
				mur_options.global = sl_ptr1;
			else
				sl_ptr->next = sl_ptr1;
			sl_ptr = sl_ptr1;
			while (*c != '\0'  &&  *c != ',')
				++c;
			if (*c1 == '~')
			{
				++c1;
				sl_ptr->exclude = TRUE;
			} else
				sl_ptr->exclude = FALSE;
			sl_ptr->len = c - c1;
			sl_ptr->buff = (char *)malloc(sl_ptr->len);
			*c = '\0';
			strcpy(sl_ptr->buff, c1);
		}
		mur_options.selection = TRUE;
	}
	/*
	 *	-USER=(list of user names)
	 */
	if (cli_present("USER") == CLI_PRESENT)
	{
		if (!cli_get_value("USER", qual_buffer))
			mupip_exit(ERR_MUPCLIERR);
		if ((top = strlen(qual_buffer)) == 0)
		{
			util_out_print("-USER requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		for (c1 = c = qual_buffer, ctop = &qual_buffer[top];  c < ctop;  c1 = ++c)
		{
			sl_ptr1 = (select_list *)malloc(sizeof(select_list));
			sl_ptr1->next = NULL;
			if (mur_options.user == NULL)
				mur_options.user = sl_ptr1;
			else
				sl_ptr->next = sl_ptr1;
			sl_ptr = sl_ptr1;
			while (*c != '\0'  &&  *c != ',')
				++c;
			if (*c1 == '~')
			{
				++c1;
				sl_ptr->exclude = TRUE;
			} else
				sl_ptr->exclude = FALSE;
			sl_ptr->len = c - c1;
			sl_ptr->buff = (char *)malloc(sl_ptr->len);
			*c = '\0';
			strcpy(sl_ptr->buff, c1);
		}
		mur_options.selection = TRUE;
	}
	/*
	 *	-ID=(list of user process id's)
	 */
	if (cli_present("ID") == CLI_PRESENT)
	{
		if (!cli_get_value("ID", qual_buffer))
			mupip_exit(ERR_MUPCLIERR);
		if ((top = strlen(qual_buffer)) == 0)
		{
			util_out_print("-ID requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		for (c1 = c = qual_buffer, ctop = &qual_buffer[top];  c < ctop;  c1 = ++c)
		{
			ll_ptr1 = (long_list *)malloc(sizeof(long_list));
			ll_ptr1->next = NULL;
			if (mur_options.id == NULL)
				mur_options.id = ll_ptr1;
			else
				ll_ptr->next = ll_ptr1;
			ll_ptr = ll_ptr1;
			while (*c != '\0'  &&  *c != ',')
				++c;
			if (*c1 == '~')
			{
				++c1;
				ll_ptr->exclude = TRUE;
			} else
				ll_ptr->exclude = FALSE;
			if ((length = c - c1) > 8  ||  (ll_ptr->num = asc2i((uchar_ptr_t)c1, length)) == -1)
			{
				util_out_print("Invalid -ID qualifier value: !AD", TRUE, length, c1);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
		mur_options.selection = TRUE;
	}
	/*
	 *	-TRANSACTION=[~]SET|KILL
	 */
	if (cli_present("TRANSACTION") == CLI_PRESENT)
	{
		if (!cli_get_value("TRANSACTION", qual_buffer))
			mupip_exit(ERR_MUPCLIERR);
		if ((length = strlen(qual_buffer)) == 0)
		{
			util_out_print("-TRANSACTION requires a value", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		c1 = qual_buffer;
		lower_to_upper((uchar_ptr_t)c1, (uchar_ptr_t)c1, length);
		if (*c1 == '~')
		{
			++c1;
			--length;
			exclude = TRUE;
		} else
			exclude = FALSE;
		if (memcmp(c1, "KILL", MIN(length, 4)) == 0)
			mur_options.transaction = exclude ? TRANS_SETS : TRANS_KILLS;
		else if (memcmp(c1, "SET", MIN(length, 3)) == 0)
			mur_options.transaction = exclude ? TRANS_KILLS : TRANS_SETS;
		else
		{
			util_out_print("Invalid -TRANSACTION qualifier value;  specify SET or KILL", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.selection = TRUE;
	}
	if (mur_options.forward)
		mur_options.apply_after_image = FALSE;
	else
		mur_options.apply_after_image = TRUE;
	/* DSE has after image records.
	   Following undocmented option will allow users to force the appropriate action,
	   instead of default behavior */
	if ((status = cli_present("AFTER_IMAGE_APPLY")) == CLI_PRESENT)
	{
		length = sizeof(bool_buff);
		if (cli_get_str("AFTER_IMAGE_APPLY", bool_buff, &length))
		{
			if (0 == strcmp(bool_buff, "TRUE"))
				mur_options.apply_after_image = TRUE;
			else if (0 == strcmp(bool_buff, "FALSE"))
				mur_options.apply_after_image = FALSE;
		}
	}
}
