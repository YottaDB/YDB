/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  Should change bool to boolean_t together on Unix and VMS
 */

#define	OUTOFBAND_MSK 0x02000008
#define CTRLC_MSK 0x00000008
#define CTRLY_MSK 0x02000000
#define CTRLC     3
#define CTRLY	  25
#define MAXOUTOFBAND 31

enum outofbands
{
	ctrly = 1,
	ctrlc,
	ctrap,
	tptimeout,
	jobinterrupt
};

/* ------------------------------------------------------------------
 *  Old-style declarations (and uses) of this function abound.
 * Be sure to change all declarations at once!
 * ------------------------------------------------------------------
 */
void outofband_action(bool);

void outofband_clear(void);

