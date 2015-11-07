/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <climsgdef.h>
#include <descrip.h>
#include <ssdef.h>

#include "timers.h"

void change_fhead_timer(char *timer_name, sm_int_ptr_t timer_address, int default_time, bool zero_is_ok)
/* default_time is in milliseconds */
{
	uint4 old_value[2];
	int i;
	short len;
	int4 status;
	char *cp;
	unsigned char time_buff[63];
	struct dsc$descriptor_s dsc, timd;

	error_def(ERR_TIMRBADVAL);

	timd.dsc$b_dtype = DSC$K_DTYPE_T;
	timd.dsc$b_class = DSC$K_CLASS_S;
	timd.dsc$a_pointer = time_buff;
	timd.dsc$w_length = SIZEOF(time_buff);
	dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	dsc.dsc$b_class = DSC$K_CLASS_S;
	dsc.dsc$a_pointer = timer_name;
	for (i = 0 , cp = timer_name ; *cp && i < SIZEOF(time_buff) ; cp++, i++)
	    ;
	dsc.dsc$w_length = i;
	status = cli$present(&dsc);
	if (CLI$_NEGATED == status)
	{
		if (TRUE == zero_is_ok)
			timer_address[0] = timer_address[1] = 0;
		else
		{
			timer_address[0] = default_time * TIMER_SCALE;
	    		timer_address[1] = -1;
		}
	} else if (CLI$_PRESENT == status)
	{
		status = cli$get_value(&dsc, &timd, &len);
		if (SS$_NORMAL == status)
		{
			old_value[0] = timer_address[0];
			old_value[1] = timer_address[1];
			timd.dsc$w_length = len;
			status = sys$bintim(&timd, timer_address);
			if ((!zero_is_ok && timer_address[0] == 0) || timer_address[1] < -9 ||
				(timer_address[1] == -9 && timer_address[0] < -1640161632))		/* < 1 hour */
			{
				timer_address[0] = old_value[0];
				timer_address[1] = old_value[1];
				rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
			}
			assert(status & 1);
		}
		else
			rts_error(VARLSTCNT(1) ERR_TIMRBADVAL);
	}
	return;
}
