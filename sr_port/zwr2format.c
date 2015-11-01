/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_ctype.h"

#include "zshow.h"
#include "patcode.h"

#define MAX_DIGIT_NUM 4

boolean_t zwr2format(mstr *src, mstr *des)
{

        unsigned char *cp, *end;
	int ch, counter, fastate = 0, num;
	int digit;	/* declared to be int to match the prototype of isdigit() */
	char *tmp;
	mstr one;
	char buff[MAX_DIGIT_NUM];

	des->len = 0;
	if (src->len > 0)
	{
	        cp = (unsigned char *)src->addr;
	        end = cp + src->len;
		if ((ch = *cp) == '"')
		{
		        cp++;
		        fastate = 0;
		}
		else if ('$' == ch)
		{
		        if('C' != *(cp + 1))
				return FALSE;
			if('(' != *(cp + 2))
				return FALSE;
			cp += 3;
			fastate = 1;
	        }
		else
		        fastate = 0;

		for(; cp < end; cp++)
		{
		        ch = *cp;
			switch(fastate)
			{
			case 0:
			        if('"' == ch)
				{
				        if (cp < end - 1) /* not the last one */
					{
					        switch(*(cp + 1))
						{
						case '"':
						        *(des->addr + des->len) = ch;
							des->len++;
							cp++;
							break;
						case '_':
							if('$' != *(cp + 2))
							        return FALSE;
							if(('C' != *(cp + 3)) && ('c' != *(cp + 3)))
							        return FALSE;
							if('(' != *(cp + 4))
							        return FALSE;
							cp += 4;
							fastate = 1;
							break;
						default:
							return FALSE;
							break;
						}
					}
				}
				else
				{
				        *(des->addr + des->len) = ch;
					des->len++;
				}
				break;
			case 1:
				for (counter = 0, digit = TRUE; digit && (counter < MAX_DIGIT_NUM);)
				{
				        digit = ISDIGIT(ch = *cp++);
					buff[counter++] = ch;
				}
				buff[--counter] = 0;
				if ((0 == counter) || (MAX_DIGIT_NUM <= counter) || digit)
					return FALSE;
				*(des->addr + des->len) = num = ATOI(buff);
				des->len++;
				switch(ch)
				{
				case ')':
				        if (cp < end) /* not end */
					{
					        if('_' != *cp)
						        return FALSE;
						if('"' == *(cp + 1))
						{
							fastate = 0;
							cp++;
						}
						else if ('$' == *(cp + 1))
						{
							if (('C' != *(cp + 2)) && ('c' != *(cp + 2)))
								return FALSE;
							if ('(' != *(cp + 3))
								return FALSE;
							cp += 3;
						}
						else
							return FALSE;
					}
					else
					{
					        if (end != cp)
						        return FALSE;
					}
					break;
				case ',':
					cp--;
					break;
			        default:
					return FALSE;
					break;
				}
				break;
			default:
				return FALSE;
				break;
			}
		}
	}
	return TRUE;
}
