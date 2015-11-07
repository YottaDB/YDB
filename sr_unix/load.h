/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LOAD_INCLUDED
#define LOAD_INCLUDED

#define ONERROR_STOP		0
#define ONERROR_PROCEED		1
#define ONERROR_INTERACTIVE	2
#define ONERROR_PROCESS														\
{																\
	GBLREF int onerror;													\
																\
	if (ONERROR_STOP == onerror)												\
	{															\
		break;														\
	}															\
	if (ONERROR_INTERACTIVE == onerror && !mu_interactive("Load terminated by operator\n"))					\
	{															\
		onerror = ONERROR_STOP;												\
		/* User selected Not to proceed */										\
		break;														\
	}															\
	mupip_error_occurred = FALSE;												\
	continue; /* continue, when (onerror = ONERROR_PROCEED) or when user selects Yes in ONERROR_INTERACTIVE */		\
}

void bin_load(uint4 begin, uint4 end, char *line1_ptr, int line1_len);
void go_load(uint4 begin, uint4 end, char *line1_ptr, int line1_len, char *line2_ptr, int line2_len);
void goq_load(void);

#endif /* LOAD_INCLUDED */
