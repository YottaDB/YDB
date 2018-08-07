/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "cli.h"
#include "util.h"
#include "real_len.h"		/* for real_len() prototype */
#include "have_crit.h"
#include "gtm_multi_proc.h"
#include "interlock.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_gbls_t	murgbl;

LITREF	char		*jrt_label[JRT_RECTYPES];

static	const	char	statistics_fao[] = "   !5AZ !10UL";
static  const   char	statistics_header[] = "!/Record type    Count";
static  const   char 	dashes_fao[] = "!#*-";

#define DOUBLE_ARG(X)	X,X

enum
{
	SHW_BROKEN = 0,	/* defining it as SHW_BROKEN since SHOW_BROKEN is already defined elsewhere */
	SHW_ACTIVE = 1,
	SHW_FINISHED = 2,
	NUM_SHW_TYPES
};

#define	PRINT_SHOW_HEADER(jctl)												\
{															\
	util_out_print("!/-------------------------------------------------------------------------------", TRUE);	\
	util_out_print("SHOW output for journal file !AD", TRUE, jctl->jnl_fn_len, jctl->jnl_fn);			\
	util_out_print("-------------------------------------------------------------------------------", TRUE);	\
}

#define	ZERO_TIME_LITERAL	"                   0"

static	const	char proc_header[] =
	"PID        NODE             USER         TERM JPV_TIME           ";
static	const	char proc_fao[] =
	"!10ZL !16AD !12AD !4AD !19AD";

#define	TIME_DISPLAY_FAO	" !19AD"

/* Convert a time value to a string in the TIME_FORMAT_STRING's format */
int	format_time(jnl_proc_time proc_time, char *string, int string_len, int time_format)
{
	time_t		short_time, seconds;
	struct tm	*tsp;
	uint4		days;
	int		len;

	if (LONG_TIME_FORMAT == time_format)
		short_time = MID_TIME(proc_time);
	else
		short_time = (time_t)proc_time;
	GTM_LOCALTIME(tsp, (const time_t *)&short_time);
	SPRINTF(string, "%04d/%02d/%02d %02d:%02d:%02d", (1900 + tsp->tm_year), (1 + tsp->tm_mon), tsp->tm_mday,
			tsp->tm_hour, tsp->tm_min, tsp->tm_sec);
	assert(LENGTH_OF_TIME >= strlen(string));
	return STRLEN(string);
}

static	void	mur_show_jpv(jnl_process_vector	*pv, boolean_t print_header)
{
	int	jpv_time_len, node_len, user_len, term_len;
	char	jpv_time_str[LENGTH_OF_TIME + 1];

	jpv_time_len = format_time(pv->jpv_time, jpv_time_str, SIZEOF(jpv_time_str), LONG_TIME_FORMAT);
	node_len = real_len(JPV_LEN_NODE,	(uchar_ptr_t)pv->jpv_node);
	user_len = real_len(JPV_LEN_USER,	(uchar_ptr_t)pv->jpv_user);
	term_len = real_len(JPV_LEN_TERMINAL,	(uchar_ptr_t)pv->jpv_terminal);
	if (print_header)
	{
		util_out_print((caddr_t)proc_header, TRUE);
		util_out_print((caddr_t)dashes_fao, TRUE, SIZEOF(proc_header) - 1);
	}
	util_out_print((caddr_t)proc_fao, TRUE, pv->jpv_pid, node_len, pv->jpv_node, user_len, pv->jpv_user,
		term_len, pv->jpv_terminal, jpv_time_len, jpv_time_str);
}

void	mur_show_header(jnl_ctl_list * jctl)
{
	jnl_file_header	*hdr;
	int		time_len, idx;
	char		time_str[LENGTH_OF_TIME + 1];
	char 		outbuf[GTMCRYPT_HASH_HEX_LEN + 1];

	hdr = jctl->jfh;
	util_out_print("!/Journal file name       !AD", TRUE, jctl->jnl_fn_len, jctl->jnl_fn);
	util_out_print("Journal file label      !AD", TRUE, SIZEOF(JNL_LABEL_TEXT) - 1, hdr->label);
	util_out_print("Database file name      !AD", TRUE, hdr->data_file_name_length, hdr->data_file_name);
	util_out_print(" Prev journal file name !AD", TRUE, hdr->prev_jnl_file_name_length, hdr->prev_jnl_file_name);
	util_out_print(" Next journal file name !AD", TRUE, hdr->next_jnl_file_name_length, hdr->next_jnl_file_name);
	if (hdr->before_images)
		util_out_print("!/ Before-image journal                      ENABLED", TRUE);
	else
		util_out_print("!/ Before-image journal                     DISABLED", TRUE);
	util_out_print(" Journal file header size                 !8UL [0x!XL]", TRUE, DOUBLE_ARG(JNL_HDR_LEN));
	util_out_print(" Virtual file size                        !8UL [0x!XL] blocks", TRUE, DOUBLE_ARG(hdr->virtual_size));
	util_out_print(" Journal file checksum seed             !10UL [0x!XL]", TRUE, DOUBLE_ARG(hdr->checksum));
	util_out_print(" Crash                                       !AD", TRUE, 5, (hdr->crash ? " TRUE" : "FALSE"));
	util_out_print(" Recover interrupted                         !AD", TRUE, 5, (hdr->recover_interrupted ? " TRUE" : "FALSE"));
	util_out_print(" Journal file encrypted                      !AD", TRUE, 5,
			(IS_ENCRYPTED(hdr->is_encrypted) ? " TRUE" : "FALSE"));
	util_out_print(" Journal file (re)encryption in progress     !AD", TRUE, 5, (USES_NEW_KEY(hdr) ? " TRUE" : "FALSE"));
	GET_HASH_IN_HEX(hdr->encryption_hash, outbuf, GTMCRYPT_HASH_HEX_LEN);
	util_out_print(" Journal file hash                           !AD", TRUE, GTMCRYPT_HASH_HEX_LEN, outbuf);
	GET_HASH_IN_HEX(hdr->encryption_hash2, outbuf, GTMCRYPT_HASH_HEX_LEN);
	util_out_print(" Journal file hash2                          !AD", TRUE, GTMCRYPT_HASH_HEX_LEN, outbuf);
	util_out_print(" Journal file uses null IV                   !AD", TRUE, 5, (hdr->non_null_iv ? "FALSE" : " TRUE"));
	util_out_print(" Journal file encryption hash cutoff  !12SL", TRUE, hdr->encryption_hash_cutoff);
	util_out_print(" Journal file hash2 start TN  !20@UQ [0x!16@XQ]", TRUE, DOUBLE_ARG(&hdr->encryption_hash2_start_tn));
	util_out_print(" Blocks to Upgrade Adjustment           !10UL [0x!XL]", TRUE,
		DOUBLE_ARG(hdr->prev_recov_blks_to_upgrd_adjust));
	util_out_print(" End of Data                            !10UL [0x!XL]", TRUE, DOUBLE_ARG(hdr->end_of_data));
	util_out_print(" Prev Recovery End of Data              !10UL [0x!XL]", TRUE, DOUBLE_ARG(hdr->prev_recov_end_of_data));
	util_out_print(" Endian Format                              !AD", TRUE, STR_LIT_LEN(ENDIANTHISJUSTIFY), ENDIANTHISJUSTIFY);
	time_len = format_time(hdr->bov_timestamp, time_str, SIZEOF(time_str), SHORT_TIME_FORMAT);
	util_out_print(" Journal Creation Time        "TIME_DISPLAY_FAO, TRUE, time_len, time_str);
	time_len = format_time(hdr->eov_timestamp, time_str, SIZEOF(time_str), SHORT_TIME_FORMAT);
	util_out_print(" Time of last update          "TIME_DISPLAY_FAO, TRUE, time_len, time_str);
	util_out_print(" Begin Transaction            !20@UQ [0x!16@XQ]", TRUE, DOUBLE_ARG(&hdr->bov_tn));
	util_out_print(" End Transaction              !20@UQ [0x!16@XQ]", TRUE, DOUBLE_ARG(&hdr->eov_tn));
	util_out_print(" Align size                       !16UL [0x!XL] bytes", TRUE, DOUBLE_ARG(hdr->alignsize));
	util_out_print(" Epoch Interval                           !8UL", TRUE, EPOCH_SECOND2SECOND(hdr->epoch_interval));
	assert(!REPL_WAS_ENABLED(hdr));
	util_out_print(" Replication State                        !8AD", TRUE, 8,
		(hdr->repl_state == repl_closed ? "  CLOSED" : (hdr->repl_state == repl_open ? "    OPEN" : "WAS_OPEN")));
	util_out_print(" Jnlfile SwitchLimit              !16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(hdr->autoswitchlimit));
	util_out_print(" Jnlfile Allocation               !16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(hdr->jnl_alq));
	util_out_print(" Jnlfile Extension                !16UL [0x!XL] blocks", TRUE, DOUBLE_ARG(hdr->jnl_deq));
	util_out_print(" Maximum Journal Record Length    !16UL [0x!XL]", TRUE, DOUBLE_ARG(hdr->max_jrec_len));
	util_out_print(" Turn Around Point Offset               !10UL [0x!XL]", TRUE, DOUBLE_ARG(hdr->turn_around_offset));
	util_out_print(" Last EOF Written                            !AD", TRUE, 5, (hdr->last_eof_written ? " TRUE" : "FALSE"));
	util_out_print(" Was a Previous Generation Journal File      !AD", TRUE, 5, (hdr->is_not_latest_jnl ? " TRUE" : "FALSE"));
	if (hdr->turn_around_time)
		time_len = format_time(hdr->turn_around_time, time_str, SIZEOF(time_str), SHORT_TIME_FORMAT);
	else
	{
		time_len = STR_LIT_LEN(ZERO_TIME_LITERAL);
		MEMCPY_LIT(time_str, ZERO_TIME_LITERAL);
	}
	util_out_print(" Turn Around Point Time       !20AD", TRUE, time_len, time_str);
	util_out_print(" Start Region Sequence Number !20@UQ [0x!16@XQ]", TRUE, &hdr->start_seqno, &hdr->start_seqno);
	util_out_print(" End Region Sequence Number   !20@UQ [0x!16@XQ]", TRUE, &hdr->end_seqno, &hdr->end_seqno);
	/* Dump stream seqnos for upto 16 streams if any are non-zero */
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
	{	/* Dump stream seqnos. Dont dump them unconditionally as they will swamp the output.
		 * We usually expect 1 or 2 streams to have non-zero values so dump it only if non-zero.
		 * Note that in case the journal file is created for the first time as part of a supplementary instance,
		 * the stream seqno would be 0 so the start_seqno in the jnl file header could be 0 whereas the end seqno
		 * could be non-zero. In that case, use the end-seqno to determine if the stream seqno needs to be
		 * dumped or not. In pro be safe and use both values and dump both if one of them is non-zero.
		 */
		assert(!hdr->strm_start_seqno[idx] || hdr->strm_end_seqno[idx]);
		if (hdr->strm_start_seqno[idx] || hdr->strm_end_seqno[idx])
		{
			util_out_print(" Stream !2UL : Start RegSeqno   !20@UQ [0x!16@XQ]", TRUE,
				idx, &hdr->strm_start_seqno[idx], &hdr->strm_start_seqno[idx]);
			util_out_print(" Stream !2UL : End   RegSeqno   !20@UQ [0x!16@XQ]", TRUE,
				idx, &hdr->strm_end_seqno[idx], &hdr->strm_end_seqno[idx]);
		}
	}
	util_out_print("!/Process That Created the Journal File:!/", TRUE);
	mur_show_jpv(&hdr->who_created, TRUE);
	util_out_print("!/Process That First Opened the Journal File:!/", TRUE);
	mur_show_jpv(&hdr->who_opened, TRUE);
	util_out_print("", TRUE);
}

void	mur_output_show()
{
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		*jctl;
	int			rectype, size;
	pini_list_struct	*plst;
	ht_ent_int4 		*tabent, *topent;
	boolean_t		first_time = TRUE, ok_to_show[NUM_SHW_TYPES], release_latch;
	int			index;
	jnl_proc_time		tmp_jpv_time;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */
	const int		show_state[NUM_SHW_TYPES] = { BROKEN_PROC, ACTIVE_PROC, FINISHED_PROC };

	assert(mur_options.show);
	assert(3 == NUM_SHW_TYPES);
	assert(0 == SHW_BROKEN);
	assert(1 == SHW_ACTIVE);
	assert(2 == SHW_FINISHED);
	assert(NUM_SHW_TYPES == ARRAYSIZE(ok_to_show));
	assert(NUM_SHW_TYPES == ARRAYSIZE(show_state));
	if (multi_proc_in_use)
	{
		assert(NULL == multi_proc_key);
		assert(!multi_proc_key_exception);
		DEBUG_ONLY(multi_proc_key_exception = TRUE);	/* "multi_proc_in_use" is still TRUE so set this to avoid asserts */
		GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch); /* Grab a latch for the entire show duration */
		assert(release_latch);
	}
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total;  rctl < rctl_top;  rctl++)
	{
		if (multi_proc_in_use)
		{
			if (!rctl->this_pid_is_owner)
				continue;	/* in a parallel processing environment, process only regions we own */
		}
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
			size = jctl->pini_list.size;
			ok_to_show[SHW_FINISHED] = (mur_options.show & SHOW_ALL_PROCESSES) ? TRUE : FALSE;
			ok_to_show[SHW_ACTIVE] = ok_to_show[SHW_FINISHED] || (mur_options.show & SHOW_ACTIVE_PROCESSES);
			ok_to_show[SHW_BROKEN] = ok_to_show[SHW_ACTIVE] || (mur_options.show & SHOW_BROKEN);
			for (index = 0; index < NUM_SHW_TYPES; index++)
			{
				if (ok_to_show[index])
				{
					assert((SHW_BROKEN == index) || (CLI_PRESENT == cli_present("SHOW")));
					first_time = TRUE;
					for (tabent = jctl->pini_list.base, topent = jctl->pini_list.top; tabent < topent; tabent++)
					{
						if (HTENT_VALID_INT4(tabent, pini_list_struct, plst))
						{
							if (show_state[index] == plst->state)
							{
								if (first_time)
								{
									switch(index)
									{
									case SHW_BROKEN:
										/* print show-header in case SHOW=BROKEN was not
										 * explicitly specified but implicitly assumed
										 * due to mur_options.update
										 */
										if (CLI_PRESENT != cli_present("SHOW"))
											PRINT_SHOW_HEADER(jctl);
										util_out_print("!/Process(es) with BROKEN "
											"transactions in this journal:!/", TRUE);
										break;
									case SHW_ACTIVE:
										util_out_print("!/Process(es) that are still "
											"ACTIVE in this journal:!/", TRUE);
										break;
									case SHW_FINISHED:
										util_out_print("!/Process(es) that are COMPLETE "
											"in this journal:!/", TRUE);
										break;
									}
								}
								/* Temporarily fix plst->jpv->jpv_time to be plst->pini_jpv_time so
								 * gets displayed accurately. It is possible for the two to differ
								 * in case backward recovery/rollback virtually truncated the
								 * pre-recovery journal file and played its journal records in a new
								 * journal file with a newer PINI record. We want the pre-recovery
								 * time to be displayed hence this temporary adjustment.
								 */
								assert((plst->jpv.jpv_time == plst->pini_jpv_time)
									|| (!mur_options.forward && mur_options.update));
								tmp_jpv_time = plst->jpv.jpv_time;
								plst->jpv.jpv_time = plst->pini_jpv_time;
								mur_show_jpv(&plst->jpv, first_time);
								/* By the time "mur_output_show" is called, forward phase of
								 * recovery is complete so no need to actually restore jpv_time
								 * but just in case it is needed in the future, restore it.
								 */
								plst->jpv.jpv_time = tmp_jpv_time;
								first_time = FALSE;
							}
						}
					}
				}
			}
			if (mur_options.show & SHOW_STATISTICS)
			{
				assert(CLI_PRESENT == cli_present("SHOW"));
				util_out_print((caddr_t)statistics_header, TRUE);
				util_out_print((caddr_t)dashes_fao, TRUE, STR_LIT_LEN(statistics_header));
				for (rectype = JRT_BAD;  rectype < JRT_RECTYPES;  ++rectype)
				{
					if ((JRT_TRIPLE == rectype) || (JRT_HISTREC == rectype))
					{
						assert(0 == jctl->jnlrec_cnt[rectype]);
						continue;
					}
					util_out_print((caddr_t)statistics_fao, TRUE,
						jrt_label[rectype], jctl->jnlrec_cnt[rectype]);
				}
			}
			jctl = jctl->next_gen;
			if ((CLI_PRESENT == cli_present("SHOW")) || !first_time)
				util_out_print("", TRUE);
		}
	}
	if (multi_proc_in_use)
	{
		REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		DEBUG_ONLY(multi_proc_key_exception = FALSE);
	}
}
