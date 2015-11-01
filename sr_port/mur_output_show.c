/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_time.h"

#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashdef.h"
#include "hashtab.h"
#include "muprec.h"
#include "cli.h"
#include "util.h"
#ifdef VMS
#include <descrip.h>
#include <jpidef.h>
#endif

GBLREF	mur_opt_struct	mur_options;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF 	mur_gbls_t	murgbl;

LITREF	char		*jrt_label[JRT_RECTYPES];

static	const	char	statistics_fao[] = "   !5AZ    !7UL";
static  const   char	statistics_header[] = "!/Record type    Count";
static  const   char 	dashes_fao[] = "!#*-";

#define	LONG_TIME_FORMAT	0
#define	SHORT_TIME_FORMAT	1

#define DOUBLE_ARG(X)	X,X

#define	PRINT_SHOW_HEADER(jctl)												\
{															\
	util_out_print("!/-------------------------------------------------------------------------------", TRUE);	\
	util_out_print("SHOW output for journal file !AD", TRUE, jctl->jnl_fn_len, jctl->jnl_fn);			\
	util_out_print("-------------------------------------------------------------------------------", TRUE);	\
}

#define	ZERO_TIME_LITERAL	"                   0"

#if defined(VMS)
/* Headings and FAO specs, etc. */
static  const   char proc_header[] =
        "PID      NODE     USER         TERM     JPV_TIME             PNAME           IMGCNT   MODE  LOGIN_TIME          ";
static	const	char proc_fao[] =
        "!8XL !8AD !12AD !8AD !20AD !15AD !8XL !5AD !20AD";

/* Convert a time value to a string in the TIME_FORMAT_STRING's format.  this routine currently does not handle $h printing */
static int	format_time(jnl_proc_time proc_time, char *string, int string_len, int time_format)
{
	jnl_proc_time	long_time;

	if (SHORT_TIME_FORMAT == time_format)
		JNL_WHOLE_FROM_SHORT_TIME(long_time, proc_time);
	else
		long_time = proc_time;
	GET_LONG_TIME_STR(long_time, string, string_len);
	assert(LENGTH_OF_TIME >= strlen(string));
	return strlen(string);
}

static	void	mur_show_jpv(jnl_process_vector *pv, boolean_t print_header)
{
	int	jpv_time_len, node_len, user_len, term_len, proc_len, login_time_len;
	char	*mode_str, login_time_str[LENGTH_OF_TIME + 1], jpv_time_str[LENGTH_OF_TIME + 1];

	jpv_time_len = format_time(pv->jpv_time, jpv_time_str, sizeof(jpv_time_str), LONG_TIME_FORMAT);
	login_time_len = format_time(pv->jpv_login_time, login_time_str, sizeof(login_time_str), LONG_TIME_FORMAT);
	node_len = real_len(JPV_LEN_NODE,	pv->jpv_node);
	user_len = real_len(JPV_LEN_USER,	pv->jpv_user);
	proc_len = real_len(JPV_LEN_PRCNAM,	pv->jpv_prcnam);
	term_len = real_len(JPV_LEN_TERMINAL,	pv->jpv_terminal);
	switch (pv->jpv_mode)
	{
		case JPI$K_DETACHED:	mode_str = "Detch";	break;
		case JPI$K_NETWORK:	mode_str = "Netwk";	break;
		case JPI$K_BATCH:	mode_str = "Batch";	break;
		case JPI$K_LOCAL:	mode_str = "Local";	break;
		case JPI$K_DIALUP:	mode_str = "Dialu";	break;
		case JPI$K_REMOTE:	mode_str = "Remot";	break;
		default:		mode_str = "UNKWN";
	}
	if (print_header)
	{
		util_out_print(proc_header, TRUE);
		util_out_print(dashes_fao, TRUE, sizeof(proc_header) - 1);
	}
	util_out_print(proc_fao, TRUE, pv->jpv_pid, node_len, pv->jpv_node, user_len, pv->jpv_user,
		term_len, pv->jpv_terminal, jpv_time_len, jpv_time_str,
		proc_len, pv->jpv_prcnam, pv->jpv_image_count, 5, mode_str, login_time_len, login_time_str);
}

#elif defined(UNIX)

static	const	char proc_header[] =
	"PID        NODE         USER     TERM JPV_TIME           ";
static	const	char proc_fao[] =
	"!10ZL !12AD !8AD !4AD !38AD";

/* Convert a time value to a string in the TIME_FORMAT_STRING's format */
static int	format_time(jnl_proc_time proc_time, char *string, int string_len, int time_format)
{
	time_t		short_time, seconds;
	struct tm	*tsp;
	uint4		days;
	int		len;

	if (LONG_TIME_FORMAT == time_format)
		short_time = MID_TIME(proc_time);
	else
		short_time = (time_t)proc_time;
	tsp = localtime((const time_t *)&short_time);
	SPRINTF(string, "%04d/%02d/%02d %02d:%02d:%02d", (1900 + tsp->tm_year), (1 + tsp->tm_mon), tsp->tm_mday,
			tsp->tm_hour, tsp->tm_min, tsp->tm_sec);
	assert(LENGTH_OF_TIME >= strlen(string));
	return strlen(string);
}

static	void	mur_show_jpv(jnl_process_vector	*pv, boolean_t print_header)
{
	int	jpv_time_len, node_len, user_len, term_len;
	char	jpv_time_str[LENGTH_OF_TIME + 1];

	jpv_time_len = format_time(pv->jpv_time, jpv_time_str, sizeof(jpv_time_str), LONG_TIME_FORMAT);
	node_len = real_len(JPV_LEN_NODE,	pv->jpv_node);
	user_len = real_len(JPV_LEN_USER,	pv->jpv_user);
	term_len = real_len(JPV_LEN_TERMINAL,	pv->jpv_terminal);
	if (print_header)
	{
		util_out_print(proc_header, TRUE);
		util_out_print(dashes_fao, TRUE, sizeof(proc_header) - 1);
	}
	util_out_print(proc_fao, TRUE, pv->jpv_pid, node_len, pv->jpv_node, user_len, pv->jpv_user,
		term_len, pv->jpv_terminal, jpv_time_len, jpv_time_str);
}

#endif

void	mur_show_header(jnl_ctl_list * jctl)
{
	jnl_file_header	*header;
	int		regno;
	int		time_len;
	char		time_str[LENGTH_OF_TIME + 1];

	header = jctl->jfh;
	util_out_print("!/Journal file name!_!AD", TRUE, jctl->jnl_fn_len, jctl->jnl_fn);
	util_out_print("Journal file label!_!AD", TRUE, sizeof(JNL_LABEL_TEXT) - 1, header->label);
	util_out_print("Database file name!_!AD", TRUE, header->data_file_name_length, header->data_file_name);
	util_out_print(" Prev journal file name!_!AD", TRUE, header->prev_jnl_file_name_length, header->prev_jnl_file_name);
	util_out_print(" Next journal file name!_!AD", TRUE, header->next_jnl_file_name_length, header->next_jnl_file_name);
	if (header->before_images)
		util_out_print("!/ Before-image journal!_!_!_ ENABLED", TRUE);
	else
		util_out_print("!/ Before-image journal!_!_!_DISABLED", TRUE);
	util_out_print(" Journal file header size!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(JNL_HDR_LEN));
	util_out_print(" Virtual file size!_!_!_!8UL [0x!XL] blocks", TRUE, DOUBLE_ARG(header->virtual_size));
	util_out_print(" Crash    !_!_!_!_   !AD", TRUE, 5, (header->crash ? " TRUE" : "FALSE"));
	util_out_print(" Recover interrupted!_!_!_   !AD", TRUE, 5, (header->recover_interrupted ? " TRUE" : "FALSE"));
	util_out_print(" End of Data!_!_!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(header->end_of_data));
	util_out_print(" Prev Recovery End of Data!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(header->prev_recov_end_of_data));
	time_len = format_time(header->bov_timestamp, time_str, sizeof(time_str), SHORT_TIME_FORMAT);
	util_out_print(" Journal Creation Time      !20AD", TRUE, time_len, time_str);
	time_len = format_time(header->eov_timestamp, time_str, sizeof(time_str), SHORT_TIME_FORMAT);
	util_out_print(" Time of last update        !20AD", TRUE, time_len, time_str);
	util_out_print(" Begin Transaction!_!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(header->bov_tn));
	util_out_print(" End Transaction!_!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(header->eov_tn));
	util_out_print(" Align size!_!_!_!16UL [0x!XL] bytes", TRUE, DOUBLE_ARG(header->alignsize));
	util_out_print(" Epoch Interval!_!_!_!_!8UL", TRUE, EPOCH_SECOND2SECOND(header->epoch_interval));
	util_out_print(" Replication State!_!_!_  !AD", TRUE, 6, (header->repl_state == repl_closed ? "CLOSED" : "  OPEN"));
	util_out_print(" Updates Disabled on Secondary!_!_   !AD", TRUE, 5, (header->update_disabled ? " TRUE" : "FALSE"));
	util_out_print(" Jnlfile SwitchLimit!_!_!16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(header->autoswitchlimit));
	util_out_print(" Jnlfile Allocation!_!_!16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(header->jnl_alq));
	util_out_print(" Jnlfile Extension!_!_!16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(header->jnl_deq));
	util_out_print(" Maximum Physical Record Length!_!16UL [0x!XL]", TRUE, DOUBLE_ARG(header->max_phys_reclen));
	util_out_print(" Maximum Logical Record Length!_!16UL [0x!XL]", TRUE, DOUBLE_ARG(header->max_logi_reclen));
	util_out_print(" Turn Around Point Offset!_!_!8UL [0x!XL]", TRUE, DOUBLE_ARG(header->turn_around_offset));
	if (header->turn_around_time)
		time_len = format_time(header->turn_around_time, time_str, sizeof(time_str), SHORT_TIME_FORMAT);
	else
	{
		time_len = STR_LIT_LEN(ZERO_TIME_LITERAL);
		MEMCPY_LIT(time_str, ZERO_TIME_LITERAL);
	}
	util_out_print(" Turn Around Point Time     !20AD", TRUE, time_len, time_str);
	util_out_print(" Start Region Sequence Number !_!16@ZJ [0x!16@XJ]", TRUE, &header->start_seqno, &header->start_seqno);
	util_out_print(" End Region Sequence Number!_!16@ZJ [0x!16@XJ]", TRUE, &header->end_seqno, &header->end_seqno);

	util_out_print("!/Process That Created the Journal File:!/", TRUE);
	mur_show_jpv(&header->who_created, TRUE);
	util_out_print("!/Process That Last Wrote to the Journal File:!/", TRUE);
	mur_show_jpv(&header->who_opened, TRUE);
	util_out_print("", TRUE);
}

void	mur_output_show()
{
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		*jctl;
	int			cnt, regno, rectype, size;
	pini_list_struct	*plst;
	hashtab_ent 		*h_ent;
	boolean_t		first_time;

	assert(mur_options.show);
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total;  rctl < rctl_top;  rctl++)
	{
		jctl = (NULL == rctl->jctl_turn_around) ? rctl->jctl_head : rctl->jctl_turn_around;
		while (jctl)
		{
			if (CLI_PRESENT == cli_present("SHOW"))
				PRINT_SHOW_HEADER(jctl);	/* print show-header unconditionally only if SHOW was specified */
			if (mur_options.show & SHOW_HEADER)
			{
				assert(CLI_PRESENT == cli_present("SHOW"));
				mur_show_header(jctl);
			}
			size = jctl->pini_list->size;
			if (mur_options.show & SHOW_BROKEN
				|| mur_options.show & SHOW_ACTIVE_PROCESSES
				|| mur_options.show & SHOW_ALL_PROCESSES)
			{
				first_time = TRUE;
				h_ent = jctl->pini_list->tbl;
				for (cnt = 0; cnt < size; cnt++, h_ent++)
				{
					if (h_ent->v)
					{
						plst = (pini_list_struct *)h_ent->v;
						if (BROKEN_PROC == plst->state)
						{
							if (first_time)
							{	/* print show-header in case SHOW=BROKEN was not explicitly
								 * specified but implicitly assumed due to mur_options.update
								 */
								if (CLI_PRESENT != cli_present("SHOW"))
									PRINT_SHOW_HEADER(jctl);
								util_out_print("!/Process(es) with BROKEN transactions in this "
										"journal:!/", TRUE);
							}
							mur_show_jpv(&plst->jpv, first_time);
							first_time = FALSE;
						}
					}
				}
			}
			if (mur_options.show & SHOW_ACTIVE_PROCESSES
				|| mur_options.show & SHOW_ALL_PROCESSES)
			{
				assert(CLI_PRESENT == cli_present("SHOW"));
				first_time = TRUE;
				h_ent = jctl->pini_list->tbl;
				for (cnt = 0; cnt < size; cnt++, h_ent++)
				{
					if (h_ent->v)
					{
						plst = (pini_list_struct *)h_ent->v;
						if (ACTIVE_PROC == plst->state)
						{
							if (first_time)
								util_out_print("!/Process(es) that are still ACTIVE in this "
										"journal:!/", TRUE);
							mur_show_jpv(&plst->jpv, first_time);
							first_time = FALSE;
						}
					}
				}
			}
			if (mur_options.show & SHOW_ALL_PROCESSES)
			{
				assert(CLI_PRESENT == cli_present("SHOW"));
				first_time = TRUE;
				h_ent = jctl->pini_list->tbl;
				for (cnt = 0; cnt < size; cnt++, h_ent++)
				{
					if (h_ent->v)
					{
						plst = (pini_list_struct *)h_ent->v;
						if (FINISHED_PROC == plst->state)
						{
							if (first_time)
								util_out_print("!/Process(es) that are COMPLETE in this journal:!/",
										TRUE);
							mur_show_jpv(&plst->jpv, first_time);
							first_time = FALSE;
						}
					}
				}
			}
			if (mur_options.show & SHOW_STATISTICS)
			{
				assert(CLI_PRESENT == cli_present("SHOW"));
				util_out_print(statistics_header, TRUE);
				util_out_print(dashes_fao, TRUE, sizeof statistics_header - 1);
				for (rectype = JRT_BAD;  rectype < JRT_RECTYPES;  ++rectype)
					util_out_print(statistics_fao, TRUE, jrt_label[rectype], jctl->jnlrec_cnt[rectype]);
			}
			jctl = jctl->next_gen;
			if ((CLI_PRESENT == cli_present("SHOW")) || !first_time)
				util_out_print("", TRUE);
		}
	}
}
