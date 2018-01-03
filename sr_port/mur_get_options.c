/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
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
#include "gtm_strings.h"

#include "gtm_multi_thread.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_string.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "cli.h"
#include "min_max.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"
#include "gtm_bintim.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF 	mur_gbls_t	murgbl;
GBLREF 	jnl_gbls_t	jgbl;
GBLREF	boolean_t	mupip_jnl_recover;

error_def(ERR_INVERRORLIM);
error_def(ERR_INVGLOBALQUAL);
error_def(ERR_INVIDQUAL);
error_def(ERR_INVQUALTIME);
error_def(ERR_INVREDIRQUAL);
error_def(ERR_INVTRNSQUAL);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOTPOSITIVE);
error_def(ERR_RSYNCSTRMVAL);

#define EXCLUDE_CHAR	'~'
#define STR2PID asc2i
#define	MAX_PID_LEN	10	/* maximum number of decimal digits in the process-id */
#define REDIRECT_STR		"specify as \"old-file-name=new-file-name,...\""
#define	WILDCARD_CHAR1	'*'
#define	WILDCARD_CHAR2	'%'

static	char	default_since_time[] = "0 0:0:00",
		default_lookback_time[] = "0 0:5:00";
static char	* const extr_parms[] =
{
	/* Must match the order of  enum broken_type */
	"EXTRACT",
	"BROKENTRANS",
	"LOSTTRANS"
};

void	mur_get_options(void)
{
	int4		status;
	uint4		ustatus, state;
	unsigned short	length;
	char		*cptr, *ctop, *qual_buffer, inchar;
	char		*qual_buffer_ptr, *entry, *entry_ptr;
	char		*file_name_specified, *file_name_expanded;
	unsigned int 	file_name_specified_len, file_name_expanded_len;
	int		extr_type, top, onln_rlbk_val, status2;
	boolean_t	global_exclude;
	long_list	*ll_ptr, *ll_ptr1;
	redirect_list	*rl_ptr, *rl_ptr1, *tmp_rl_ptr;
	select_list	*sl_ptr, *sl_ptr1;
	boolean_t	interactive, parse_error;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	qual_buffer = (char *)malloc(MAX_LINE);
	entry = (char *)malloc(MAX_LINE);
	memset(&mur_options, 0, SIZEOF(mur_options));
	/*----- 	-VERBOSE	-----*/
	if (CLI_PRESENT == cli_present("VERBOSE"))
		mur_options.verbose = TRUE;
	/*----- JOURNAL ACTION QUALIFIERS -----*/
	/*----- 	-RECOVER 	-----*/
	mur_options.update = cli_present("RECOVER") == CLI_PRESENT;
	/*----- 	-ROLLBACK	-----*/
	mur_options.rollback = cli_present("ROLLBACK") == CLI_PRESENT;
	assert(FALSE == jgbl.onlnrlbk);
	if (mur_options.rollback)
	{
		mur_options.update = TRUE;
		onln_rlbk_val = cli_present("ONLINE");
		jgbl.onlnrlbk = onln_rlbk_val ? (onln_rlbk_val != CLI_NEGATED) : FALSE; /* Default is -NOONLINE */
	}
	TREF(skip_file_corrupt_check) = mupip_jnl_recover = mur_options.update;
	jgbl.mur_rollback = mur_options.rollback;	/* needed to set jfh->repl_state properly for newly created jnl files */
	murgbl.resync_strm_index = INVALID_SUPPL_STRM;
	if (CLI_PRESENT == cli_present("RESYNC"))
	{
		status = cli_get_uint64("RESYNC", (gtm_uint64_t *)&murgbl.resync_seqno);
		if (!status || (0 == murgbl.resync_seqno))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTPOSITIVE, 2, LEN_AND_LIT("RESYNC"));
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.resync_specified = TRUE;
		if (CLI_PRESENT == cli_present("RSYNC_STRM"))
		{
			assert(CLI_PRESENT != cli_present("FORWARD"));
			status = cli_get_int("RSYNC_STRM", &murgbl.resync_strm_index);
			if (!status)
				mupip_exit(ERR_MUPCLIERR);
			if ((0 > murgbl.resync_strm_index) || (MAX_SUPPL_STRMS <= murgbl.resync_strm_index))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RSYNCSTRMVAL);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
	}
	if ((status = cli_present("FETCHRESYNC")) == CLI_PRESENT)
	{
		if (!cli_get_int("FETCHRESYNC", &mur_options.fetchresync_port))
			mupip_exit(ERR_MUPCLIERR);
	}
	/* SHOW[=(ALL|HEADER|PROCESSES|ACTIVE_PROCESSES|BROKEN_TRANSACTIONS|STATISTICS)] */
	mur_options.show = SHOW_NONE;
	if (CLI_PRESENT == cli_present("SHOW"))
	{
		if (CLI_PRESENT == cli_present("SHOW.ACTIVE_PROCESSES"))
			mur_options.show |= SHOW_ACTIVE_PROCESSES;
		if (CLI_PRESENT == cli_present("SHOW.ALL"))
			mur_options.show |= SHOW_ALL;
		if (CLI_PRESENT == cli_present("SHOW.BROKEN_TRANSACTIONS"))
			mur_options.show |= SHOW_BROKEN;
		if (CLI_PRESENT == cli_present("SHOW.HEADER"))
			mur_options.show |= SHOW_HEADER;
		if (CLI_PRESENT == cli_present("SHOW.PROCESSES"))
			mur_options.show |= SHOW_ALL_PROCESSES;
		if (CLI_PRESENT == cli_present("SHOW.STATISTICS"))
			mur_options.show |= SHOW_STATISTICS;
		UNIX_ONLY(assert(SHOW_NONE != mur_options.show);)
		if (SHOW_NONE == mur_options.show)
			mur_options.show = SHOW_ALL;	/* VMS CLI does not seem to recognize SHOW.ALL as default for some reason */
	} else if (mur_options.update)
		mur_options.show = SHOW_BROKEN;
	mur_options.detail = cli_present("DETAIL") == CLI_PRESENT;
	if (CLI_PRESENT == cli_present("FULL"))
		mur_options.extract_full = TRUE;
	/*-----		-EXTRACT[=file-name] or -LOSTTRANS=<file-name> or -BROKENTRANS=<file_name>	-----*/
	for (extr_type = 0; extr_type < TOT_EXTR_TYPES; extr_type++)
	{
		mur_options.extr[extr_type] = FALSE;
		mur_options.extr_fn_is_stdout[extr_type] = FALSE;
		if (CLI_PRESENT == cli_present(extr_parms[extr_type]))
		{
			mur_options.extr[extr_type] = TRUE;
			length = MAX_LINE;
			if (cli_get_str(extr_parms[extr_type], qual_buffer, &length))
			{
				mur_options.extr_fn_len[extr_type] = length;
				mur_options.extr_fn[extr_type] = (char *)malloc(mur_options.extr_fn_len[extr_type] + 1);
				strncpy(mur_options.extr_fn[extr_type], qual_buffer, length);
				mur_options.extr_fn[extr_type][length] = '\0';
				mur_options.extr_fn_is_stdout[extr_type] =
					(0 == STRNCASECMP(qual_buffer, JNL_STDO_EXTR, SIZEOF(JNL_STDO_EXTR)));
			}
		}
	}
	/*----- JOURNAL DIRECTION QUALIFIERS -----*/
	/*-----		-FORWARD		-----*/
	mur_options.forward = cli_present("FORWARD") == CLI_PRESENT;
	/*-----		-BACKWARD		-----*/
	assert(mur_options.forward != (cli_present("BACKWARD") == CLI_PRESENT));
	DEBUG_ONLY(jgbl.mur_options_forward = mur_options.forward;)

	/*----- JOURNAL TIME QUALIFIERS -----*/
	/*-----		-AFTER=delta_or_absolute_time	-----*/
	if (cli_present("AFTER") == CLI_PRESENT)
	{
		length = MAX_LINE;
		cli_get_str("AFTER", qual_buffer, &length);
		status = gtm_bintim(qual_buffer, &mur_options.after_time);
		if (status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("AFTER"));
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	/*-----		-SINCE=delta_or_absolute_time	-----*/
	mur_options.since_time_specified = FALSE;
	if (cli_present("SINCE") == CLI_PRESENT)
	{
		length = MAX_LINE;
		cli_get_str("SINCE", qual_buffer, &length);
		status = gtm_bintim(qual_buffer, &mur_options.since_time);
		if (status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("SINCE"));
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.since_time_specified = TRUE;
	} else if (!mur_options.forward)
		(void)gtm_bintim(default_since_time, &mur_options.since_time);
	/*-----		-BEFORE=delta_or_absolute_time	-----*/
	mur_options.before_time_specified = FALSE;
	if (cli_present("BEFORE") == CLI_PRESENT)
	{
		length = MAX_LINE;
		cli_get_str("BEFORE", qual_buffer, &length);
		status = gtm_bintim(qual_buffer, &mur_options.before_time);
		if (status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("BEFORE"));
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.before_time_specified = TRUE;
	}
	/* [NO]LOOKBACK_LIMIT[=("TIME=delta_or_absolute_time","OPERATIONS=integer")] */
	mur_options.lookback_time_specified = FALSE;
	if (CLI_PRESENT == (status = cli_present("LOOKBACK_LIMIT")))
	{
		length = MAX_LINE;
		CLI_GET_STR_ALL("LOOKBACK_LIMIT",qual_buffer, &length);
		if (CLI_PRESENT == cli_present("LOOKBACK_LIMIT.OPERATIONS"))
		{
			mur_options.lookback_opers_specified = TRUE;
			length = MAX_LINE;
			cli_get_str("LOOKBACK_LIMIT.OPERATIONS", qual_buffer, &length);
			mur_options.lookback_opers = asc2i((uchar_ptr_t)qual_buffer, (int4)length);
		}
		if (CLI_PRESENT == cli_present("LOOKBACK_LIMIT.TIME"))
		{
			length = MAX_LINE;
			cli_get_str("LOOKBACK_LIMIT.TIME", qual_buffer, &length);
			status = gtm_bintim(qual_buffer, &mur_options.lookback_time);
			if (status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("TIME"));
				mupip_exit(ERR_MUPCLIERR);
			}
			mur_options.lookback_time_specified = TRUE;
		}
	} else if (status != CLI_NEGATED  &&  !mur_options.forward)
		(void)gtm_bintim(default_lookback_time, &mur_options.lookback_time);
	/*----- JOURNAL CONTROL QUALIFIERS -----*/
	/*----- 	-REDIRECT=(old-file-name=new-file-name,...)	-----*/
	if (cli_present("REDIRECT") == CLI_PRESENT)
	{
		file_name_specified = (char *)malloc(MAX_FN_LEN + 1);
		file_name_expanded = (char *)malloc(GTM_PATH_MAX);
		length = MAX_LINE;
		if (!CLI_GET_STR_ALL("REDIRECT", qual_buffer, &length))
			mupip_exit(ERR_MUPCLIERR);
		qual_buffer_ptr = qual_buffer;
		for (ctop = qual_buffer + length;  qual_buffer_ptr < ctop;)
		{
			if (!cli_get_str_ele(qual_buffer_ptr, entry, &length, FALSE))
				mupip_exit(ERR_MUPCLIERR);
			qual_buffer_ptr += length;
			assert((',' == *qual_buffer_ptr) || !(*qual_buffer_ptr)); /* either comma separator or end of option list */
			if (',' == *qual_buffer_ptr)
				qual_buffer_ptr++;  /* skip separator */
#ifdef UNIX
			/* parantheses are not allowed on UNIX */
			if (('(' == *entry) || (')' == *(qual_buffer_ptr-1)))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT(REDIRECT_STR));
				mupip_exit(ERR_MUPCLIERR);
			}
#endif
			entry_ptr = cptr = entry;
			while (cptr < (entry_ptr + length) &&  *cptr != ','  &&  *cptr != '=')
				++cptr;
			if ('=' != *cptr)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT(REDIRECT_STR));
				mupip_exit(ERR_MUPCLIERR);
			}
			rl_ptr1 = (redirect_list *)malloc(SIZEOF(redirect_list));
			rl_ptr1->next = NULL;
			if (mur_options.redirect == NULL)
				mur_options.redirect = rl_ptr1;
			else
				rl_ptr->next = rl_ptr1;
			rl_ptr = rl_ptr1;
			file_name_specified_len = (unsigned int)(cptr - entry_ptr);
			if (file_name_specified_len > (MAX_FN_LEN + 1))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
 							LEN_AND_LIT("Redirect DB filename too long: greater than 255"));
				mupip_exit(ERR_MUPCLIERR);
			}
			memcpy(file_name_specified, entry, file_name_specified_len);
			*(file_name_specified + file_name_specified_len)= '\0';
			if (!get_full_path(file_name_specified, file_name_specified_len, file_name_expanded,
				&file_name_expanded_len, GTM_PATH_MAX, &ustatus))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
									LEN_AND_LIT("Unable to find full pathname"));
				mupip_exit(ERR_MUPCLIERR);
			}
			for (tmp_rl_ptr = mur_options.redirect; tmp_rl_ptr != NULL; tmp_rl_ptr = tmp_rl_ptr->next)
			{
				if (((tmp_rl_ptr->org_name_len == file_name_expanded_len) &&
				     (0 == memcmp(tmp_rl_ptr->org_name, file_name_expanded, tmp_rl_ptr->org_name_len))) ||
				    ((tmp_rl_ptr->new_name_len == file_name_expanded_len) &&
				     (0 == memcmp(tmp_rl_ptr->new_name, file_name_expanded, tmp_rl_ptr->new_name_len))))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
						LEN_AND_LIT("Duplicate or invalid specification of files"));
					mupip_exit(ERR_MUPCLIERR);
				}
			}
			rl_ptr->org_name_len = file_name_expanded_len;
			rl_ptr->org_name = (char *)malloc(rl_ptr->org_name_len + 1);
			memcpy(rl_ptr->org_name, file_name_expanded, rl_ptr->org_name_len);
			rl_ptr->org_name[rl_ptr->org_name_len] = '\0';
			entry_ptr = cptr + 1; /* skip the = */
			file_name_specified_len = length - file_name_specified_len - 1; /* the rest of the entry);*/
			if (file_name_specified_len > (MAX_FN_LEN + 1))
                        {
                                  gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
							LEN_AND_LIT("Redirect DB filename too long: greater than 255"));
                                  mupip_exit(ERR_MUPCLIERR);
                        }
			memcpy(file_name_specified, entry_ptr, file_name_specified_len);
			*(file_name_specified + file_name_specified_len)= '\0';
			if (!get_full_path(file_name_specified, file_name_specified_len, file_name_expanded,
				&file_name_expanded_len, MAX_FN_LEN, &ustatus))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
										LEN_AND_LIT("Unable to find full pathname"));
				mupip_exit(ERR_MUPCLIERR);
			}
			for (tmp_rl_ptr = mur_options.redirect; tmp_rl_ptr != NULL; tmp_rl_ptr = tmp_rl_ptr->next)
			{
				if ((tmp_rl_ptr->org_name_len == file_name_expanded_len &&
				    0 == memcmp(tmp_rl_ptr->org_name, file_name_expanded, tmp_rl_ptr->org_name_len)) ||
				   (tmp_rl_ptr->new_name_len == file_name_expanded_len &&
				    0 == memcmp(tmp_rl_ptr->new_name, file_name_expanded, tmp_rl_ptr->new_name_len)))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
						LEN_AND_LIT("Duplicate or invalid specification of files"));
					mupip_exit(ERR_MUPCLIERR);
				}
			}
			rl_ptr->new_name_len = file_name_expanded_len;
			rl_ptr->new_name = (char *)malloc(rl_ptr->new_name_len + 1);
			memcpy(rl_ptr->new_name, file_name_expanded, rl_ptr->new_name_len);
			rl_ptr->new_name[rl_ptr->new_name_len] = '\0';
		}
		free(file_name_specified);
		free(file_name_expanded);
	}
	/*----- 	-FENCES=NONE|ALWAYS|PROCESS 	-----*/
	mur_options.fences = FENCE_PROCESS;	/* DEFAULT */
	if (cli_present("FENCES") == CLI_PRESENT)
	{
		if (CLI_PRESENT == cli_present("FENCES.NONE"))
			mur_options.fences = FENCE_NONE;
		else if (CLI_PRESENT == cli_present("FENCES.ALWAYS"))
			mur_options.fences = FENCE_ALWAYS;
		if (mur_options.rollback && (FENCE_PROCESS != mur_options.fences))
		{
			util_out_print("MUPIP JOURNAL -ROLLBACK only supports -FENCES=PROCESS", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	DEBUG_ONLY(jgbl.mur_fences_none = (FENCE_NONE == mur_options.fences);)
	/*-----		-[NO]INTERACTIVE	-----*/
	interactive = (boolean_t) isatty(0);
	mur_options.interactive = interactive && (CLI_NEGATED != cli_present("INTERACTIVE"));
	/*-----		-[NO]CHAIN 		-----*/
	mur_options.chain = TRUE; /* By Default or specified without negation */
	if ((status = cli_present("CHAIN")) == CLI_NEGATED)
		mur_options.chain = FALSE;
	/*-----		-[NO]ERROR_LIMIT[=integer]	-----*/
	if ((status = cli_present("ERROR_LIMIT")) == CLI_PRESENT)
	{
		if (!cli_get_int("ERROR_LIMIT", &mur_options.error_limit))
			mupip_exit(ERR_MUPCLIERR);
		if (mur_options.error_limit < 0)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_INVERRORLIM, 0);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else if (status == CLI_NEGATED)
		mur_options.error_limit = 1000000;
	/*-----		-[NO]CHECKTN 		-----*/
	mur_options.notncheck = (cli_present("CHECKTN") == CLI_NEGATED);
	if (mur_options.notncheck && mur_options.rollback && mur_options.forward)
	{
		util_out_print("MUPIP JOURNAL -ROLLBACK -FORWARD does not support -NOCHECKTN", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	/*----- JOURNAL SELECTION QUALIFIERS -----*/
	/*-----		-GLOBAL=(list of global names)	-----*/
	if (cli_present("GLOBAL") == CLI_PRESENT)
	{
		length = MAX_LINE;
		if (!CLI_GET_STR_ALL("GLOBAL", qual_buffer, &length))
			mupip_exit(ERR_MUPCLIERR);
		qual_buffer_ptr = qual_buffer;
		global_exclude = FALSE;
		if ('"' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
			if ('"' == qual_buffer_ptr[length-1])
				qual_buffer_ptr[--length] = '\0';
		}
		if (EXCLUDE_CHAR == *qual_buffer_ptr)
		{
			global_exclude = TRUE;
			++qual_buffer_ptr;
			--length;
		}
		if ('(' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
			if (')' == qual_buffer_ptr[length-1])
				qual_buffer_ptr[--length] = '\0';
		} else if (global_exclude)
		{
			--qual_buffer_ptr;
			++length;
			global_exclude = FALSE;
		}
		for (ctop = qual_buffer_ptr + length; qual_buffer_ptr < ctop;)
		{
			entry_ptr = entry;
			/* this is a simplistic state machine that does not allow for more than 30 nestings of '(' and '"'*/
			state = 0x1;
			parse_error = FALSE;
			for (length = 0; (inchar = *qual_buffer_ptr); length++)
			{
				switch (inchar)
				{
					case '(':
						if (state >> 31)
							parse_error = TRUE;
						else
							state = (state << 1) | 1;
						break;
					case ')':
						if ((state > 1) && (state & 1))
							state = (state >> 1);
						else if (1 == state)
							parse_error = TRUE;
						break;
					case '"':
						if ((state > 1) && !(state & 1))
							state = (state >> 1);
						else if (state >> 31)
							parse_error = TRUE;
						else if ('"' == *(qual_buffer_ptr + 1))
							qual_buffer_ptr++;	/* two consecutive double-quote is treated as one */
						else
							state = (state << 1) | 0;
						break;
				}
				if ((',' == inchar) && (1 == state))
				{
					assert(FALSE == parse_error);
					break;
				}
				*entry_ptr++ = *qual_buffer_ptr++;
				if (parse_error || ((qual_buffer_ptr == ctop) && (state > 1)))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_INVGLOBALQUAL, 3,
						qual_buffer_ptr - &qual_buffer[0], ctop - &qual_buffer[0], qual_buffer);
					mupip_exit(ERR_MUPCLIERR);
				}
			}
			*entry_ptr = '\0';
			assert((',' == *qual_buffer_ptr) || !(*qual_buffer_ptr)); /* either comma separator or end of option list */
			if (',' == *qual_buffer_ptr)
				qual_buffer_ptr++;  /* skip separator */
			entry_ptr = entry;
			sl_ptr1 = (select_list *)malloc(SIZEOF(select_list));
			sl_ptr1->next = NULL;
			if (NULL == mur_options.global)
				mur_options.global = sl_ptr1;
			else
				sl_ptr->next = sl_ptr1;
			sl_ptr = sl_ptr1;
			if ('"' == entry_ptr[length - 1])
				--length;
			if ('"' == *entry_ptr)
			{
				entry_ptr++;
				length--;
			}
			if (EXCLUDE_CHAR == *entry_ptr)
			{
				++entry_ptr;
				length--;
				sl_ptr->exclude = TRUE;
			} else
				sl_ptr->exclude = FALSE;
			if (global_exclude)
				sl_ptr->exclude = !sl_ptr->exclude;
			sl_ptr->len = length;
			sl_ptr->buff = (char *)malloc(sl_ptr->len);
			memcpy(sl_ptr->buff, entry_ptr, length);
			sl_ptr->has_wildcard = FALSE;
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR1, length)) ? FALSE : TRUE);
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR2, length)) ? FALSE : TRUE);
		}
		mur_options.selection = TRUE;
	}
	/*----- 	-USER=(list of user names)	-----*/
	if (cli_present("USER") == CLI_PRESENT)
	{
		length = MAX_LINE;
		if (!CLI_GET_STR_ALL("USER", qual_buffer, &length))
			mupip_exit(ERR_MUPCLIERR);
		qual_buffer_ptr = qual_buffer;
		global_exclude = FALSE;
		if ('"' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
			if ('"' == qual_buffer_ptr[length - 1])
				qual_buffer_ptr[--length] = '\0';
		}
		if (EXCLUDE_CHAR == *qual_buffer_ptr)
		{
			global_exclude = TRUE;
			++qual_buffer_ptr;
			--length;
		}
		if ('(' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
		} else if (global_exclude)
		{
			--qual_buffer_ptr;
			++length;
			global_exclude = FALSE;
		}
		if (')' == qual_buffer_ptr[length-1])
			qual_buffer_ptr[--length] = '\0';
		for (ctop = qual_buffer_ptr + length; qual_buffer_ptr < ctop;)
		{
			if (!cli_get_str_ele(qual_buffer_ptr, entry, &length, FALSE))
				mupip_exit(ERR_MUPCLIERR);
			qual_buffer_ptr += length;
			assert(',' == *qual_buffer_ptr || !(*qual_buffer_ptr));	/* either comma separator or end of option list */
			if (',' == *qual_buffer_ptr)
				qual_buffer_ptr++;  /* skip separator */
			entry_ptr = entry;
			sl_ptr1 = (select_list *)malloc(SIZEOF(select_list));
			sl_ptr1->next = NULL;
			if (NULL == mur_options.user)
				mur_options.user = sl_ptr1;
			else
				sl_ptr->next = sl_ptr1;
			sl_ptr = sl_ptr1;
			if ('"' == entry_ptr[length - 1])
				--length;
			if ('"' == *entry_ptr)
			{
				entry_ptr++;
				length--;
			}
			if (EXCLUDE_CHAR == *entry_ptr)
			{
				++entry_ptr;
				length--;
				sl_ptr->exclude = TRUE;
			} else
				sl_ptr->exclude = FALSE;
			if (global_exclude)
				sl_ptr->exclude = !sl_ptr->exclude;
			sl_ptr->len = length;
			sl_ptr->buff = (char *)malloc(sl_ptr->len);
			memcpy(sl_ptr->buff, entry_ptr, length);
			sl_ptr->has_wildcard = FALSE;
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR1, length)) ? FALSE : TRUE);
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR2, length)) ? FALSE : TRUE);
		}
		mur_options.selection = TRUE;
	}
	/*-----		-ID=(list of user process id's)	-----*/
	if (cli_present("ID") == CLI_PRESENT)
	{
		length = MAX_LINE;
		if (!CLI_GET_STR_ALL("ID", qual_buffer, &length))
			mupip_exit(ERR_MUPCLIERR);
		qual_buffer_ptr = qual_buffer;
		global_exclude = FALSE;
		if ('"' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
			if ('"' == qual_buffer_ptr[length-1])
				qual_buffer_ptr[--length] = '\0';
		}
		if (EXCLUDE_CHAR == *qual_buffer_ptr)
		{
			global_exclude = TRUE;
			++qual_buffer_ptr;
			--length;
		}
		if ('(' == *qual_buffer_ptr )
		{
			++qual_buffer_ptr;
			--length;
		} else if (global_exclude)
		{
			--qual_buffer_ptr;
			++length;
			global_exclude = FALSE;
		}
		if (')' == qual_buffer_ptr[length-1])
			qual_buffer_ptr[--length] = '\0';
		for (ctop = qual_buffer_ptr + length; qual_buffer_ptr < ctop;)
		{
			if (!cli_get_str_ele(qual_buffer_ptr, entry, &length, FALSE))
				mupip_exit(ERR_MUPCLIERR);
			qual_buffer_ptr += length;
			assert(',' == *qual_buffer_ptr || !(*qual_buffer_ptr));	/* either comma separator or end of option list */
			if (',' == *qual_buffer_ptr)
				qual_buffer_ptr++;  /* skip separator */
			entry_ptr = entry;
			ll_ptr1 = (long_list *)malloc(SIZEOF(long_list));
			ll_ptr1->next = NULL;
			if (NULL == mur_options.id)
				mur_options.id = ll_ptr1;
			else
				ll_ptr->next = ll_ptr1;
			ll_ptr = ll_ptr1;
			if ('"' == entry_ptr[length - 1])
				--length;
			if ('"' == *entry_ptr)
			{
				entry_ptr++;
				length--;
			}
			if (EXCLUDE_CHAR == *entry_ptr)
			{
				++entry_ptr;
				length--;
				ll_ptr->exclude = TRUE;
			} else
				ll_ptr->exclude = FALSE;
			if (global_exclude)
				ll_ptr->exclude = !ll_ptr->exclude;
			if ((MAX_PID_LEN < length) ||
			    ((ll_ptr->num = STR2PID((uchar_ptr_t)entry_ptr, length)) == (unsigned int) - 1))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVIDQUAL, 2, length, entry_ptr);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
		mur_options.selection = TRUE;
	}
	/*-----		-TRANSACTION=[NO]SET|KILL	-----*/
	if (cli_present("TRANSACTION") == CLI_PRESENT)
	{
		status = cli_present("TRANSACTION.KILL");
		if (CLI_PRESENT == status)
			mur_options.transaction |= TRANS_KILLS;
		else if (CLI_NEGATED == status)
			mur_options.transaction |= TRANS_SETS;
		status = cli_present("TRANSACTION.SET");
		if (CLI_PRESENT == status)
			mur_options.transaction |= TRANS_SETS;
		else if (CLI_NEGATED == status)
			mur_options.transaction |= TRANS_KILLS;
		if ((TRANS_KILLS != mur_options.transaction) && (TRANS_SETS != mur_options.transaction))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_INVTRNSQUAL, 0);
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.selection = TRUE;
	}
	/*-----		-[NO]VERIFY	-----*/
	/* -VERIFY is default except in case RECOVER -FORWARD or ROLLBACK -FORWARD where -NOVERIFY is default.
	 * Also, if -NOCHECKTN is specified, we want -VERIFY to be default so we get the
	 * "Transaction number continuity check failed" message. Additionally, not doing so could cause tp_resolve_time
	 * to be set to a non-zero value (in mur_tp_resolve_time.c) and in turn cause issues in forward processing phase
	 * due to discontinuous timeranges in consecutive jnl file generations like what v54003/C9K08003315 subtest induces.
	 */
	assert(FALSE == mur_options.verify_specified);
	if (CLI_PRESENT == (status = cli_present("VERIFY")))
	{
		mur_options.verify = TRUE;
		mur_options.verify_specified = TRUE;
	}
	else if (CLI_NEGATED == status)
		mur_options.verify = FALSE;
	else
		mur_options.verify = (!mur_options.update || !mur_options.forward || mur_options.notncheck);

	/* by default after_images are applied during backward recovery. the APPLY_AFTER_IMAGE option can override that behaviour */
	mur_options.apply_after_image = !mur_options.forward;
	if (CLI_PRESENT == (status = cli_present("APPLY_AFTER_IMAGE")))
		mur_options.apply_after_image = TRUE;
	else if (CLI_NEGATED == status)
		mur_options.apply_after_image = FALSE;
	/* If the only request is -SHOW=HEAD, set show_head_only. Also reset -VERIFY if previously assumed by default */
	if ((SHOW_HEADER == mur_options.show) && !mur_options.update
			&& !mur_options.verify_specified && (CLI_PRESENT != cli_present("EXTRACT")))
	{
		mur_options.show_head_only = TRUE;
		mur_options.verify = FALSE;
	}
	/*-----		-PARALLEL=MAXTHREADS/MAXPROCS	-----*/
	if (CLI_PRESENT == cli_present("PARALLEL"))
	{
		if (!cli_get_int("PARALLEL", &gtm_mupjnl_parallel))
			gtm_mupjnl_parallel = 0; /* Treat -PARALLEL without any value as full parallelism */
	}
	free(entry);
	free(qual_buffer);
}
