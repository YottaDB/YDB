/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDS_RUNDOWN_INCLUDED
#define GDS_RUNDOWN_INCLUDED

#ifdef UNIX
int4 gds_rundown(void);

/* A multiplicative factor to the # of processors used in determining if a GT.M process in gds_rundown can bypass semaphores */
#ifdef DEBUG
#	define PROC_FACTOR	2
#else
#	define PROC_FACTOR	20
#endif

#else
void gds_rundown(void);
#endif

#define CAN_BYPASS(SEMVAL, CANCELLED_TIMER, INST_IS_FROZEN)									   \
	(((IS_GTM_IMAGE && csd->mumps_can_bypass) && !CANCELLED_TIMER && (PROC_FACTOR * (num_additional_processors + 1) < SEMVAL)) \
	|| ((2 < SEMVAL) && (IS_LKE_IMAGE || IS_DSE_IMAGE)) || INST_IS_FROZEN)


#endif /* GDS_RUNDOWN_INCLUDED */
