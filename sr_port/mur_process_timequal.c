/****************************************************************
 *
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
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

#ifdef VMS
static	int4	mur_rel2abstime(jnl_proc_time deltatime, jnl_proc_time basetime, boolean_t roundup)
{
	/* In VMS, time in journal records is not stored in units of seconds. Instead it is stored in units of epoch-seconds.
	 * (see vvms/jnlsp.h for comment on epoch-seconds). Since an epoch-second is approximately .8388th of a second, it is
	 * possible that two consecutive journal records with timestamps of say t1 and t1+1 might map to the same time in seconds.
	 * A mapping between epoch-seconds and seconds is given below (using the EPOCH_SECOND2SECOND macro)
	 * 	epoch_second = 91 : second = 77
	 * 	epoch_second = 92 : second = 78
	 * 	epoch_second = 93 : second = 79
	 * 	epoch_second = 94 : second = 79
	 * 	epoch_second = 95 : second = 80
	 * 	epoch_second = 96 : second = 81
	 * 	epoch_second = 97 : second = 82
	 * 	epoch_second = 98 : second = 83
	 * 	epoch_second = 99 : second = 84
	 * say basetime  is  83 seconds (this translates to 98 epoch-seconds)
	 * say deltatime is   4 seconds (this translates to  4/.8388 = 4.76 = 4 (rounded down) epoch-seconds)
	 * Now if we do 98 - 4 we get 94 epoch-seconds which maps to 79 seconds which is indeed 4 seconds below 83 seconds.
	 * But notice that even 93 epoch-seconds maps to 79 seconds.
	 * If this time is to be used as since-time or after-time or lookback-time it is 93 epoch-seconds that needs to be taken
	 * 	(instead of 94) as otherwise we might miss out on few journal records that have 93 epoch-second timestamp).
	 * If this time is to be used as before-time, it is 94 epoch-seconds that needs to be considered (instead of 93) as
	 * 	otherwise we will stop at 93 timestamp journal records and miss out on including 94 timestamp journal records
	 * 	although they correspond to the same second which the user sees in the journal extract.
	 * Therefore, it is necessary that a relative to absolute delta-time conversion routine takes care of this.
	 * It is taken care of in the below function mur_rel2abstime.
	 * "roundup" is TRUE in case this function is called for mur_options.before_time and FALSE otherwise.
	 */
	uint4		baseseconds;
	int4		diffseconds, deltaseconds;

	/* because of the way the final journal extract time comes out in seconds, the EPOCH_SECOND2SECOND macro needs to be
	 * passed one more than the input epoch-seconds in order for us to get the exact corresponding seconds unit. wherever
	 * the macro is used below, subtract one to find out the actual epoch-second that is being considered.
	 */
	deltaseconds = EPOCH_SECOND2SECOND(-deltatime);
	baseseconds = EPOCH_SECOND2SECOND(basetime + 1);
	deltatime += basetime;
	diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);
	if (diffseconds < deltaseconds)
	{
		while ((baseseconds - EPOCH_SECOND2SECOND(deltatime + 1)) < deltaseconds)
			deltatime--;
		DEBUG_ONLY(diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);)
		assert(diffseconds == deltaseconds);
	} else if (diffseconds > deltaseconds)
	{
		while ((baseseconds - EPOCH_SECOND2SECOND(deltatime + 1)) > deltaseconds)
			deltatime++;
		DEBUG_ONLY(diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);)
		assert(diffseconds == deltaseconds);
	}
	if (roundup)
	{
		if (EPOCH_SECOND2SECOND(deltatime + 2) == EPOCH_SECOND2SECOND(deltatime + 1))
			deltatime++;
		assert(EPOCH_SECOND2SECOND(deltatime + 1) < EPOCH_SECOND2SECOND(deltatime + 2));
	} else
	{
		if (EPOCH_SECOND2SECOND(deltatime) == EPOCH_SECOND2SECOND(deltatime + 1))
			deltatime--;
		assert(EPOCH_SECOND2SECOND(deltatime) < EPOCH_SECOND2SECOND(deltatime + 1));
	}
	return deltatime;
}
#endif


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
			GET_TIME_STR(mur_options.before_time, time_str1);
			GET_TIME_STR(mur_options.since_time, time_str2);
			gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL1, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
		if (mur_options.lookback_time > mur_options.since_time)
		{
			GET_TIME_STR(mur_options.lookback_time, time_str1);
			GET_TIME_STR(mur_options.since_time, time_str2);
			gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL2, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
	} else
	{
		if (mur_options.before_time < min_bov_time)
		{
			GET_TIME_STR(mur_options.before_time, time_str1);
			GET_TIME_STR(min_bov_time, time_str2);
			gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL3, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
		if (mur_options.before_time < mur_options.after_time)
		{
			GET_TIME_STR(mur_options.before_time, time_str1);
			GET_TIME_STR(mur_options.after_time, time_str2);
			gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL4, 2, time_str1, time_str2);
			mupip_exit(ERR_MUNOACTION);
		}
	}
}
