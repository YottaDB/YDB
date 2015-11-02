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

#ifndef STRPIECEDIFF_INCLUDED
#define STRPIECEDIFF_INCLUDED

void	strpiecediff(mstr *oldstr, mstr *newstr, mstr *delim,
			uint4 numpieces, gtm_num_range_t *piecearray, boolean_t use_matchc, mstr *pcdiff_mstr);

#endif
