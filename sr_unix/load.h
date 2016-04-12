/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#define BADZCHSET		-1
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

void		bin_load(uint4 begin, uint4 end, char *line1_ptr, int line1_len);
void		go_call_db(int routine, char *parm1, int parm2, int val_off1, int val_len1);
int		go_get(char **in_ptr, int max_len, uint4 max_rec_size);
void		go_load(uint4 begin, uint4 end, unsigned char *recbuf, char *line3_ptr, int line3_len, uint4 max_rec_size, int fmt,
			int utf8_extract, int dos);
void		goq_load(void);
int		get_load_format(char **line1_ptr, char **line3_ptr, int *line1_len, int *line3_len, uint4 *max_rec_size,
			int *utf8_extract, int *dos);
boolean_t	gtm_regex_perf(const char *rexpr, char *str_buff);

#endif /* LOAD_INCLUDED */
