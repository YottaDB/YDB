/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IORMDEFSP_H
#define IORMDEFSP_H

#define EBCDIC_RMEOL "\25"	/*	#pragma(suspend) not working in macros	*/
#define ASCII_RMEOL "\n"

#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
#define RMEOL ((ascii != iod->out_code_set) ? EBCDIC_RMEOL : ASCII_RMEOL )
#else
#define RMEOL ASCII_RMEOL
#endif

#endif /* IORMDEFSP_H */
