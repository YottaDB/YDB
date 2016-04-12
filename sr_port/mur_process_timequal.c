/****************************************************************
 *
 * Copyright (c) 2005-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_time.h"
#ifdef VMS
#include <math.h>	/* for mur_rel2abstime() function */
#include <descrip.h>
#endif
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
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "cli.h"
#include "mupip_exit.h"
#include "have_crit.h"

GBLREF	mur_opt_struct	mur_options;

error_def (ERR_JNLTMQUAL1);
error_def (ERR_JNLTMQUAL2);
error_def (ERR_JNLTMQUAL3);
error_def (ERR_JNLTMQUAL4);
error_def (ERR_MUNOACTION);

void mur_process_timequal(jnl_tm_t max_lvrec_time, jnl_tm_t min_bov_time)
{
	char				time_str1[LENGTH_OF_TIME + 1], time_str2[LENGTH_OF_TIME + 1];

	/* All time qualifiers are specified in delta time or absolute time.
	 *	mur_options.since_time == 0 means no since time was specified
	 *	mur_options.since_time < 0 means it is the delta since time.
	 *	mur_options.since_time > 0 means it is absolute since time in seconds
	 * 	mur_options.before_time and mur_options.lookback_time also have same meanings for values == 0, < 0 and > 0.
	 */
	if (mur_options.since_time <= 0)
		REL2ABSTIME(mur_options.since_time, max_lvrec_time, FALSE); /* make it absolute time */
	if (!mur_options.before_time_specified)
		mur_options.before_time = MAXUINT4;
	else if (mur_options.before_time <= 0)
		REL2ABSTIME(mur_options.before_time, max_lvrec_time, TRUE); /* make it absolute time */
	if ((CLI_PRESENT == cli_present("AFTER")) && (mur_options.after_time <= 0))
		REL2ABSTIME(mur_options.after_time, max_lvrec_time, FALSE); /* make it absolute time */
	if (mur_options.lookback_time <= 0)
		REL2ABSTIME(mur_options.lookback_time, mur_options.since_time, FALSE); /* make it absolute time */
	if (!mur_options.forward)
	{
		if (mur_options.before_time < mur_options.since_time)
		{
			if (mur_options.since_time_specified)
			{	/* Both -BEFORE and -SINCE were specified explicitly but out of order. Issue error */
				GET_TIME_STR(mur_options.before_time, time_str1);
				GET_TIME_STR(mur_options.since_time, time_str2);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLTMQUAL1, 2, time_str1, time_str2);
				mupip_exit(ERR_MUNOACTION);
			} else
			{	/* -BEFORE was specified but -SINCE was not specified.
				 * Set -SINCE to be equal to -BEFORE and continue processing.
				 */
				mur_options.since_time = mur_options.before_time;
			}
		}
		if (mur_options.lookback_time > mur_options.since_time)
		{
			GET_TIME_STR(mur_options.lookback_time, time_str1);
			GET_TIME_STR(mur_options.since_time, time_str2);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLTMQUAL2, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
	} else
	{
		if (mur_options.before_time < min_bov_time)
		{
			GET_TIME_STR(mur_options.before_time, time_str1);
			GET_TIME_STR(min_bov_time, time_str2);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLTMQUAL3, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
		if (mur_options.before_time < mur_options.after_time)
		{
			GET_TIME_STR(mur_options.before_time, time_str1);
			GET_TIME_STR(mur_options.after_time, time_str2);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLTMQUAL4, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
	}
}
