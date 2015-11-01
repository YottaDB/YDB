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

#include <time.h>
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "util.h"

GBLREF	mur_opt_struct	mur_options;



/* A utility routine to compute the length of a string, exclusive of trailing blanks or nuls
   (NOTE:  this routine is also called from mur_report_error() and the mur_extract_*() routines) */

int real_len (length, str)
int	length;
char	*str;
{
	int	q;

	for (q = length - 1;  q >= 0  &&  (str[q] == ' '  ||  str[q] == '\0');  --q)
		;

	return q + 1;
}


/* Convert a time value to a string in the format "MM/DD HH:MM" */

static	int	format_time(time, string)
int4	*time;
char	*string;
{
	struct tm	*ts = localtime((const time_t *)time);

	SPRINTF(string, "%2d/%2d %2d:%02d", ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min);

	return strlen(string);
}


/* These are used by prep_jpv(), below */

static	int	login_time_len, node_len, user_len, term_len;
static	char	login[LENGTH_OF_TIME + 1];

static	void	prep_jpv(pv)
jnl_process_vector	*pv;
{
	login_time_len = format_time(&pv->jpv_login_time, login);

	user_len = real_len(JPV_LEN_USER,	pv->jpv_user);
	node_len = real_len(JPV_LEN_NODE,	pv->jpv_node);
	term_len = real_len(JPV_LEN_TERMINAL,	pv->jpv_terminal);
}


/* Headings and FAO specs, etc. */

static	const	char	proc_header_1[] = "Host            User            PID    Terminal",
			proc_header_2[] = "Host            User            PID    Terminal        Initialization  Termination",
			proc_header_3[] = "Host            User            PID    Terminal        Initialization",
			dashes_fao[] = "!#*-",
			proc_fao_1[] = "!15AD !15AD !5ZL  !15AD",
			proc_fao_2[] = "!15AD !15AD !5ZL  !15AD !11AD     !11AD",
			proc_fao_3[] = "!15AD !15AD !5ZL  !15AD !11AD",
			statistics_header[] = "Record type    Count",
			statistics_fao[] = "   !5AZ    !7UL";


/* A table based on the journal record types */

static	char	* const label[] =
{
#define JNL_TABLE_ENTRY(A,B,C,D)	C,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};



void	mur_output_show(ctl)
ctl_list	*ctl;
{
	jnl_file_header *header;
	show_list_type	*slp;
	int		i, logout_time_len;
	char		logout[LENGTH_OF_TIME + 1], create[LENGTH_OF_TIME + 1];
	bool		heading_printed;
	unsigned char	*ptr, qwstring[100];
	unsigned char	*ptr1, qwstring1[100];


	if (mur_options.show & SHOW_HEADER)
	{
		header = mur_get_file_header(ctl->rab);

		util_out_print("!^Journal file name:!_!AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
		util_out_print("Journal file label:!_!AD", TRUE, sizeof(JNL_LABEL_TEXT) - 1, header->label);
		util_out_print("!^Database file name:!_!AD", TRUE, header->data_file_name_length, header->data_file_name);
		util_out_print("Prev jnl file name:!_!AD", TRUE, header->prev_jnl_file_name_length, header->prev_jnl_file_name);
		util_out_print("!^Replication State:!_!AD", TRUE, 6, (header->repl_state == repl_closed ? "CLOSED" : "OPEN  "));
		util_out_print("Align size:!_!_!SL", TRUE, header->alignsize);
		util_out_print("Epoch Interval:!_!_!SL", TRUE, header->epoch_interval);
		ptr = i2ascl(qwstring, header->start_seqno);
		ptr1 = i2asclx(qwstring1, header->start_seqno);
		util_out_print("Start RegSeqNo:!_!_!AD [0x!AD]", TRUE, ptr-qwstring, qwstring, ptr1 - qwstring1, qwstring1);

		if (header->before_images)
			util_out_print("Before-image journal:!_ENABLED", TRUE);
		else
			util_out_print("Before-image journal:!_DISABLED", TRUE);

		prep_jpv(&header->who_created);

		util_out_print("!/Journal file created !AD by:", TRUE,
				format_time(&header->who_created.jpv_time, create), create);
		util_out_print(proc_header_1, TRUE);
		util_out_print(dashes_fao, TRUE, sizeof proc_header_1 - 1);
		util_out_print(proc_fao_1, TRUE,
				node_len, header->who_created.jpv_node,
				user_len, header->who_created.jpv_user,
				header->who_created.jpv_pid,
				term_len, header->who_created.jpv_terminal);

		prep_jpv(&header->who_opened);

		util_out_print("!/Journal file last opened !AD by:", TRUE,
				format_time(&header->who_opened.jpv_time, create), create);
		util_out_print(proc_header_1, TRUE);
		util_out_print(dashes_fao, TRUE, sizeof proc_header_1 - 1);
		util_out_print(proc_fao_1, TRUE,
				node_len, header->who_opened.jpv_node,
				user_len, header->who_opened.jpv_user,
				header->who_opened.jpv_pid,
				term_len, header->who_opened.jpv_terminal);
	}

	if (mur_options.show & SHOW_ALL_PROCESSES)
	{
		heading_printed = FALSE;

		for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
			if (SOME_TIME(slp->jpv.jpv_time))
			{
				if (!heading_printed)
				{
					util_out_print("!/Processes completed in ", FALSE);
					if (mur_options.show & SHOW_HEADER)
						util_out_print("this journal file:", TRUE);
					else
						util_out_print("journal file !AD:", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
					util_out_print(proc_header_2, TRUE);
					util_out_print(dashes_fao, TRUE, sizeof proc_header_2 - 1);
					heading_printed = TRUE;
				}

				if (slp->jpv.jpv_login_time == 0)
					slp->jpv.jpv_login_time = slp->jpv.jpv_time;

				prep_jpv(&slp->jpv);

				util_out_print(proc_fao_2, TRUE,
						node_len, slp->jpv.jpv_node,
						user_len, slp->jpv.jpv_user,
						slp->jpv.jpv_pid,
						term_len, slp->jpv.jpv_terminal,
						login_time_len, login,
						format_time(&slp->jpv.jpv_time, logout), logout);
			}
	}

	if (mur_options.show & SHOW_ACTIVE_PROCESSES)
	{
		heading_printed = FALSE;

		for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
			if (!SOME_TIME(slp->jpv.jpv_time))
			{
				if (!heading_printed)
				{
					util_out_print("!/Processes NOT completed in ", FALSE);
					if (mur_options.show & SHOW_HEADER)
						util_out_print("this journal file:", TRUE);
					else
						util_out_print("journal file !AD:", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
					util_out_print(proc_header_3, TRUE);
					util_out_print(dashes_fao, TRUE, sizeof proc_header_3 - 1);
					heading_printed = TRUE;
				}

				prep_jpv(&slp->jpv);

				util_out_print(proc_fao_3, TRUE,
						node_len, slp->jpv.jpv_node,
						user_len, slp->jpv.jpv_user,
						slp->jpv.jpv_pid,
						term_len, slp->jpv.jpv_terminal,
						login_time_len, login);
			}
	}

	if (mur_options.show & SHOW_BROKEN)
	{
		heading_printed = FALSE;

		for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
			if (slp->broken)
			{
				if (!heading_printed)
				{
					util_out_print("!/Processes with broken transactions in ", FALSE);
					if (mur_options.show & SHOW_HEADER)
						util_out_print("this journal file:", TRUE);
					else
						util_out_print("journal file !AD:", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
					util_out_print(proc_header_3, TRUE);
					util_out_print(dashes_fao, TRUE, sizeof proc_header_3 - 1);
					heading_printed = TRUE;
				}

				prep_jpv(&slp->jpv);

				util_out_print(proc_fao_3, TRUE,
						node_len, slp->jpv.jpv_node,
						user_len, slp->jpv.jpv_user,
						slp->jpv.jpv_pid,
						term_len, slp->jpv.jpv_terminal,
						login_time_len, login);
			}

		heading_printed = FALSE;

		for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
			if (slp->broken  &&  slp->recovered)
			{
				if (!heading_printed)
				{
					util_out_print("!/Processes with broken transactions not excluded from recovery:", TRUE);
					util_out_print(proc_header_3, TRUE);
					util_out_print(dashes_fao, TRUE, sizeof proc_header_3 - 1);
					heading_printed = TRUE;
				}

				prep_jpv(&slp->jpv);

				util_out_print(proc_fao_3, TRUE,
						node_len, slp->jpv.jpv_node,
						user_len, slp->jpv.jpv_user,
						slp->jpv.jpv_pid,
						term_len, slp->jpv.jpv_terminal,
						login_time_len, login);
			}

		heading_printed = FALSE;

		for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
			if (mur_multi_missing(slp->jpv.jpv_pid))
			{
				if (!heading_printed)
				{
					util_out_print("!/Processes with transactions missing regions:", TRUE);
					util_out_print(proc_header_2, TRUE);
					util_out_print(dashes_fao, TRUE, sizeof proc_header_2 - 1);
					heading_printed = TRUE;
				}

				if (SOME_TIME(slp->jpv.jpv_time))
					logout_time_len = format_time(&slp->jpv.jpv_time, logout);
				else
					logout_time_len = 0;

				prep_jpv(&slp->jpv);

				util_out_print(proc_fao_2, TRUE,
						node_len, slp->jpv.jpv_node,
						user_len, slp->jpv.jpv_user,
						slp->jpv.jpv_pid,
						term_len, slp->jpv.jpv_terminal,
						login_time_len, login,
						logout_time_len, logout);
			}
	}

	if (mur_options.show & SHOW_STATISTICS)
	{
		util_out_print("!/Summary of records processed in journal file !AD:", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
		util_out_print(statistics_header, TRUE);
		util_out_print(dashes_fao, TRUE, sizeof statistics_header - 1);

		for (i = JRT_BAD;  i < JRT_RECTYPES;  ++i)
			util_out_print(statistics_fao, TRUE, label[i], ctl->jnlrec_cnt[i]);
	}

}
