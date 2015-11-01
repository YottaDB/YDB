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

#ifndef SETZDIR_INCLUDED
#define SETZDIR_INCLUDED

void setzdir(mval *newdir, mval *full_path_of_newdir);

#if defined(UNIX)

#define SKIP_DEVICE(dir)
#define SKIP_DEVICE_IF_NOT_NEEDED(dir)

#elif defined(VMS)

#define SKIP_DEVICE(dir) /* skip DEVICE part in dir */						\
{												\
	char	*dir_ptr;									\
												\
	for (dir_ptr = (dir)->str.addr; dir_ptr < (dir)->str.addr + (dir)->str.len; dir_ptr++)	\
	{ /* find location of device delimiter */						\
		if (':' == *dir_ptr)								\
			break;									\
	}											\
	assert(dir_ptr > (dir)->str.addr &&  /* must have DEVICE + DIRECTORY */ 		\
	       dir_ptr < (dir)->str.addr + (dir)->str.len);					\
	(dir)->str.len -= (++dir_ptr - (dir)->str.addr);					\
	(dir)->str.addr = dir_ptr;								\
}

#define SKIP_DEVICE_IF_NOT_NEEDED(dir) 		\
	if (ZDIR_FORM_DIRECTORY == zdir_form) 	\
		SKIP_DEVICE(dir);

#else
#error UNSUPPORTED PLATFORM
#endif

#endif /* SETZDIR_INCLUDED */
