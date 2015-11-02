/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_user_name.h"

boolean_t trigger_user_name(char *trigger_value, int trigger_value_len)
{
	char		*ptr;

	ptr = strchr(trigger_value, TRIGNAME_SEQ_DELIM);
	return ((NULL == ptr) || ((trigger_value_len - 1) == (int)(ptr - trigger_value)));
}

