/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	GBLREF  boolean_t		dollar_ztexit_bool;				\
	GBLREF  volatile boolean_t	dollar_zininterrupt;				\
											\
	error_def(ERR_JOBINTRRETHROW);				/* BYPASSOK */		\
											\
	if (dollar_ztexit_bool && !dollar_zininterrupt)				\
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBINTRRETHROW);		\
}
void jobinterrupt_event(int sig, siginfo_t *info, void *context);
void jobinterrupt_init(void);
void jobintrpt_ztime_process(boolean_t ztimeo);

#endif
