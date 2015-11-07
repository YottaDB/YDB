/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Lock status block declaration.  */
/* WARNING: be certain any changes in this structure declaration are reflected in gtm_enq and gtm_enqw.  */
typedef struct {
	short	cond;
	short	reserved;
	uint4	lockid;
	uint4	valblk[4];
} lock_sb;

#define MAX_VMS_LOCKS 1024

uint4 gtm_deq(unsigned int lkid, void *valblk, unsigned int acmode, unsigned int flags);
uint4 gtm_enq(unsigned int efn, unsigned int lkmode, lock_sb *lsb, unsigned int flags,
	void *resnam, unsigned int parid, void *astadr, unsigned int astprm,
	void *blkast, unsigned int acmode, unsigned int nullarg);
uint4 gtm_enqw(unsigned int efn, unsigned int lkmode, lock_sb *lsb, unsigned int flags,
	void *resnam, unsigned int parid, void *astadr, unsigned int astprm, void *blkast,
	unsigned int acmode, unsigned int nullarg);
uint4 ccp_enq(unsigned int efn, unsigned int lkmode, lock_sb *lksb, unsigned int flags,
	void *resnam, unsigned int parid, void *astadr, unsigned int astprm,
	void *blkast, unsigned int acmode, unsigned int nullarg);
uint4 ccp_enqw(unsigned int efn, unsigned int lkmode, lock_sb *lksb, unsigned int flags,
	void *resnam, unsigned int parid, void *astadr, unsigned int astprm,
	void *blkast, unsigned int acmode, unsigned int nullarg);
