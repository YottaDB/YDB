/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
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

#include "gdsroot.h"
#include "gdsblk.h"
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

boolean_t	mur_do_wildcard(char *jnl_str, char *pat_str, int jnl_len, int pat_len)
{
	bool	*escaped_chars, asterisk_not_seen=TRUE;
	int	pat_counter=0, pat_str_escaped_counter=0, count=0, jnl_counter=0, i, j=0, k=0, p, q, index1, count1=0, index2;
	int     sav_jnl=0, pcount=0, jcount=0, escaped_chars_length=0, pat_str_escaped_len=0;
	char	*pat_str_escaped;

	escaped_chars = (bool *)malloc(pat_len * SIZEOF(bool));
	pat_str_escaped = (char *)malloc((pat_len + 1) * SIZEOF(char));
	/* Adjust the pattern string, to remove the escape chars and make the array of indices to be escaped */
	memset(escaped_chars, 0, pat_len * SIZEOF(bool)); /* Set boolean array to all false */
	index1 = index2 = 0;
	while (index1 < pat_len)
	{
		if (*(pat_str + index1) == '\\')
		{
			escaped_chars[index2] = TRUE;
			index1++;
			if (index1 >= pat_len)
				break;
		}
		pat_str_escaped[index2++] = pat_str[index1++];
	}
	assert(index2 <= index1);
	pat_str_escaped_len = index2;
	pat_str_escaped[pat_str_escaped_len] = '\0';
	/* Use the new pattern string */
	while ((jnl_counter < jnl_len) && (pat_counter < pat_str_escaped_len))  /* main loop */
	{
		while ((jnl_counter < jnl_len) && (pat_counter < pat_str_escaped_len) &&
				(escaped_chars[pat_counter] ||
					 ((*(pat_str_escaped + pat_counter) != '*') && (*(pat_str_escaped + pat_counter) != '%'))))
		{
			asterisk_not_seen = TRUE;
			if ( *(jnl_str + jnl_counter) != *(pat_str_escaped + pat_counter) )
			{
				free(escaped_chars);
				free(pat_str_escaped);
				return(FALSE);  /* characters do not match */
			}
			else  /* go to next char */
			{
				jnl_counter++;
				pat_counter++;
			}
		}
		/* break out of loop if wildcard seen */
		if ((pat_counter < pat_str_escaped_len) && (jnl_counter < jnl_len) && (*(pat_str_escaped + pat_counter) == '%'))
		{	/* simple case of percent: increment pointers and continue */
			jnl_counter++;
			pat_counter++;
		} else if ((pat_counter < pat_str_escaped_len) && (*(pat_str_escaped + pat_counter) == '*'))
		{	/* gets rough ,fasten seat belts */
			pat_counter++;
			i = pat_counter;
			while (asterisk_not_seen && (i < pat_str_escaped_len)) /* find the next occurrence of asterisk to memcmp */
			{
				if (!escaped_chars[i] && (*(pat_str_escaped + i) == '*'))
					asterisk_not_seen = FALSE;
				else
					i++;
			}
			if (i == pat_str_escaped_len)  /* no asterisk found after the current one */
			{
				if ((i - pat_counter) > jnl_len)
				{
					free(escaped_chars);
					free(pat_str_escaped);
					return(FALSE);
				}
				if (!memcmp(jnl_str + (jnl_len - (i-pat_counter)), pat_str_escaped + pat_counter, i - pat_counter))
				{
					free(escaped_chars);
					free(pat_str_escaped);
					return(TRUE);
				}
				else    /* maybe they do not match or else it contains percent character */
				{
					index1 = i - pat_counter;
					count = pat_counter;
					while (count1 < index1)
					{
						if ((!escaped_chars[count] && ( *(pat_str_escaped + count) == '%')) ||
						    ( *(pat_str_escaped + count) == *(jnl_str + (jnl_len - index1) + count1 )))
						{
							pat_counter++;
							count++;
							count1++;
						} else
						{
							count1++;
							count++;
						}
					}
					free(escaped_chars);
					free(pat_str_escaped);
					if (pat_counter == pat_str_escaped_len)
						return(TRUE);
					else
						return(FALSE);
				}
			} else if (i < pat_str_escaped_len)	/* another asterisk seen before end of string */
			{
				sav_jnl = jnl_counter;
				while ((jnl_counter < jnl_len)
						&& memcmp(jnl_str + jnl_counter, pat_str_escaped + pat_counter, i - pat_counter))
					jnl_counter++;
				if (jnl_counter == jnl_len)
				{
					jcount = i - pat_counter;
					index2 = pat_counter;
					while (index2 <= (jnl_len - jcount) + 1)
					{
						if ((!escaped_chars[pat_counter] && ( *(pat_str_escaped + pat_counter) == '%')) ||
						    ( *(pat_str_escaped + pat_counter) ==  *(jnl_str + sav_jnl)))
						{
							pat_counter++;
							sav_jnl++;
							pcount++;
							if (pcount == jcount)
								break;
						} else
						{
							sav_jnl++;
							index2++;
						}
					}
					free(escaped_chars);
					free(pat_str_escaped);
					if (jcount == pcount)
						return(TRUE);
					else
						return(FALSE);
				}
				/* synchronize the character pointers after processing an asterisk */
				if (i < (pat_str_escaped_len - 1))
					pat_counter = i + 1;
				while ((jnl_counter < jnl_len)
						&& (*(jnl_str + jnl_counter) != *(pat_str_escaped + pat_counter))
						&& ((escaped_chars[pat_counter] || *(pat_str_escaped + pat_counter) != '%')))
					jnl_counter++;
				if (jnl_counter == jnl_len) /* if unable to synchronize */
				{
					free(escaped_chars);
					free(pat_str_escaped);
					return(FALSE);
				}
			}
		}
	}
	free(escaped_chars);
	free(pat_str_escaped);
	if ((jnl_counter == jnl_len) && (pat_counter == pat_str_escaped_len))
		return TRUE;
	else
		return FALSE;
}
