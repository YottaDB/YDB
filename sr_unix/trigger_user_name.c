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

#include "gtm_ctype.h"
#include "gtm_string.h"
#include <rtnhdr.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_user_name.h"

#define NOLENGTH  -1

/* A quick heuristic to determine if the trigger name is user defined or auto
 * generated. Detect an interior (not at the end) # sign in the subject string
 * This is intended for use on valid trigger names, do not use this for
 * validation */
boolean_t trigger_user_name(char *trigger_value, int trigger_value_len)
{
	char		*ptr;

	ptr = memchr(trigger_value, TRIGNAME_SEQ_DELIM, trigger_value_len);
	return ((NULL == ptr) || ((trigger_value_len - 1) == (int)(ptr - trigger_value)));
}

/* Function returns the length of the trigger name or the character position of
 * the failure. There are two valid types of names here defined by the following
 * PATCODES
 * - Delete by user defined name:	  ?1(1"%",1A).27(1A,1N).1(1"#",1"*")
 * - Delete by auto generated name:	  ?1(1"%",1A).20(1A,1N)1"#"1(1.6N.1"#",1"*")
 * Keep in mind that this function does not have any side-effects and does not strip off
 * a trailing # sign or wild card
 */
int validate_input_trigger_name(char *trigger_name, uint4 trigger_name_len, boolean_t *wildcard_ptr)
{
	char		*ptr, *tail;
	uint4		len, name_len, num_len, max_len;
	boolean_t	wild, poundtail;

	if (0 == trigger_name_len)
		/* reject zero lengths, use -1 because returning 0 for 0 won't signal an error */
		return NOLENGTH;
	name_len = num_len = 0;
	ptr = trigger_name;
	len = trigger_name_len;
	tail = ptr + (len - 1);
	assert (ptr >= trigger_name);
	if (MAX_MIDENT_LEN < trigger_name_len)
		/* reject strings with super long lengths */
		return MAX_USER_TRIGNAME_LEN;
	if (!ISALPHA_ASCII(*ptr) && ('%' != *ptr))
		/* first char must be alpha or '%' sign */
		return INTCAST(ptr - trigger_name);
	if ('*' == *tail)
	{ /* strip the wild card to skip checking it */
		wild = TRUE;
		tail--;
		len--;
		name_len++;
	} else
		wild = FALSE;
	if (wildcard_ptr)
		*wildcard_ptr = wild;
	if (tail == ptr)
		/* special case to return sooner for a single character name */
		return INTCAST(ptr - trigger_name) + 1 + ((wild) ? 1 : 0);
	if (trigger_user_name(trigger_name, trigger_name_len))
		/* user defined name, use MAX_USER_TRIGNAME_LEN as max_len */
		max_len = MAX_USER_TRIGNAME_LEN;
	else
		/* auto generated name, use MAX_AUTO_TRIGNAME_LEN as max_len */
		max_len = MAX_AUTO_TRIGNAME_LEN;
	poundtail = (TRIGNAME_SEQ_DELIM == *tail);
	if (MAX_USER_TRIGNAME_LEN + ((poundtail) ? 1 : 0) < trigger_name_len)
		/* name, must be under 28 chars (MAX_USER_TRIGNAME_LEN), but increment
		 * by one to forgive a trailing # sign, the 29th char */
		return max_len;
	while (++ptr <= tail && TRIGNAME_SEQ_DELIM != *ptr)
	{
		if ((!ISALNUM_ASCII(*ptr)) || (max_len < ++name_len))
			/* reject non-ALPHA-NUMERICS until first # sign or string end */
			return INTCAST(ptr - trigger_name);
		if (ptr == tail)
			break;
	}
	assert (ptr >= trigger_name);
	if (tail <= ptr)
		/* if the above loop terminated on this we're done, add in the wild card as necessary */
		return INTCAST(ptr - trigger_name) + 1 + ((wild) ? 1 : 0);
	if (wild)
		/* reject anything between the first # sign and wild card */
		return INTCAST(ptr - trigger_name);
	while (++ptr <= tail && TRIGNAME_SEQ_DELIM != *ptr)
	{ /* validate the numeric portion of the auto generated name */
		if ((!ISDIGIT_ASCII(*ptr)) || (NUM_TRIGNAME_SEQ_CHARS < ++num_len))
			/* reject non-numeric or reject aaa#1234567 */
			return INTCAST(ptr - trigger_name);
		if (ptr == tail)
			break;
	}
	if (0 == num_len)
		/* reject aaa##  aka no numbers between adjacent '#' signs */
		return INTCAST(ptr - trigger_name);
	assert (ptr >= trigger_name);
	/* anything after the second # sign, then (ptr - trigger_name) + 1 != trigger_name_len */
	return INTCAST(ptr - trigger_name) + 1;
}

