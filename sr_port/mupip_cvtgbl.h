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

#ifndef MUPIP_CVTGBL_INCLUDED
#define MUPIP_CVTGBL_INCLUDED

void mupip_cvtgbl(void);
#ifdef UNIX
int get_file_format(char **line1_ptr, char **line2_ptr, int *line1_len, int *line2_len);
#endif
#endif /* MUPIP_CVTGBL_INCLUDED */
