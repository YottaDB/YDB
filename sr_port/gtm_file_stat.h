/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_FILE_STAT_INCLUDED
#define GTM_FILE_STAT_INCLUDED

/* Note that FILE_READONLY also implies FILE_PRESENT and the callers can use this information if necessary */
#define FILE_NOT_FOUND 0
#define FILE_PRESENT 1
#define FILE_READONLY 2
#define FILE_STAT_ERROR 4

/* Copy the filename from "src" to "dest" using the following rules.
 * 	  (i) removing the version information
 * 	 (ii) doing compression of contiguous directory delimiters (']' or '>' followed by '<' or '[').
 * 	(iii) transform every '<' to a '[' and every '>' to a ']'
 * e.g.
 * 	src = user:[library.]<v990.pro>gtmshr.exe;23
 * 	dst = user:[library.v990.pro]gtmshr.exe
 *
 * Note that without (iii) we would have got the following mixed notation for "dst" which is incorrect
 * 	as the '[' is balanced by a '>' (at the end of ".pro>") instead of a corresponding ']'.
 *
 * 	e.g. src = user:[library.]<v990.pro>gtmshr.exe;23
 * 	     dst = user:[library.v990.pro>gtmshr.exe */
#ifdef VMS
#define fncpy_nover(src, src_len, dest, dest_len)							\
{													\
	unsigned char *sptr, *dptr;									\
	for (sptr = (unsigned char *)src, dptr = dest; sptr < (src + src_len) && (';' != *sptr); )	\
	{												\
		if (('>' == *sptr || ']' == *sptr) && ('<' == *(sptr + 1) || '[' == *(sptr + 1)))	\
		 	sptr += 2;									\
		else if ('<' == *sptr)									\
		{											\
			*dptr++ = '[';									\
			sptr++;										\
		} else if ('>' == *sptr)								\
		{											\
			*dptr++ = ']';									\
			sptr++;										\
		} else											\
			*dptr++ = *sptr++;								\
	}												\
	dest_len = dptr - (unsigned char *)(dest);							\
	*(dptr) = 0;											\
}
#endif

int gtm_file_stat(mstr *file, mstr *def, mstr *ret, boolean_t check_prv, uint4 *status);

#endif /* GTM_FILE_STAT_INCLUDED */
