/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
	bool	asterisk_not_seen=TRUE;
	int	pat_counter=0,count=0,jnl_counter=0,i,j=0,k=0,p,q,index1,count1=0,index2;
	int     sav_jnl=0,pcount=0,jcount=0;

	/* correcting the length inconsistencies in padded arrays from $getjpi */
	for (q = jnl_len - 1; q > 0; q--)
	{
		if ( (*(jnl_str + q) == ' ') || (*(jnl_str + q ) == 0))
			jnl_len-- ;
		else
			break;
	}
	while ((jnl_counter < jnl_len) && (pat_counter < pat_len))  /* main loop */
	{
		while (( *(pat_str + pat_counter) != '*') && ( *(pat_str + pat_counter) != '%') &&
			(jnl_counter < jnl_len) && (pat_counter < pat_len))
		{
			asterisk_not_seen = TRUE;
			if ( *(jnl_str + jnl_counter) != *(pat_str + pat_counter) )
				return(FALSE);  /* characters do not match */
			else  /* go to next char */
			{
				jnl_counter++;
				pat_counter++;
			}
		}
		/* break out of loop if wildcard seen */
		if (( *(pat_str + pat_counter) == '%') && (pat_counter < pat_len) && (jnl_counter < jnl_len))
		{	/* simple case of percent: increment pointers and continue */
			jnl_counter++;
			pat_counter++;
		} else if ( *(pat_str + pat_counter) == '*') /* gets rough ,fasten seat belts */
		{
			pat_counter++;
			i = pat_counter;
			while ((asterisk_not_seen) && (i < pat_len)) /* find the next occurrence of asterisk to memcmp */
			{
				if (*(pat_str + i) == '*')
					asterisk_not_seen= FALSE;
				else
					i++;
			}
			if (i == pat_len)  /* no asterisk found after the current one */
			{
				if (!memcmp(jnl_str + (jnl_len - (i - pat_counter)),pat_str + pat_counter,i - pat_counter))
					return(TRUE);
				else    /* maybe they do not match or else it contains percent character */
				{
					index1 = i - pat_counter;
					count = pat_counter;
					while (count1 < index1)
					{
						if (( *(pat_str + count) == '%') ||
						    ( *(pat_str + count) == *(jnl_str + (jnl_len - index1) + count1 )))
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
					if (pat_counter == pat_len)
						return(TRUE);
					else
						return(FALSE);
				}
			} else if (i < pat_len)	/* another asterisk seen before end of string */
			{
				sav_jnl = jnl_counter;
				while (memcmp(jnl_str + jnl_counter, pat_str + pat_counter, i - pat_counter)
						&& (jnl_counter < jnl_len))
					jnl_counter++;
				if (jnl_counter == jnl_len)
				{
					jcount = i - pat_counter;
					index2 = pat_counter;
					while (index2 <= (jnl_len - jcount)+1)
					{
						if (( *(pat_str + pat_counter) == '%') ||
						    ( *(pat_str + pat_counter) ==  *(jnl_str + sav_jnl)))
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
					if (jcount == pcount)
						return(TRUE);
					else
						return(FALSE);
				}
				/* synchronize the character pointers after processing an asterisk */
				if (i < (pat_len-1))
					pat_counter = i + 1;
				while ((* (jnl_str + jnl_counter) != *(pat_str + pat_counter)) &&
						(jnl_counter < jnl_len) && (*(pat_str + pat_counter) != '%'))
					jnl_counter++;
				if (jnl_counter == jnl_len) /* if unable to synchronize */
					return(FALSE);
			}
		}
	}
	if ((jnl_counter == jnl_len) && (pat_counter == pat_len))
		return TRUE;
	else
		return FALSE;
}
