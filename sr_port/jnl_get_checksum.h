/****************************************************************
 *
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef __JNL_GET_CHECKSUM_H_
#define __JNL_GET_CHECKSUM_H_

#define INIT_CHECKSUM_SEED 1
#define CHKSUM_SEGLEN4 8

#define ADJUST_CHECKSUM(sum, num4)  (((sum) >> 4) + ((sum) << 4) + (num4))

uint4 jnl_get_checksum(uint4 checksum, uint4 *buff, int bufflen);
uint4 jnl_get_checksum_entire(uint4 checksum, uint4 *buff, int bufflen);

#endif
