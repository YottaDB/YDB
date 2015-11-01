/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBINTR_PROCESS_INCLUDED
#define JOBINTR_PROCESS_INCLUDED

#define JOBINTR_TP_RETHROW								\
{ /* rethrow job interrupt($ZINT) if $ZTEXIT is true and not already in $ZINTR */	\
	GBLREF  boolean_t		dollar_zininterrupt;				\
	GBLREF  boolean_t		dollar_ztexit_bool;				\
	error_def(ERR_JOBINTRRETHROW);							\
											\
	if (dollar_ztexit_bool && !dollar_zininterrupt)				\
		rts_error(VARLSTCNT(1) ERR_JOBINTRRETHROW);				\
}

void jobinterrupt_process(void);

#endif
