/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#ifdef VMS
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <jpidef.h>
#include <fab.h>
#include <rab.h>
#include <nam.h>
#include <rmsdef.h>
#endif
#include "gtm_unistd.h"
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

#ifdef VMS
static	const	$DESCRIPTOR(output_qualifier,		"OUTPUT");
static	const	$DESCRIPTOR(process_qualifier,		"PROCESS");
#define EXCLUDE_CHAR	'-'
#define STR2PID		asc_hex2i
#define	MAX_PID_LEN	8	/* maximum number of hexadecimal digits in the process-id */
#define REDIRECT_STR		"specify as (old-file-name=new-file-name,...)"
#else
#define EXCLUDE_CHAR	'~'
#define STR2PID asc2i
#define	MAX_PID_LEN	10	/* maximum number of decimal digits in the process-id */
#define REDIRECT_STR		"specify as \"old-file-name=new-file-name,...\""
#endif
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
	int		extr_type, top, onln_rlbk_val;
	boolean_t	global_exclude;
	long_list	*ll_ptr, *ll_ptr1;
	redirect_list	*rl_ptr, *rl_ptr1, *tmp_rl_ptr;
	select_list	*sl_ptr, *sl_ptr1;
	boolean_t	interactive, parse_error;
#	ifdef VMS
	int4		item_code, mode;
	jnl_proc_time	max_time;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef VMS
	DEBUG_ONLY(
		JNL_WHOLE_TIME(max_time);
		/* The following assert is to make sure we get some fix for the toggling of max_time's bit 55 (bit 0 lsb, 63 msb).
		 * Bit 55 is currently 1 (it was 0 until some time in 1973) and will remain so until another 2087. Bit 55 toggles
		 * approximately every 41700 days (114 years). We need to fix the way mupip recover operates to take care of such
		 * transition periods. And the fix needs to be done before it is too late. Hence this assert. This assert will
		 * fail in year 2084, approximately 980 days before the toggle time (in year 2087).
		 */
		assert(JNL_FULL_HI_TIME(max_time) < JNL_HITIME_WARN_THRESHOLD);
	)
#	endif
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
	UNIX_ONLY(assert(FALSE == jgbl.onlnrlbk);)
	if (mur_options.rollback)
	{
		mur_options.update = TRUE;
		UNIX_ONLY(
			onln_rlbk_val = cli_present("ONLINE");
			jgbl.onlnrlbk = onln_rlbk_val ? (onln_rlbk_val != CLI_NEGATED) : FALSE; /* Default is -NOONLINE */
		)
	}
	TREF(skip_file_corrupt_check) = mupip_jnl_recover = mur_options.update;
	jgbl.mur_rollback = mur_options.rollback;	/* needed to set jfh->repl_state properly for newly created jnl files */
	UNIX_ONLY(murgbl.resync_strm_index = INVALID_SUPPL_STRM;)
	if (CLI_PRESENT == cli_present("RESYNC"))
	{
		status = cli_get_uint64("RESYNC", (gtm_uint64_t *)&murgbl.resync_seqno);
		if (!status || (0 == murgbl.resync_seqno))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_NOTPOSITIVE, 2, LEN_AND_LIT("RESYNC"));
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.resync_specified = TRUE;
		UNIX_ONLY(
			if (CLI_PRESENT == cli_present("RSYNC_STRM"))
			{
				status = cli_get_int("RSYNC_STRM", &murgbl.resync_strm_index);
				if (!status)
					mupip_exit(ERR_MUPCLIERR);
				if ((0 > murgbl.resync_strm_index) || (MAX_SUPPL_STRMS <= murgbl.resync_strm_index))
				{
					gtm_putmsg(VARLSTCNT(1) ERR_RSYNCSTRMVAL);
					mupip_exit(ERR_MUPCLIERR);
				}
			}
		)
	}
	if ((status = cli_present("FETCHRESYNC")) == CLI_PRESENT)
	{
		if (!cli_get_int("FETCHRESYNC", &mur_options.fetchresync_port))
			mupip_exit(ERR_MUPCLIERR);
	}
	/*-----		-[NO]VERIFY	-----*/
	mur_options.verify = cli_present("VERIFY") != CLI_NEGATED;

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
		if (CLI_PRESENT == cli_present(extr_parms[extr_type]))
		{
			mur_options.extr[extr_type] = TRUE;
			length = MAX_LINE;
			if (cli_get_str(extr_parms[extr_type], qual_buffer, &length))
			{
				mur_options.extr_fn_len[extr_type] = length;
				mur_options.extr_fn[extr_type] = (char *)malloc(mur_options.extr_fn_len[extr_type] + 1);
				strncpy(mur_options.extr_fn[extr_type], qual_buffer, length);
				mur_options.extr_fn[extr_type][length]='\0';
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
			gtm_putmsg(VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("AFTER"));
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
			gtm_putmsg(VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("SINCE"));
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
			gtm_putmsg(VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("BEFORE"));
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
				gtm_putmsg(VARLSTCNT(4) ERR_INVQUALTIME, 2, LEN_AND_LIT("TIME"));
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
		file_name_expanded = (char *)malloc(MAX_FN_LEN + 1);
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
				gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT(REDIRECT_STR));
				mupip_exit(ERR_MUPCLIERR);
			}
#endif
			entry_ptr = cptr = entry;
			while (cptr < (entry_ptr + length) &&  *cptr != ','  &&  *cptr != '=')
				++cptr;
			if ('=' != *cptr)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT(REDIRECT_STR));
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
			memcpy(file_name_specified, entry, file_name_specified_len);
			*(file_name_specified + file_name_specified_len)= '\0';
			if (!get_full_path(file_name_specified, file_name_specified_len, file_name_expanded,
				&file_name_expanded_len, MAX_FN_LEN, &ustatus))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT("Unable to find full pathname"));
				mupip_exit(ERR_MUPCLIERR);
			}
			for (tmp_rl_ptr = mur_options.redirect; tmp_rl_ptr != NULL; tmp_rl_ptr = tmp_rl_ptr->next)
			{
				if (((tmp_rl_ptr->org_name_len == file_name_expanded_len) &&
				     (0 == memcmp(tmp_rl_ptr->org_name, file_name_expanded, tmp_rl_ptr->org_name_len))) ||
				    ((tmp_rl_ptr->new_name_len == file_name_expanded_len) &&
				     (0 == memcmp(tmp_rl_ptr->new_name, file_name_expanded, tmp_rl_ptr->new_name_len))))
				{
					gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
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
			memcpy(file_name_specified, entry_ptr, file_name_specified_len);
			*(file_name_specified + file_name_specified_len)= '\0';
			if (!get_full_path(file_name_specified, file_name_specified_len, file_name_expanded,
				&file_name_expanded_len, MAX_FN_LEN, &ustatus))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2, LEN_AND_LIT("Unable to find full pathname"));
				mupip_exit(ERR_MUPCLIERR);
			}
			for (tmp_rl_ptr = mur_options.redirect; tmp_rl_ptr != NULL; tmp_rl_ptr = tmp_rl_ptr->next)
			{
				if ((tmp_rl_ptr->org_name_len == file_name_expanded_len &&
				    0 == memcmp(tmp_rl_ptr->org_name, file_name_expanded, tmp_rl_ptr->org_name_len)) ||
				   (tmp_rl_ptr->new_name_len == file_name_expanded_len &&
				    0 == memcmp(tmp_rl_ptr->new_name, file_name_expanded, tmp_rl_ptr->new_name_len)))
				{
					gtm_putmsg(VARLSTCNT(4) ERR_INVREDIRQUAL, 2,
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
	mur_options.fences = FENCE_PROCESS;
	if (cli_present("FENCES") == CLI_PRESENT)
	{
		if (CLI_PRESENT == cli_present("FENCES.NONE"))
			mur_options.fences = FENCE_NONE;
		else if (CLI_PRESENT == cli_present("FENCES.ALWAYS"))
			mur_options.fences = FENCE_ALWAYS;
		else if (CLI_PRESENT == cli_present("FENCES.PROCESS"))
			mur_options.fences = FENCE_PROCESS;	/* DEFAULT */
	}
	DEBUG_ONLY(jgbl.mur_fences_none = (FENCE_NONE == mur_options.fences);)
	/*-----		-[NO]INTERACTIVE	-----*/
#ifdef VMS
	item_code = JPI$_MODE;
	lib$getjpi(&item_code, NULL, NULL, &mode, NULL, NULL);
	interactive = (JPI$K_INTERACTIVE == mode);
#else
	interactive = (boolean_t) isatty(0);
#endif
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
			gtm_putmsg(VARLSTCNT(2) ERR_INVERRORLIM, 0);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else if (status == CLI_NEGATED)
		mur_options.error_limit = 1000000;
#ifdef VMS
	/*-----		-OUTPUT (VMS ONLY) 	-----*/
	if ( CLI_PRESENT == cli_present("OUTPUT"))
		util_out_open(&output_qualifier);
#endif
	/*-----		-[NO]CHECKTN 		-----*/
	mur_options.notncheck = (cli_present("CHECKTN") == CLI_NEGATED);
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
					gtm_putmsg(VARLSTCNT(5) ERR_INVGLOBALQUAL, 3, qual_buffer_ptr - &qual_buffer[0],
								ctop - &qual_buffer[0], qual_buffer);
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
			if (!cli_get_str_ele(qual_buffer_ptr, entry, &length, UNIX_ONLY(FALSE) VMS_ONLY(TRUE)))
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
			if (!cli_get_str_ele(qual_buffer_ptr, entry, &length, UNIX_ONLY(FALSE) VMS_ONLY(TRUE)))
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
				gtm_putmsg(VARLSTCNT(4) ERR_INVIDQUAL, 2, length, entry_ptr);
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
			gtm_putmsg(VARLSTCNT(2) ERR_INVTRNSQUAL, 0);
			mupip_exit(ERR_MUPCLIERR);
		}
		mur_options.selection = TRUE;
	}
#ifdef VMS
	/*----- 	/PROCESS=(list of user process names) 	-----*/
	if (cli_present("PROCESS") == CLI_PRESENT)
	{	/* this is VMS only */
		length = MAX_LINE;
		if (!CLI_GET_STR_ALL("PROCESS", qual_buffer, &length))
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
			assert((',' == *qual_buffer_ptr) || !(*qual_buffer_ptr)); /* either comma separator or end of option list */
			if (',' == *qual_buffer_ptr)
				qual_buffer_ptr++;  /* skip separator */
			entry_ptr = entry;
			sl_ptr1 = (select_list *)malloc(SIZEOF(select_list));
			sl_ptr1->next = NULL;
			if (mur_options.process == NULL)
				mur_options.process = sl_ptr1;
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
				--length;
				sl_ptr->exclude = TRUE;
			} else
				sl_ptr->exclude = FALSE;
			if (global_exclude)
				sl_ptr->exclude = !sl_ptr->exclude;
			sl_ptr->len = length;
			sl_ptr->buff = (char *)malloc(length);
			memcpy(sl_ptr->buff, entry_ptr, length);
			sl_ptr->has_wildcard = FALSE;
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR1, length)) ? FALSE : TRUE);
			sl_ptr->has_wildcard += ((NULL == memchr(sl_ptr->buff, WILDCARD_CHAR2, length)) ? FALSE : TRUE);
		}
		mur_options.selection = TRUE;
	}
#endif
	/* by default after_images are applied during backward recovery. the APPLY_AFTER_IMAGE option can override that behaviour */
	mur_options.apply_after_image = !mur_options.forward;
	if (CLI_PRESENT == (status = cli_present("APPLY_AFTER_IMAGE")))
		mur_options.apply_after_image = TRUE;
	else if (CLI_NEGATED == status)
		mur_options.apply_after_image = FALSE;
	/* if the only request is -SHOW=HEAD, set show_head_only */
	if ((SHOW_HEADER == mur_options.show) && !mur_options.update && !mur_options.verify &&
	    (CLI_PRESENT != cli_present("EXTRACT")))
		mur_options.show_head_only = TRUE;
	free(entry);
	free(qual_buffer);
}
