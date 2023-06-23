/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

struct rc_oflow {
    int4	 page;
    char	*buff;
    short	 top;
    short	 size;
    short	 dsid;
    short	 offset;
    char	 zcode;
};
typedef struct rc_oflow	 rc_oflow;

/* DT block header is 24 bytes, need to leave space */
#define RC_BLKHD_PAD (24 - SIZEOF(blk_hdr))
/* DT has 6 byte max key at end, need to leave space */
#define RC_MAXKEY_PAD 6
/* DT puts in a fake first key */
#define RC_FIRST_PAD 6
/* In large blocks, leave extra space */
#define RC_BLK_PAD(x) ((x >= 2048) ? 128 : RC_BLKHD_PAD + RC_MAXKEY_PAD + RC_FIRST_PAD)
