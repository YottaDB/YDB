/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if defined(VMS)
#include <climsgdef.h>
#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */
#endif

#include "gtm_string.h"		/* for memcpy() */

#include "gdsroot.h"
#include "gtm_stat.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "cli.h"
#include "util.h"
#include "gtmmsg.h"

error_def(ERR_BFRQUALREQ);
error_def(ERR_FILEPARSE);
error_def(ERR_JNLALIGNSZCHG);
error_def(ERR_JNLINVALLOC);
error_def(ERR_JNLINVEXT);
error_def(ERR_JNLINVSWITCHLMT);
error_def(ERR_JNLMINALIGN);


#define get_cli_val(cli_val, positive_opt, negate_opt)				\
{										\
	cli_status1 = cli_present(positive_opt);				\
	cli_status2 = cli_present(negate_opt);					\
	assert(!(CLI_PRESENT == cli_status1 && CLI_PRESENT == cli_status2));	\
	assert(!(CLI_NEGATED == cli_status1 && CLI_NEGATED == cli_status2));	\
	if (CLI_PRESENT == cli_status1)						\
		cli_val = CLI_PRESENT;						\
	else if (CLI_PRESENT == cli_status2)   					\
		cli_val = CLI_NEGATED;						\
	else									\
		cli_val = CLI_ABSENT;						\
}

boolean_t mupip_set_journal_parse(set_jnl_options *jnl_options, jnl_create_info *jnl_info)
{
	uint4		status, bits_of_alignsize = 0;
	unsigned short	temp_jnl_fn_len;
	int4		alignsize;
	int		cli_status1, cli_status2;

	/* Parsing for Journal/Replication state change */
	cli_status1 = cli_present("JOURNAL");
	jnl_options->cli_journal = (CLI_PRESENT != cli_status1 && CLI_NEGATED != cli_status1) ? CLI_ABSENT: cli_status1;
	get_cli_val(jnl_options->cli_enable, "JOURNAL.ENABLE", "JOURNAL.DISABLE");
	get_cli_val(jnl_options->cli_on, "JOURNAL.ON", "JOURNAL.OFF");
	get_cli_val(jnl_options->cli_replic_on, "REPLICATION.ON", "REPLICATION.OFF");
	if (CLI_PRESENT == (cli_status1 = cli_present("JOURNAL.BEFORE_IMAGES")))
	{
		jnl_options->image_type_specified = TRUE;
		jnl_info->before_images = TRUE;
	} else if (CLI_NEGATED == cli_status1)
	{
		VMS_ONLY(assert(CLI_PRESENT != jnl_options->cli_replic_on);)
		jnl_options->image_type_specified = TRUE;
		jnl_info->before_images = FALSE;
	} else
	{
		jnl_options->image_type_specified = FALSE;
		assert(!(CLI_PRESENT == jnl_options->cli_on ||
			((CLI_PRESENT == jnl_options->cli_enable) && (CLI_ABSENT == jnl_options->cli_on))));
	}
	assert(JNL_ALLOC_MIN > DIVIDE_ROUND_UP(JNL_FILE_FIRST_RECORD, DISK_BLOCK_SIZE));
	/* Parsing for other journal characteristics given in "JOURNAL" option */
	if (jnl_options->allocation_specified = (CLI_PRESENT == cli_present("JOURNAL.ALLOCATION")))
	{
		if (!cli_get_int("JOURNAL.ALLOCATION", &jnl_info->alloc))
			return FALSE;
		if ((jnl_info->alloc < JNL_ALLOC_MIN) || (jnl_info->alloc > JNL_ALLOC_MAX))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_JNLINVALLOC, 3, jnl_info->alloc, JNL_ALLOC_MIN, JNL_ALLOC_MAX);
			return FALSE;
		}
	}
	if (jnl_options->alignsize_specified = (CLI_PRESENT == cli_present("ALIGNSIZE")))
	{
		if (!cli_get_int("ALIGNSIZE", &alignsize))
			return FALSE;
		if (alignsize < JNL_MIN_ALIGNSIZE)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_JNLMINALIGN, 2, alignsize, JNL_MIN_ALIGNSIZE);
			return FALSE;
		}
		if (alignsize > JNL_MAX_ALIGNSIZE)
		{
			util_out_print("ALIGNSIZE cannot be greater than !UL", TRUE, JNL_MAX_ALIGNSIZE);
			return FALSE;
		}
		LOG2_OF_INTEGER(alignsize, bits_of_alignsize);
		if ((1 << bits_of_alignsize) != alignsize)
		{
			alignsize = 1 << bits_of_alignsize; /* This will make alignsize power of two */
			gtm_putmsg(VARLSTCNT(3) ERR_JNLALIGNSZCHG, 1, alignsize);
		}
		jnl_info->alignsize = alignsize * DISK_BLOCK_SIZE;
	}
	if (jnl_options->autoswitchlimit_specified = (CLI_PRESENT == cli_present("AUTOSWITCHLIMIT")))
	{
		if (!cli_get_int("AUTOSWITCHLIMIT", &jnl_info->autoswitchlimit))
			return FALSE;
		if (JNL_AUTOSWITCHLIMIT_MIN > jnl_info->autoswitchlimit
			|| JNL_ALLOC_MAX < jnl_info->autoswitchlimit)
		{
			gtm_putmsg(VARLSTCNT(5) ERR_JNLINVSWITCHLMT, 3, jnl_info->autoswitchlimit,
								JNL_AUTOSWITCHLIMIT_MIN, JNL_ALLOC_MAX);
			return FALSE;
		}
	}
	if (jnl_options->buffer_size_specified = (CLI_PRESENT == cli_present("BUFFER_SIZE")))
	{
		if (!cli_get_int("BUFFER_SIZE", &jnl_info->buffer))
			return FALSE;
		if (jnl_info->buffer <= 0)
			return FALSE;
	}
	if (jnl_options->epoch_interval_specified = (CLI_PRESENT == cli_present("EPOCH_INTERVAL")))
	{
		if (!cli_get_int("EPOCH_INTERVAL", &jnl_info->epoch_interval))
			return FALSE;
		if (jnl_info->epoch_interval <= 0)
		{
			util_out_print("EPOCH_INTERVAL cannot be ZERO (or negative)", TRUE);
			return FALSE;
		}
		if (jnl_info->epoch_interval > MAX_EPOCH_INTERVAL)
		{
			util_out_print("EPOCH_INTERVAL cannot be greater than !UL", TRUE, MAX_EPOCH_INTERVAL);
			return FALSE;
		}
		jnl_info->epoch_interval = SECOND2EPOCH_SECOND(jnl_info->epoch_interval);
	}
	if (jnl_options->extension_specified = (CLI_PRESENT == cli_present("JOURNAL.EXTENSION")))
	{
		if (!cli_get_int("JOURNAL.EXTENSION", &jnl_info->extend))
			return FALSE;
		if (jnl_info->extend < 0)
		{
			util_out_print("EXTENSION_COUNT cannot be negative", TRUE);
			return FALSE;
		}
		if (jnl_info->extend > JNL_EXTEND_MAX)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_JNLINVEXT, 2, jnl_info->extend, JNL_EXTEND_MAX);
			return FALSE;
		}
	}
	temp_jnl_fn_len = jnl_info->jnl_len = MAX_FN_LEN + 1;
	if (jnl_options->filename_specified = cli_get_str("FILENAME", (char *)jnl_info->jnl, &temp_jnl_fn_len))
		jnl_info->jnl_len = temp_jnl_fn_len;
	else
		jnl_info->jnl_len = 0;
	if ((CLI_PRESENT == (cli_status1 = cli_present(UNIX_ONLY("SYNC_IO") VMS_ONLY("CACHE")))) || (CLI_NEGATED ==  cli_status1))
	{
		jnl_options->sync_io_specified = TRUE;
		jnl_options->sync_io = (UNIX_ONLY(CLI_PRESENT) VMS_ONLY(CLI_NEGATED) == cli_status1) ? TRUE: FALSE;
	}
	else
		jnl_options->sync_io_specified = FALSE;
	UNIX_ONLY(
	if (jnl_options->yield_limit_specified = (CLI_PRESENT == cli_present("YIELD_LIMIT")))
	{
		if (!cli_get_int("YIELD_LIMIT", &jnl_options->yield_limit))
			return FALSE;
		if (jnl_options->yield_limit < MIN_YIELD_LIMIT)
		{
			util_out_print("YIELD_LIMIT cannot be less than !UL", TRUE, MIN_YIELD_LIMIT);
			return FALSE;
		}
		if (jnl_options->yield_limit > MAX_YIELD_LIMIT)
		{
			util_out_print("YIELD_LIMIT cannot be greater than !UL", TRUE, MAX_YIELD_LIMIT);
			return FALSE;
		}
	}
	)
	return TRUE;
}
