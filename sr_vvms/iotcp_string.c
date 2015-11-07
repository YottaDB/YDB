/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ZY: self-defined string operation routines
 *     since string routines cause multiply-defined problem on VMS
 */

#include "mdef.h"     /* 96/12/30 smw for memcmp */

char *vmssp_strcpy(char *s1, char *s2)
{
	char *p, *q;
	p = s1;
	q = s2;
	for (; *q; ++q)
		*p++ = *q;
	*p = '\0';
	return(s1);
}


char *vmssp_strcat(char *s1, char *s2)
{
	char  *p, *q;
	for (p=s1; *p; ++p);
	for (q=s2; *q; ++q)
		*p++ = *q;
	*p = '\0';
	return(s1);
}


int vmssp_sprintf(char *s1, char *format, char *sour, unsigned short num)
{
	char correct_format[10];
	char *str, *p, q[10];
	int  t, i, j;

	vmssp_strcpy(correct_format, "%s,%d");
	if (memcmp(format, correct_format, 6))
		/* How to print error by GTM staff?
		   printf("Error: sprintf format is not correct!\n");
		*/
		return(-1);

	str = s1;
	for (p=sour,i=0; *p; p++,i++)
		*str++ = *p;
	*str++ = ',';
	i++;
	for(t=num,j=0; t>9; )
	{
		q[j++] = (t % 10) + '0';
		t = (int)t/10;
	}
	q[j] = t + '0';
	for (; j>=0 ; --j, ++i)
		*str++ = q[j];
	*str = '\0';
	return(i);
}


int vmssp_sscanf(char *str, char *format, char *rel, short *num)
{
	char correct_format[10];
	char *p;
	int  t;

	if (!(*str))      /* empty string, unix returns EOF */
		return(-1);

	vmssp_strcpy(correct_format, "%[^,],%hu");
	if (memcmp(format, correct_format, 10))
		/* How to print error by GTM staff?
		   printf("Error: sscanf format is not correct!\n");
		*/
		return(-1);

	for (p=str; *p && *p!=','; p++)
		*rel++ = *p;
	*rel = '\0';
	if (!(*p))
		/* no , match */
		return(-1);
	else
	{
		for (p++, *num=0; *p; p++)
		{
			t = *p - '0';
			*num = *num * 10 + t;
		}
		if (*str == ',')
			return(1);
		else
			return(2);
	}
}

